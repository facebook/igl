# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

import unittest

from glsl2slang import ConversionError, convert, parse_assignments, reflect

VERTEX = """#version 450
precision highp float;
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv_in;
layout(location = 0) out vec2 uv;

layout(set = 1, binding = 1, std140) uniform Frame {
  mat4 mvp;
  mat4 model;
} frame;

void main() {
  // Clip-space position.
  gl_Position = frame.mvp * vec4(position, 1.0);
  uv = uv_in;
}
"""

FRAGMENT = """#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 2) uniform sampler2D albedo;

void main() {
  color = texture(albedo, uv);
}
"""


def body(source: str, **kwargs: object) -> str:
    """The converted fragment stage of `source` (statements of main)."""
    return convert({"fragment": source}, **kwargs)


class TypesAndBuiltinsTest(unittest.TestCase):
    def test_renames_types(self) -> None:
        out = body(
            "void main() { vec3 a = vec3(1.0); ivec2 b = ivec2(0); uvec4 c; bvec2 d; mat3x2 e; }"
        )
        for expected in ("float3 a", "int2 b", "uint4 c", "bool2 d", "float3x2 e"):
            self.assertIn(expected, out)

    def test_renames_builtins(self) -> None:
        out = body(
            "layout(location = 0) out vec4 c;\n"
            "void main() { float x = 1.0; c = vec4(mix(x, 2.0, 0.5), fract(x),"
            " inversesqrt(x), atan(x, 2.0)); }"
        )
        for expected in ("lerp(x, 2.0, 0.5)", "frac(x)", "rsqrt(x)", "atan2(x, 2.0)"):
            self.assertIn(expected, out)

    def test_mod_uses_a_helper(self) -> None:
        out = body("void main() { float m = mod(-1.0, 3.0); }")
        self.assertIn("#define glsl_mod(x, y)", out)
        self.assertIn("glsl_mod(-1.0, 3.0)", out)

    def test_vector_comparisons_become_operators(self) -> None:
        out = body("void main() { vec2 a; bool b = any(lessThan(a, vec2(0.5))); }")
        self.assertIn("any((a < float2(0.5)))", out)


class MatrixProductTest(unittest.TestCase):
    def test_products_follow_the_matrix_rule(self) -> None:
        out = body(
            "void main() { mat4 a; mat4 b; vec4 v; vec4 p = a * v; vec4 q = v * a;"
            " mat4 c = a * b; float s = 2.0; mat4 d = a * s; }"
        )
        self.assertIn("p = mul(v, a)", out)
        self.assertIn("q = mul(a, v)", out)
        self.assertIn("c = mul(b, a)", out)
        self.assertIn("d = a * s", out)

    def test_compound_matrix_product(self) -> None:
        out = body("void main() { mat4 a; mat4 b; a *= b; }")
        self.assertIn("a = mul(b, a)", out)

    def test_truncating_matrix_constructor_becomes_a_cast(self) -> None:
        out = body("void main() { mat4 a; mat3 b = mat3(a); }")
        self.assertIn("((float3x3)a)", out)

    def test_block_member_matrices(self) -> None:
        out = convert({"vertex": VERTEX})
        self.assertIn("mul(float4(position, 1.0), frame.mvp)", out)


class ResourcesTest(unittest.TestCase):
    def test_uniform_block_and_sampler(self) -> None:
        out = convert({"vertex": VERTEX, "fragment": FRAGMENT})
        self.assertIn("IGL_UNIFORM_BUFFER(Frame, frame, 1);", out)
        self.assertIn("IGL_TEXTURE2D(albedo, 2);", out)
        self.assertIn("IGL_SAMPLE2D(albedo, uv)", out)

    def test_overrides_bindings(self) -> None:
        out = convert(
            {"vertex": VERTEX, "fragment": FRAGMENT},
            textures={"albedo": 0},
            buffers={"frame": 3},
        )
        self.assertIn("IGL_UNIFORM_BUFFER(Frame, frame, 3);", out)
        self.assertIn("IGL_TEXTURE2D(albedo, 0);", out)

    def test_unnamed_block_members_get_an_instance(self) -> None:
        out = body(
            "layout(binding = 1) uniform Params { vec4 tint; };\n"
            "layout(location = 0) out vec4 c;\nvoid main() { c = tint; }"
        )
        self.assertIn("IGL_UNIFORM_BUFFER(Params, params, 1);", out)
        self.assertIn("c = params.tint;", out)

    def test_loose_uniforms_gather_into_a_buffer(self) -> None:
        out = body(
            "uniform vec4 tint;\nuniform float time;\n"
            "layout(location = 0) out vec4 c;\nvoid main() { c = tint * time; }",
            uniforms_index=2,
        )
        self.assertIn("IGL_UNIFORM_BUFFER(Uniforms, uniforms, 2);", out)
        self.assertIn("c = uniforms.tint * uniforms.time;", out)

    def test_storage_buffer_atomics(self) -> None:
        out = convert(
            {
                "compute": "layout(local_size_x = 64) in;\n"
                "layout(binding = 0) buffer Counts { uint counts[]; } c;\n"
                "void main() { atomicAdd(c.counts[0], 1u); uint n = c.counts[1];"
                " c.counts[2] = n; }"
            }
        )
        self.assertIn("IGL_STORAGE_BUFFER(Atomic<uint>, c, 0);", out)
        self.assertIn("c[0].add(1u);", out)
        self.assertIn("uint n = c[1].load();", out)
        self.assertIn("c[2].store(n);", out)

    def test_needs_a_binding(self) -> None:
        with self.assertRaisesRegex(ConversionError, "--texture"):
            body("uniform sampler2D t;\nvoid main() {}")

    def test_rejects_textures_the_abi_lacks(self) -> None:
        with self.assertRaisesRegex(ConversionError, "2D textures only"):
            body("layout(binding = 0) uniform samplerCube t;\nvoid main() {}")


class StagesTest(unittest.TestCase):
    def test_vertex_entry_point(self) -> None:
        out = convert({"vertex": VERTEX})
        self.assertIn("[[vk::location(0)]] float3 position : POSITION;", out)
        self.assertIn("[[vk::location(1)]] float2 uv_in : TEXCOORD;", out)
        self.assertIn("float4 glsl_Position : SV_Position;", out)
        self.assertIn('[shader("vertex")]', out)
        self.assertIn("output.glsl_Position = glsl_Position;", out)

    def test_stages_share_a_program_in_namespaces(self) -> None:
        out = convert({"vertex": VERTEX, "fragment": FRAGMENT})
        self.assertIn("namespace vertex_glsl {", out)
        self.assertIn("namespace fragment_glsl {", out)
        self.assertIn("vertex_glsl::glslMain();", out)
        self.assertIn("[[vk::location(0)]] float2 uv : TEXCOORD0;", out)

    def test_varyings_must_match(self) -> None:
        fragment = FRAGMENT.replace("in vec2 uv", "in vec3 uv")
        with self.assertRaisesRegex(ConversionError, "no matching vertex output"):
            convert({"vertex": VERTEX, "fragment": fragment})

    def test_glsl_es_1(self) -> None:
        out = convert(
            {
                "vertex": "attribute vec4 position;\nvarying vec2 v;\n"
                "void main() { v = position.xy; gl_Position = position; }",
                "fragment": "precision mediump float;\nvarying vec2 v;\n"
                "void main() { gl_FragColor = vec4(v, 0.0, 1.0); }",
            }
        )
        self.assertIn("float4 glsl_FragColor : SV_Target0;", out)
        self.assertIn("glsl_FragColor = float4(v, 0.0, 1.0);", out)

    def test_rejects_frag_color_with_declared_outputs(self) -> None:
        with self.assertRaisesRegex(
            ConversionError, "gl_FragColor and declares outputs"
        ):
            body(
                "layout(location = 0) out vec4 c;\n"
                "void main() { c = vec4(1.0); gl_FragColor = vec4(0.0); }"
            )

    def test_compute_builtins_and_shared_memory(self) -> None:
        out = convert(
            {
                "compute": "layout(local_size_x = 8, local_size_y = 8) in;\n"
                "shared float tile[64];\n"
                "void main() { tile[gl_LocalInvocationIndex] = 1.0; barrier(); }"
            }
        )
        self.assertIn("[numthreads(8, 8, 1)]", out)
        self.assertIn("groupshared float tile[64];", out)
        self.assertIn("GroupMemoryBarrierWithGroupSync();", out)
        self.assertIn("uint gl_LocalInvocationIndex_ : SV_GroupIndex", out)

    def test_flat_varyings(self) -> None:
        out = convert(
            {
                "vertex": "layout(location = 0) flat out int id;\nvoid main() { id = 1; }",
                "fragment": "layout(location = 0) flat in int id;\n"
                "layout(location = 0) out vec4 c;\nvoid main() { c = vec4(float(id)); }",
            }
        )
        self.assertIn("nointerpolation int id : TEXCOORD0;", out)


class FidelityTest(unittest.TestCase):
    def test_keeps_comments_and_preprocessor_lines(self) -> None:
        out = convert({"vertex": VERTEX})
        self.assertIn("// Clip-space position.", out)
        source = (
            "layout(location = 0) out vec4 c;\nvoid main() {\n#if defined(RED)\n"
            "  c = vec4(1.0, 0.0, 0.0, 1.0);\n#else\n  c = vec4(1.0);\n#endif\n}"
        )
        out = body(source)
        self.assertIn("#if defined(RED)", out)
        self.assertIn("#else", out)
        self.assertIn("#endif", out)

    def test_drops_version_and_precision(self) -> None:
        out = convert({"vertex": VERTEX})
        self.assertNotIn("#version", out)
        self.assertNotIn("precision", out)


class ErrorsTest(unittest.TestCase):
    def test_unclosed_array_is_a_conversion_error(self) -> None:
        with self.assertRaisesRegex(ConversionError, "unclosed"):
            body("uniform vec4 tint[2;\nvoid main() {}")

    def test_assignments_need_a_name(self) -> None:
        with self.assertRaises(SystemExit):
            parse_assignments(["=5"], "texture")

    def test_reflect_reports_a_failing_tool(self) -> None:
        with self.assertRaisesRegex(ConversionError, "spirv-cross"):
            reflect("false", "missing.spv")
