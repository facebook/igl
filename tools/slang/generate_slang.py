#!/usr/bin/env fbpython
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Compiles Slang programs into C++ headers holding IGL shader code for every backend.

slangc compiles each program listed in a manifest to SPIR-V (Vulkan), MSL (Metal), HLSL (D3D12)
and WGSL (WebGPU); SPIRV-Cross turns the SPIR-V into GLSL 410/430 and GLSL ES 300/310 (OpenGL).
Programs declare resources through igl_abi.slangh, and the linter rejects anything that would bind
differently on some backend. Each program becomes generated/<Name>Slang.h next to the manifest,
read through IGLU/slang/SlangProgram.h.

usage: generate_slang.py --manifest programs.json --slangc PATH --spirv-cross PATH
                         [--spirv-val PATH] [--dxc PATH] [--wgsl-validate PATH] [--metal]
                         [--program NAME]... [--check]

A manifest:
  {"namespace": "igl::tests::slang",
   "programs": [{"name": "Quad", "source": "Quad.slang", "profile": "portable"},
                {"name": "Materials", "source": "Materials.slang", "profile": "highend",
                 "conformances": ["LambertMaterial:IMaterial=0"]}]}

Profiles: "portable" targets every backend; "highend" targets Vulkan and Metal only.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field

ABI_DIR = os.path.dirname(os.path.abspath(__file__))
# Command that regenerates the headers in place; recorded in each header when set.
REGENERATE = ""
# @fb-only
# MSVC rejects a string literal longer than 16380 bytes.
CHUNK_CHARS = 12000

IGL_STAGES = {
    "vertex": "Vertex",
    "fragment": "Fragment",
    "compute": "Compute",
    "amplification": "Task",
    "mesh": "Mesh",
}
# Vertex attribute names by semantic; the D3D12 backend derives semantics from these names.
ATTRIBUTE_NAMES = {
    "POSITION": "position",
    "TEXCOORD": "texcoord",
    "COLOR": "color",
    "NORMAL": "normal",
    "TANGENT": "tangent",
}


@dataclass(frozen=True)
class Profile:
    spirv: str
    spirv_env: str
    targets: frozenset[str]


PROFILES = {
    "portable": Profile(
        "spirv_1_3", "vulkan1.1", frozenset({"spirv", "gl", "hlsl", "metal", "wgsl"})
    ),
    "highend": Profile("spirv_1_5", "vulkan1.2", frozenset({"spirv", "metal"})),
}


@dataclass
class Tools:
    slangc: str
    spirv_cross: str
    spirv_val: str | None
    dxc: str | None
    wgsl_validate: str | None
    metal: bool


@dataclass
class ProgramSpec:
    name: str
    source: str
    profile: Profile
    include_dirs: list[str]
    conformances: list[str]


@dataclass
class GlResource:
    kind: str  # UniformBlock, StorageBlock, Sampler
    index: int
    name: str
    instance_name: str = ""
    entry_points: set[int] = field(default_factory=set)


@dataclass
class Entry:
    name: str
    stage: str
    workgroup_size: list[int]
    spirv: list[int] = field(default_factory=list)
    glsl: str = ""
    essl: str = ""
    hlsl: str = ""


@dataclass
class Artifacts:
    entries: list[Entry]
    msl: str = ""
    wgsl: str = ""
    gl_resources: list[GlResource] = field(default_factory=list)
    attributes: list[tuple[int, str]] = field(default_factory=list)
    slangc_version: str = ""


class GeneratorError(Exception):
    pass


def run(cmd: list[str], what: str, with_stderr: bool = False) -> str:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise GeneratorError(
            f"{what} failed:\n  {' '.join(cmd)}\n{result.stdout}{result.stderr}"
        )
    return result.stdout + result.stderr if with_stderr else result.stdout


def read_text(path: str) -> str:
    with open(path, encoding="utf-8") as f:
        return f.read()


def read_json(path: str) -> dict:
    with open(path, encoding="utf-8") as f:
        return json.load(f)


class Compiler:
    def __init__(self, tools: Tools, spec: ProgramSpec, tmp: str) -> None:
        self.tools = tools
        self.spec = spec
        self.tmp = tmp

    def slang(self, define: str, args: list[str]) -> list[str]:
        cmd = [self.tools.slangc, self.spec.source, "-I", ABI_DIR]
        for include in self.spec.include_dirs:
            cmd += ["-I", include]
        cmd += [
            "-D",
            define,
            "-matrix-layout-row-major",
            "-line-directive-mode",
            "none",
            "-O1",
        ]
        for conformance in self.spec.conformances:
            cmd += ["-conformance", conformance]
        return cmd + args

    def path(self, name: str) -> str:
        return os.path.join(self.tmp, name)

    def whole_module(self, target: str, define: str, ext: str) -> tuple[str, dict]:
        out = self.path(f"{self.spec.name}.{ext}")
        reflection = self.path(f"{self.spec.name}.{ext}.json")
        run(
            self.slang(
                define, ["-target", target, "-o", out, "-reflection-json", reflection]
            ),
            f"slangc -target {target}",
        )
        return read_text(out), read_json(reflection)

    def spirv(self, entry: Entry) -> tuple[str, list[int], dict]:
        out = self.path(f"{entry.name}.spv")
        reflection = self.path(f"{entry.name}.spv.json")
        run(
            self.slang(
                "IGL_TARGET_SPIRV",
                [
                    "-target",
                    "spirv",
                    "-profile",
                    self.spec.profile.spirv,
                    "-entry",
                    entry.name,
                    "-stage",
                    entry.stage,
                    "-o",
                    out,
                    "-reflection-json",
                    reflection,
                ],
            ),
            f"slangc -target spirv -entry {entry.name}",
        )
        if self.tools.spirv_val:
            run(
                [
                    self.tools.spirv_val,
                    "--target-env",
                    self.spec.profile.spirv_env,
                    out,
                ],
                f"spirv-val {entry.name}",
            )
        with open(out, "rb") as f:
            data = f.read()
        words = [
            int.from_bytes(data[i : i + 4], "little") for i in range(0, len(data), 4)
        ]
        return out, words, read_json(reflection)

    def hlsl(self, entry: Entry) -> str:
        out = self.path(f"{entry.name}.hlsl")
        run(
            self.slang(
                "IGL_TARGET_HLSL",
                [
                    "-target",
                    "hlsl",
                    "-profile",
                    "sm_6_0",
                    "-entry",
                    entry.name,
                    "-stage",
                    entry.stage,
                    "-o",
                    out,
                ],
            ),
            f"slangc -target hlsl -entry {entry.name}",
        )
        if self.tools.dxc:
            profile = {"vertex": "vs_6_0", "fragment": "ps_6_0", "compute": "cs_6_0"}[
                entry.stage
            ]
            run(
                [
                    self.tools.dxc,
                    "-T",
                    profile,
                    "-E",
                    entry.name,
                    out,
                    "-Fo",
                    out + ".dxil",
                ],
                f"dxc {entry.name}",
            )
        return read_text(out)

    def gl(
        self, entry: Entry, spv: str, vertex_semantics: dict[int, str]
    ) -> tuple[str, str, str]:
        """Returns GLSL, GLSL ES, and GLSL 450 that keeps layout(binding) to read names from."""
        reflection = json.loads(
            run(
                [self.tools.spirv_cross, spv, "--reflect"],
                f"spirv-cross --reflect {entry.name}",
            )
        )
        renames: list[str] = []
        for io, key in (("in", "inputs"), ("out", "outputs")):
            for variable in reflection.get(key, []):
                location = variable.get("location")
                if location is None:
                    continue
                if entry.stage == "vertex" and io == "in":
                    new_name = vertex_semantics[location]
                elif entry.stage == "fragment" and io == "out":
                    new_name = f"fragColor{location}"
                else:
                    new_name = f"v{location}"
                renames += ["--rename-interface-variable", io, str(location), new_name]
        if entry.stage == "compute":
            versions = (["--version", "430", "--no-es"], ["--version", "310", "--es"])
        else:
            versions = (
                ["--version", "410", "--no-es", "--no-420pack-extension"],
                ["--version", "300", "--es"],
            )
        versions += (["--version", "450", "--no-es"],)
        outputs = []
        for flags in versions:
            outputs.append(
                run(
                    [self.tools.spirv_cross, spv, *flags, *renames],
                    f"spirv-cross {entry.name}",
                )
            )
        return outputs[0], outputs[1], outputs[2]

    def validate_wgsl(self, wgsl: str) -> None:
        if self.tools.wgsl_validate:
            path = self.path(f"{self.spec.name}.wgsl")
            run([self.tools.wgsl_validate, path], "WGSL validation")

    def validate_msl(self) -> None:
        if self.tools.metal:
            path = self.path(f"{self.spec.name}.metal")
            run(
                [
                    "xcrun",
                    "-sdk",
                    "macosx",
                    "metal",
                    "-std=metal3.0",
                    "-c",
                    path,
                    "-o",
                    path + ".air",
                ],
                "xcrun metal",
            )


def lint_parameters(reflection: dict) -> list[str]:
    errors = []
    for param in reflection.get("parameters", []):
        name = param.get("name", "?")
        if param.get("type", {}).get("kind") == "parameterBlock":
            errors.append(
                f"{name}: ParameterBlock becomes a Metal argument buffer, which IGL cannot bind"
            )
        bindings = param.get("bindings") or [param.get("binding", {})]
        if any(b.get("kind") == "uniform" for b in bindings):
            errors.append(
                f"{name}: a global uniform outside a constant buffer; declare it in a struct bound with IGL_UNIFORM_BUFFER"
            )
    return errors


def lint_entry_points(reflection: dict, portable: bool) -> list[str]:
    errors = []
    for entry in reflection.get("entryPoints", []):
        stage = entry.get("stage")
        if stage == "vertex":
            for semantic in vertex_input_semantics(entry).values():
                if semantic not in ATTRIBUTE_NAMES:
                    errors.append(
                        f"{entry['name']}: vertex input semantic {semantic} is not one of {sorted(ATTRIBUTE_NAMES)}"
                    )
        size = entry.get("threadGroupSize", [1, 1, 1])
        if portable and stage == "compute" and size[0] * size[1] * size[2] > 128:
            errors.append(
                f"{entry['name']}: {size} threads per group; OpenGL ES 3.1 guarantees 128"
            )
    return errors


def lint(spec: ProgramSpec, reflections: dict[str, dict]) -> list[str]:
    """Checks the rules that keep bindings identical on every target (IGL Slang ABI v1)."""
    portable = spec.profile is PROFILES["portable"]
    errors: list[str] = []
    for reflection in reflections.values():
        errors += lint_parameters(reflection)
    errors += lint_entry_points(reflections["metal"], portable)
    if portable:
        errors += lint_layouts(reflections)
    return errors


def lint_compute_uniforms(artifacts: Artifacts) -> list[str]:
    """IGL's OpenGL compute pipelines bind buffers by storage block name only."""
    return [
        f"{entry.name}: uniform buffer {r.name} in a compute entry point; OpenGL compute binds storage blocks only, so use IGL_STORAGE_BUFFER"
        for i, entry in enumerate(artifacts.entries)
        if entry.stage == "compute"
        for r in artifacts.gl_resources
        if r.kind == "UniformBlock" and i in r.entry_points
    ]


def uniform_layouts(reflection: dict) -> dict[str, list[tuple[str, int, int]]]:
    layouts = {}
    for param in reflection.get("parameters", []):
        element = param.get("type", {}).get("elementType", {})
        if (
            param.get("type", {}).get("kind") == "constantBuffer"
            and element.get("kind") == "struct"
        ):
            layouts[param["name"]] = [
                (
                    f.get("name"),
                    f.get("binding", {}).get("offset", -1),
                    f.get("binding", {}).get("size", -1),
                )
                for f in element.get("fields", [])
            ]
    return layouts


def lint_layouts(reflections: dict[str, dict]) -> list[str]:
    errors = []
    # Each block is compared with the first target that declares it.
    references: dict[str, tuple[str, list[tuple[str, int, int]]]] = {}
    for target, reflection in reflections.items():
        for block, fields in uniform_layouts(reflection).items():
            reference_target, reference = references.setdefault(block, (target, fields))
            if reference != fields:
                errors.append(
                    f"{block}: member offsets differ between {reference_target} {reference} and {target} {fields}; "
                    "pad to 16-byte vectors (no float3 followed by a scalar)"
                )
    return errors


def vertex_input_semantics(entry: dict) -> dict[int, str]:
    semantics: dict[int, str] = {}
    for param in entry.get("scope", {}).get("parameters", []) or entry.get(
        "parameters", []
    ):
        fields = param.get("type", {}).get("fields")
        candidates = fields if fields else [param]
        for field_ in candidates:
            binding = field_.get("binding", {})
            if binding.get("kind") == "varyingInput" and "semanticName" in field_:
                semantic = field_["semanticName"].upper()
                if field_.get("semanticIndex", 0):
                    semantic += str(field_["semanticIndex"])
                semantics[binding["index"]] = semantic
    return semantics


BLOCK_RE = re.compile(r"layout\(([^)]*)\)\s*(uniform|buffer)\s+(\w+)\s*\{")
SAMPLER_RE = re.compile(
    r"layout\(([^)]*)\)\s*uniform\s+(?:\w+\s+)*?\w*sampler\w*\s+(\w+)\s*;"
)
BINDING_RE = re.compile(r"\bbinding\s*=\s*(\d+)")


def block_instance_name(glsl: str, open_brace: int) -> str:
    depth = 0
    for i in range(open_brace, len(glsl)):
        depth += {"{": 1, "}": -1}.get(glsl[i], 0)
        if depth == 0:
            match = re.match(r"\s*(\w*)\s*;", glsl[i + 1 :])
            return match.group(1) if match else ""
    return ""


def gl_resources(
    named_glsl: str,
    outputs: list[str],
    entry_index: int,
    resources: dict[tuple[str, int], GlResource],
) -> None:
    """Reads resource names from GLSL that keeps layout(binding); SPIRV-Cross names resources the
    same way for every GLSL version, but deduplicates block names, so reflection cannot be used."""
    found: list[tuple[str, int, str, str]] = []
    for match in BLOCK_RE.finditer(named_glsl):
        binding = BINDING_RE.search(match.group(1))
        if binding:
            kind = "UniformBlock" if match.group(2) == "uniform" else "StorageBlock"
            instance = block_instance_name(named_glsl, match.end() - 1)
            found.append((kind, int(binding.group(1)), match.group(3), instance))
    for match in SAMPLER_RE.finditer(named_glsl):
        binding = BINDING_RE.search(match.group(1))
        if binding:
            found.append(("Sampler", int(binding.group(1)), match.group(2), ""))
    for kind, index, name, instance in found:
        for glsl in outputs:
            if not re.search(rf"\b{re.escape(name)}\b", glsl):
                raise GeneratorError(
                    f"{name} is missing from GLSL that SPIRV-Cross generated"
                )
        entry = resources.setdefault(
            (kind, index), GlResource(kind, index, name, instance)
        )
        if (entry.name, entry.instance_name) != (name, instance):
            raise GeneratorError(
                f"{kind} {index} is {entry.name} in one stage and {name} in another"
            )
        entry.entry_points.add(entry_index)


def compile_program(tools: Tools, spec: ProgramSpec, tmp: str) -> Artifacts:
    compiler = Compiler(tools, spec, tmp)
    targets = spec.profile.targets
    msl, metal_reflection = compiler.whole_module("metal", "IGL_TARGET_METAL", "metal")
    compiler.validate_msl()
    reflections = {"metal": metal_reflection}
    artifacts = Artifacts(
        entries=[
            Entry(e["name"], e["stage"], list(e.get("threadGroupSize", [1, 1, 1])))
            for e in metal_reflection.get("entryPoints", [])
        ],
        msl=msl,
    )
    if not artifacts.entries:
        raise GeneratorError(f"{spec.source} has no [shader(...)] entry points")
    for entry in artifacts.entries:
        if entry.name == "main":
            raise GeneratorError(
                "an entry point named main is not valid MSL; rename it"
            )
        if entry.stage not in IGL_STAGES:
            raise GeneratorError(f"{entry.name}: unsupported stage {entry.stage}")
    if "wgsl" in targets:
        artifacts.wgsl, reflections["wgsl"] = compiler.whole_module(
            "wgsl", "IGL_TARGET_WGSL", "wgsl"
        )
        compiler.validate_wgsl(artifacts.wgsl)
    semantics: dict[int, str] = {}
    for e in metal_reflection.get("entryPoints", []):
        if e.get("stage") == "vertex":
            semantics.update(vertex_input_semantics(e))
    vertex_names = {
        location: ATTRIBUTE_NAMES.get(s, s.lower()) for location, s in semantics.items()
    }
    artifacts.attributes = sorted(vertex_names.items())
    resources: dict[tuple[str, int], GlResource] = {}
    for entry in artifacts.entries:
        spv, entry.spirv, reflections[f"spirv:{entry.name}"] = compiler.spirv(entry)
        if "hlsl" in targets and entry.stage in ("vertex", "fragment", "compute"):
            entry.hlsl = compiler.hlsl(entry)
        if "gl" in targets and entry.stage in ("vertex", "fragment", "compute"):
            entry.glsl, entry.essl, named_glsl = compiler.gl(entry, spv, vertex_names)
            gl_resources(
                named_glsl,
                [entry.glsl, entry.essl],
                artifacts.entries.index(entry),
                resources,
            )
    artifacts.gl_resources = sorted(resources.values(), key=lambda r: (r.kind, r.index))
    errors = lint(spec, reflections) + lint_compute_uniforms(artifacts)
    if errors:
        raise GeneratorError(
            f"{spec.source} breaks the IGL Slang ABI:\n  " + "\n  ".join(errors)
        )
    artifacts.slangc_version = run(
        [tools.slangc, "-v"], "slangc -v", with_stderr=True
    ).strip()
    return artifacts


def cpp_chunks(text: str) -> list[str]:
    chunks, current = [], ""
    for line in text.splitlines(keepends=True):
        if current and len(current) + len(line) > CHUNK_CHARS:
            chunks.append(current)
            current = ""
        current += line
    if current:
        chunks.append(current)
    for chunk in chunks:
        if ')slang"' in chunk:
            raise GeneratorError(
                'shader code contains )slang", the raw string delimiter'
            )
    return chunks


def emit_chunks(name: str, text: str) -> tuple[list[str], str]:
    if not text:
        return [], "{}"
    lines = [f"inline constexpr const char* {name}[] = {{"]
    for chunk in cpp_chunks(text):
        lines.append(f'    R"slang({chunk})slang",')
    lines.append("};")
    return lines, f"{{{name}, std::size({name})}}"


def emit_words(name: str, words: list[int]) -> list[str]:
    lines = [f"inline constexpr uint32_t {name}[] = {{"]
    for i in range(0, len(words), 8):
        lines.append("    " + " ".join(f"0x{w:08x}u," for w in words[i : i + 8]))
    lines.append("};")
    return lines


def cpp_string(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_header(
    spec: ProgramSpec, artifacts: Artifacts, source_label: str, namespace: str
) -> str:
    detail = f"detail{spec.name}"
    body: list[str] = []
    entry_inits: list[str] = []
    for entry in artifacts.entries:
        prefix = f"k{entry.name[0].upper()}{entry.name[1:]}"
        body += emit_words(f"{prefix}Spirv", entry.spirv)
        fields = [
            f".stage = igl::ShaderStage::{IGL_STAGES[entry.stage]}",
            f".name = {cpp_string(entry.name)}",
            f".spirv = {prefix}Spirv",
            f".spirvWords = std::size({prefix}Spirv)",
        ]
        for member, text in (
            ("glsl", entry.glsl),
            ("essl", entry.essl),
            ("hlsl", entry.hlsl),
        ):
            lines, init = emit_chunks(f"{prefix}{member.capitalize()}", text)
            body += lines
            if text:
                fields.append(f".{member} = {init}")
        size = entry.workgroup_size
        fields.append(f".workgroupSize = {{{size[0]}, {size[1]}, {size[2]}}}")
        entry_inits.append("    {" + ", ".join(fields) + "},")
    msl_lines, msl_init = emit_chunks("kMsl", artifacts.msl)
    wgsl_lines, wgsl_init = emit_chunks("kWgsl", artifacts.wgsl)
    body += msl_lines + wgsl_lines
    body += [
        "inline constexpr iglu::slang::EntryPoint kEntryPoints[] = {",
        *entry_inits,
        "};",
    ]
    program = [
        f".name = {cpp_string(spec.name)}",
        ".entryPoints = kEntryPoints",
        ".numEntryPoints = std::size(kEntryPoints)",
    ]
    if artifacts.msl:
        program.append(f".msl = {msl_init}")
    if artifacts.wgsl:
        program.append(f".wgsl = {wgsl_init}")
    if artifacts.gl_resources:
        body.append("inline constexpr iglu::slang::GlResourceName kGlResources[] = {")
        for r in artifacts.gl_resources:
            body.append(
                f"    {{.kind = iglu::slang::GlResourceKind::{r.kind}, .index = {r.index}, "
                f".name = {cpp_string(r.name)}, .instanceName = {cpp_string(r.instance_name)}, "
                f".entryPointMask = 0x{sum(1 << i for i in r.entry_points):x}u}},"
            )
        body.append("};")
        program += [
            ".glResources = kGlResources",
            ".numGlResources = std::size(kGlResources)",
        ]
    if artifacts.attributes:
        body.append(
            "inline constexpr iglu::slang::VertexAttribute kVertexAttributes[] = {"
        )
        for location, name in artifacts.attributes:
            body.append(f"    {{.location = {location}, .name = {cpp_string(name)}}},")
        body.append("};")
        program += [
            ".vertexAttributes = kVertexAttributes",
            ".numVertexAttributes = std::size(kVertexAttributes)",
        ]
    header = [
        "/*",
        " * Copyright (c) Meta Platforms, Inc. and affiliates.",
        " *",
        " * This source code is licensed under the MIT license found in the",
        " * LICENSE file in the root directory of this source tree.",
        " */",
        "",
        "// @" + "generated",
        f"// Generated by tools/slang/generate_slang.py from {source_label} with slangc {artifacts.slangc_version}.",
        *(
            [f"// @{'codegen'}-command : python3 {REGENERATE} // @fb-only"]
            if REGENERATE
            else []
        ),
        "// clang-format off",
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <iterator>",
        "#include <IGLU/slang/SlangProgram.h>",
        "",
        f"namespace {namespace} {{",
        "",
        f"namespace {detail} {{",
        *body,
        "inline constexpr iglu::slang::Program kProgram = {",
        *[f"    {line}," for line in program],
        "};",
        f"}} // namespace {detail}",
        "",
        f"inline constexpr const iglu::slang::Program& k{spec.name}Program = {detail}::kProgram;",
        "",
        f"}} // namespace {namespace}",
        "",
    ]
    return "\n".join(header)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--slangc", required=True)
    parser.add_argument("--spirv-cross", required=True)
    parser.add_argument("--spirv-val")
    parser.add_argument("--dxc")
    parser.add_argument("--wgsl-validate")
    parser.add_argument(
        "--metal", action="store_true", help="validate MSL with xcrun metal (macOS)"
    )
    parser.add_argument(
        "--program", action="append", help="only this program (repeatable)"
    )
    parser.add_argument(
        "--check", action="store_true", help="fail if a generated header is out of date"
    )
    parser.add_argument(
        "--keep-temps", help="keep intermediate files in this directory"
    )
    args = parser.parse_args()

    manifest_path = os.path.abspath(args.manifest)
    manifest_dir = os.path.dirname(manifest_path)
    manifest = read_json(manifest_path)
    tools = Tools(
        args.slangc,
        args.spirv_cross,
        args.spirv_val,
        args.dxc,
        args.wgsl_validate,
        args.metal,
    )
    out_dir = os.path.join(manifest_dir, "generated")
    stale = []
    for program in manifest["programs"]:
        if args.program and program["name"] not in args.program:
            continue
        spec = ProgramSpec(
            name=program["name"],
            source=os.path.join(manifest_dir, program["source"]),
            profile=PROFILES[program.get("profile", "portable")],
            include_dirs=[
                os.path.join(manifest_dir, d) for d in manifest.get("includeDirs", [])
            ],
            conformances=program.get("conformances", []),
        )
        try:
            if args.keep_temps:
                tmp = os.path.join(args.keep_temps, spec.name)
                os.makedirs(tmp, exist_ok=True)
                artifacts = compile_program(tools, spec, tmp)
            else:
                with tempfile.TemporaryDirectory() as tmp:
                    artifacts = compile_program(tools, spec, tmp)
        except GeneratorError as e:
            print(f"error: {e}", file=sys.stderr)
            return 1
        text = emit_header(spec, artifacts, program["source"], manifest["namespace"])
        out = os.path.join(out_dir, f"{spec.name}Slang.h")
        current = read_text(out) if os.path.exists(out) else None
        if args.check:
            if current != text:
                stale.append(out)
            continue
        if current != text:
            os.makedirs(out_dir, exist_ok=True)
            with open(out, "w", encoding="utf-8") as f:
                f.write(text)
            print(f"wrote {out}")
    if stale:
        print(
            "out of date (rerun generate_slang.py):\n  " + "\n  ".join(stale),
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
