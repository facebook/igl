# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

"""Converts GLSL shaders to Slang programs that follow the IGL Slang ABI (igl_abi.slangh).

usage: glsl2slang.py [--vertex FILE] [--fragment FILE] [--compute FILE] [-o OUT.slang]
                     [--texture NAME=UNIT]... [--buffer NAME=INDEX]... [--uniforms-index N]

A vertex and a fragment shader convert into one program whose stages share the varyings. The
output keeps the input's statements, comments and preprocessor lines, and changes what Slang or the
ABI needs:
- Types and built-ins take their Slang names (vec3 -> float3, mix -> lerp, atan(y, x) -> atan2).
- Matrix products follow the ABI's matrix rule: matrices are uploaded column-major and read
  row-major, so a GLSL matrix reads as its transpose, `M * v` becomes `mul(v, M)` and `A * B`
  becomes `mul(B, A)`. Matrix constructors, indexing and matCxR need no change under that rule.
- Samplers, uniform blocks and storage buffers become IGL_TEXTURE2D, IGL_UNIFORM_BUFFER and
  IGL_STORAGE_BUFFER at their IGL indices: `binding` from the layout qualifier unless --texture or
  --buffer says otherwise. Loose uniforms are gathered into one uniform buffer (--uniforms-index).
- Stage inputs and outputs become static variables that the entry point fills and returns, so
  helper functions that read them keep working.

Anything it cannot convert is reported as an error with its line; review the output, then check it
with `generate_slang.py`, whose linter enforces the rest of the ABI.
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

# ---------------------------------------------------------------------------------------------
# Lexer


@dataclass
class Token:
    kind: str  # "id", "num", "op", "ws", "comment", "pp" (a whole preprocessor line), "eof"
    text: str
    line: int


TOKEN_RE = re.compile(
    r"""
    (?P<ws>[ \t\r\n]+)
  | (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<num>(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuUlL]*|0[xX][0-9a-fA-F]+[uU]?)
  | (?P<id>[A-Za-z_]\w*)
  | (?P<op><<=|>>=|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\^\^|\+=|-=|\*=|/=|%=|&=|\|=|\^=|[-+*/%<>=!~&|^?:;,.(){}\[\]])
    """,
    re.VERBOSE | re.DOTALL,
)


class ConversionError(Exception):
    pass


def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    line = 1
    pos = 0
    at_line_start = True
    while pos < len(source):
        if at_line_start:
            m = re.match(r"[ \t]*#", source[pos:])
            if m:
                end = pos
                # A preprocessor line, with backslash continuations.
                while True:
                    nl = source.find("\n", end)
                    if nl == -1:
                        nl = len(source)
                    if source[nl - 1 : nl] == "\\" and nl < len(source):
                        end = nl + 1
                        continue
                    break
                text = source[pos:nl]
                tokens.append(Token("pp", text, line))
                line += text.count("\n")
                pos = nl
                continue
        m = TOKEN_RE.match(source, pos)
        if not m:
            raise ConversionError(f"line {line}: unexpected character {source[pos]!r}")
        kind = m.lastgroup or "op"
        text = m.group()
        tokens.append(Token(kind, text, line))
        newlines = text.count("\n")
        line += newlines
        if kind == "ws" and newlines:
            at_line_start = True
        elif kind != "ws":
            at_line_start = False
        pos = m.end()
    tokens.append(Token("eof", "", line))
    return tokens


# ---------------------------------------------------------------------------------------------
# Types

SCALARS = {
    "float": "float",
    "int": "int",
    "uint": "uint",
    "bool": "bool",
    "double": "double",
}
VECTOR_PREFIX = {
    "vec": "float",
    "ivec": "int",
    "uvec": "uint",
    "bvec": "bool",
    "dvec": "double",
}
SAMPLER_2D = {"sampler2D"}
UNSUPPORTED_TYPES = {
    "sampler1D",
    "sampler3D",
    "samplerCube",
    "sampler2DArray",
    "sampler2DShadow",
    "samplerExternalOES",
    "isampler2D",
    "usampler2D",
    "image2D",
}
QUALIFIERS_DROPPED = {"highp", "mediump", "lowp", "smooth", "invariant", "precise"}
DECLARATION_QUALIFIERS = QUALIFIERS_DROPPED | {
    "in",
    "out",
    "uniform",
    "buffer",
    "shared",
    "flat",
    "noperspective",
    "centroid",
    "readonly",
    "writeonly",
    "coherent",
    "restrict",
    "volatile",
    "const",
    "attribute",
    "varying",
}


@dataclass(frozen=True)
class Type:
    """A GLSL value type: base scalar plus vector size, or matrix columns x rows."""

    base: str  # float, int, uint, bool, double, void, struct name, or "?" when unknown
    rows: int = 1  # vector size, or matrix rows
    cols: int = 1  # matrix columns (1 for scalars and vectors)

    @property
    def is_matrix(self) -> bool:
        return self.cols > 1

    @property
    def is_vector(self) -> bool:
        return self.cols == 1 and self.rows > 1


UNKNOWN = Type("?")


def parse_type_name(name: str) -> Type | None:
    if name in SCALARS:
        return Type(SCALARS[name])
    if name == "void":
        return Type("void")
    m = re.fullmatch(r"(vec|ivec|uvec|bvec|dvec)([234])", name)
    if m:
        return Type(VECTOR_PREFIX[m.group(1)], int(m.group(2)))
    m = re.fullmatch(r"d?mat([234])(?:x([234]))?", name)
    if m:
        cols = int(m.group(1))
        rows = int(m.group(2) or m.group(1))
        return Type("double" if name.startswith("d") else "float", rows, cols)
    return None


def slang_type_name(name: str) -> str:
    """The Slang spelling of a GLSL type name. matCxR maps to floatCxR: see the matrix rule."""
    m = re.fullmatch(r"(vec|ivec|uvec|bvec|dvec)([234])", name)
    if m:
        return f"{VECTOR_PREFIX[m.group(1)]}{m.group(2)}"
    m = re.fullmatch(r"(d?)mat([234])(?:x([234]))?", name)
    if m:
        base = "double" if m.group(1) else "float"
        cols = m.group(2)
        rows = m.group(3) or m.group(2)
        return f"{base}{cols}x{rows}"
    return name


# ---------------------------------------------------------------------------------------------
# Expressions


@dataclass
class Expr:
    kind: str  # id, num, call, member, index, unary, postfix, binary, ternary, assign, comma, paren
    text: str = ""
    args: list[Expr] = field(default_factory=list)
    type: Type = UNKNOWN


BINARY_PRECEDENCE = {
    "||": 1,
    "^^": 2,
    "&&": 3,
    "|": 4,
    "^": 5,
    "&": 6,
    "==": 7,
    "!=": 7,
    "<": 8,
    ">": 8,
    "<=": 8,
    ">=": 8,
    "<<": 9,
    ">>": 9,
    "+": 10,
    "-": 10,
    "*": 11,
    "/": 11,
    "%": 11,
}
ASSIGN_OPS = {"=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^="}

RENAMED_FUNCTIONS = {
    "mix": "lerp",
    "fract": "frac",
    "inversesqrt": "rsqrt",
    "dFdx": "ddx",
    "dFdy": "ddy",
    "dFdxFine": "ddx_fine",
    "dFdyFine": "ddy_fine",
    "dFdxCoarse": "ddx_coarse",
    "dFdyCoarse": "ddy_coarse",
    "floatBitsToInt": "asint",
    "floatBitsToUint": "asuint",
    "intBitsToFloat": "asfloat",
    "uintBitsToFloat": "asfloat",
    "matrixCompMult": "__glsl_matrixCompMult",
    "atomicAdd": "InterlockedAdd",
    "atomicMin": "InterlockedMin",
    "atomicMax": "InterlockedMax",
    "atomicAnd": "InterlockedAnd",
    "atomicOr": "InterlockedOr",
    "atomicXor": "InterlockedXor",
    "atomicExchange": "InterlockedExchange",
    "barrier": "GroupMemoryBarrierWithGroupSync",
    "memoryBarrierShared": "GroupMemoryBarrier",
    "groupMemoryBarrier": "GroupMemoryBarrier",
    "memoryBarrier": "AllMemoryBarrier",
    "memoryBarrierBuffer": "DeviceMemoryBarrier",
}
ATOMIC_METHODS = {
    "atomicAdd": "add",
    "atomicMin": "min",
    "atomicMax": "max",
    "atomicAnd": "and",
    "atomicOr": "or",
    "atomicXor": "xor",
    "atomicExchange": "exchange",
    "atomicCompSwap": "compareExchange",
}
ATOMIC_CALL_RE = re.compile(r"\b(atomic[A-Z]\w*)\s*\(\s*(\w+)(?:\s*\.\s*(\w+))?\s*\[")
# GLSL texture function -> (ABI macro, GLSL argument count).
TEXTURE_FUNCTIONS = {
    "texture": ("IGL_SAMPLE2D", 2),
    "texture2D": ("IGL_SAMPLE2D", 2),
    "textureLod": ("IGL_SAMPLE2D_LEVEL", 3),
    "texture2DLod": ("IGL_SAMPLE2D_LEVEL", 3),
    "texelFetch": ("IGL_LOAD2D", 3),
}
UNCONVERTED_FUNCTIONS = {
    "textureSize",
    "textureGather",
    "textureProj",
    "textureGrad",
    "inverse",
}
COMPARISONS = {
    "lessThan": "<",
    "lessThanEqual": "<=",
    "greaterThan": ">",
    "greaterThanEqual": ">=",
    "equal": "==",
    "notEqual": "!=",
}
# Built-ins whose result has the type of their first argument.
SAME_TYPE_BUILTINS = {
    "abs",
    "sign",
    "floor",
    "ceil",
    "round",
    "trunc",
    "fract",
    "mod",
    "min",
    "max",
    "clamp",
    "mix",
    "step",
    "smoothstep",
    "sqrt",
    "inversesqrt",
    "exp",
    "exp2",
    "log",
    "log2",
    "pow",
    "sin",
    "cos",
    "tan",
    "asin",
    "acos",
    "atan",
    "sinh",
    "cosh",
    "tanh",
    "radians",
    "degrees",
    "normalize",
    "reflect",
    "refract",
    "faceforward",
    "dFdx",
    "dFdy",
    "fwidth",
    "saturate",
    "inverse",
}
SCALAR_BUILTINS = {"length": None, "distance": None, "dot": None}


class ExprParser:
    def __init__(self, tokens: list[Token], converter: Converter) -> None:
        self.tokens = [t for t in tokens if t.kind not in ("ws", "comment")]
        self.pos = 0
        self.converter = converter

    def peek(self, offset: int = 0) -> Token:
        i = self.pos + offset
        return self.tokens[i] if i < len(self.tokens) else Token("eof", "", 0)

    def next(self) -> Token:
        token = self.peek()
        self.pos += 1
        return token

    def expect(self, text: str) -> None:
        token = self.next()
        if token.text != text:
            raise ConversionError(
                f"line {token.line}: expected {text!r}, found {token.text!r}"
            )

    def parse(self) -> Expr:
        expr = self.comma()
        if self.peek().kind != "eof":
            token = self.peek()
            raise ConversionError(
                f"line {token.line}: unexpected {token.text!r} in expression"
            )
        return expr

    def comma(self) -> Expr:
        expr = self.assignment()
        while self.peek().text == ",":
            self.next()
            expr = Expr("comma", ",", [expr, self.assignment()], UNKNOWN)
        return expr

    def assignment(self) -> Expr:
        lhs = self.ternary()
        if self.peek().text in ASSIGN_OPS:
            op = self.next().text
            rhs = self.assignment()
            return Expr("assign", op, [lhs, rhs], lhs.type)
        return lhs

    def ternary(self) -> Expr:
        cond = self.binary(1)
        if self.peek().text == "?":
            self.next()
            a = self.assignment()
            self.expect(":")
            b = self.assignment()
            return Expr("ternary", "?", [cond, a, b], a.type)
        return cond

    def binary(self, min_precedence: int) -> Expr:
        lhs = self.unary()
        while True:
            op = self.peek().text
            precedence = BINARY_PRECEDENCE.get(op)
            if (
                precedence is None
                or precedence < min_precedence
                or self.peek().kind != "op"
            ):
                return lhs
            self.next()
            rhs = self.binary(precedence + 1)
            lhs = Expr("binary", op, [lhs, rhs], binary_type(op, lhs.type, rhs.type))

    def unary(self) -> Expr:
        token = self.peek()
        if token.kind == "op" and token.text in ("-", "+", "!", "~", "++", "--"):
            self.next()
            operand = self.unary()
            return Expr("unary", token.text, [operand], operand.type)
        return self.postfix(self.primary())

    def postfix(self, expr: Expr) -> Expr:
        while True:
            token = self.peek()
            if token.text == ".":
                self.next()
                name = self.next().text
                expr = Expr(
                    "member", name, [expr], self.converter.member_type(expr.type, name)
                )
            elif token.text == "[":
                self.next()
                index = self.comma()
                self.expect("]")
                expr = Expr("index", "[]", [expr, index], index_type(expr.type))
            elif token.text in ("++", "--"):
                self.next()
                expr = Expr("postfix", token.text, [expr], expr.type)
            else:
                return expr

    def primary(self) -> Expr:
        token = self.next()
        if token.kind == "num":
            is_float = re.search(
                r"[.eE]|[fF]$", token.text
            ) and not token.text.lower().startswith("0x")
            unsigned = token.text.lower().endswith("u")
            return Expr(
                "num",
                token.text,
                [],
                Type("float" if is_float else ("uint" if unsigned else "int")),
            )
        if token.text == "(":
            inner = self.comma()
            self.expect(")")
            return Expr("paren", "()", [inner], inner.type)
        if token.kind == "id":
            if self.peek().text == "(":
                self.next()
                args: list[Expr] = []
                if self.peek().text != ")":
                    args.append(self.assignment())
                    while self.peek().text == ",":
                        self.next()
                        args.append(self.assignment())
                self.expect(")")
                return Expr(
                    "call", token.text, args, self.converter.call_type(token.text, args)
                )
            if (
                self.peek().text == "["
                and parse_type_name(token.text)
                and self.peek(1).text == "]"
            ):
                raise ConversionError(
                    f"line {token.line}: array constructors are not supported"
                )
            return Expr("id", token.text, [], self.converter.lookup(token.text))
        raise ConversionError(
            f"line {token.line}: unexpected {token.text!r} in expression"
        )


def binary_type(op: str, a: Type, b: Type) -> Type:
    if op in ("==", "!=", "<", ">", "<=", ">=", "&&", "||", "^^"):
        return Type("bool")
    if op == "*":
        if a.is_matrix and b.is_matrix:
            return Type(a.base, a.rows, b.cols)
        if a.is_matrix and b.is_vector:
            return Type(a.base, a.rows)
        if a.is_vector and b.is_matrix:
            return Type(b.base, b.cols)
    if a.is_matrix or a.is_vector:
        return a
    if b.is_matrix or b.is_vector:
        return b
    return a if a != UNKNOWN else b


def index_type(t: Type) -> Type:
    if t.is_matrix:
        return Type(t.base, t.rows)
    if t.is_vector:
        return Type(t.base)
    if t.base.endswith("[]"):
        return Type(t.base[:-2], t.rows, t.cols)
    return UNKNOWN


# ---------------------------------------------------------------------------------------------
# Program model


@dataclass
class Variable:
    name: str
    glsl_type: str
    array: str = ""  # "[N]" or "[]" suffix, verbatim
    location: int | None = None
    interpolation: str = ""  # "nointerpolation", "noperspective", "centroid" or ""


@dataclass
class Block:
    kind: str  # "uniform" or "buffer"
    name: str
    instance: str
    members: list[Variable]
    binding: int | None
    readonly: bool = False


@dataclass
class Stage:
    name: str  # vertex, fragment, compute
    source: str
    inputs: list[Variable] = field(default_factory=list)
    outputs: list[Variable] = field(default_factory=list)
    local_size: tuple[str, str, str] = ("1", "1", "1")
    # Top-level code: functions, structs, constants, preprocessor lines.
    body: list[Token] = field(default_factory=list)
    main_name: str = "glslMain"  # what the stage's main() becomes
    namespace: str = ""  # wraps the stage's code when several stages share a program


ENTRY_NAMES = {
    "vertex": "vertexMain",
    "fragment": "fragmentMain",
    "compute": "computeMain",
}
SEMANTICS = [
    (re.compile(r"pos", re.I), "POSITION"),
    (re.compile(r"norm", re.I), "NORMAL"),
    (re.compile(r"tang", re.I), "TANGENT"),
    (re.compile(r"col", re.I), "COLOR"),
    (re.compile(r"uv|tex|coord", re.I), "TEXCOORD"),
]
VERTEX_BUILTINS = {
    "gl_VertexIndex": ("uint", "IGL_SV_VERTEX_ID"),
    "gl_VertexID": ("uint", "IGL_SV_VERTEX_ID"),
    "gl_InstanceIndex": ("uint", "IGL_SV_INSTANCE_ID"),
    "gl_InstanceID": ("uint", "IGL_SV_INSTANCE_ID"),
}
FRAGMENT_BUILTINS = {
    "gl_FragCoord": ("float4", "SV_Position"),
    "gl_FrontFacing": ("bool", "SV_IsFrontFace"),
}
COMPUTE_BUILTINS = {
    "gl_GlobalInvocationID": ("uint3", "SV_DispatchThreadID"),
    "gl_LocalInvocationID": ("uint3", "SV_GroupThreadID"),
    "gl_WorkGroupID": ("uint3", "SV_GroupID"),
    "gl_LocalInvocationIndex": ("uint", "SV_GroupIndex"),
}
BUILTIN_TYPES = {
    "gl_Position": Type("float", 4),
    "gl_FragCoord": Type("float", 4),
    "gl_FragDepth": Type("float"),
    "gl_PointSize": Type("float"),
    "gl_FrontFacing": Type("bool"),
    "gl_VertexIndex": Type("uint"),
    "gl_VertexID": Type("uint"),
    "gl_InstanceIndex": Type("uint"),
    "gl_InstanceID": Type("uint"),
    "gl_GlobalInvocationID": Type("uint", 3),
    "gl_LocalInvocationID": Type("uint", 3),
    "gl_WorkGroupID": Type("uint", 3),
    "gl_LocalInvocationIndex": Type("uint"),
}


class Converter:
    def __init__(
        self,
        textures: dict[str, int],
        buffers: dict[str, int],
        uniforms_index: int,
    ) -> None:
        self.texture_units = dict(textures)
        self.buffer_indices = dict(buffers)
        self.uniforms_index = uniforms_index
        self.textures: dict[str, int] = {}  # sampler name -> unit
        self.blocks: list[Block] = []
        self.loose_uniforms: list[Variable] = []
        self.structs: dict[str, dict[str, str]] = {}
        self.struct_definitions: dict[str, list[Token]] = {}
        self.functions: dict[str, str] = {}  # name -> GLSL return type
        self.globals: dict[str, Type] = {}
        self.scopes: list[dict[str, Type]] = []
        # Member name -> rewritten reference, for blocks without an instance name.
        self.implicit_members: dict[str, str] = {}
        self.storage_arrays: dict[str, str] = {}  # "instance.member" -> buffer name
        self.shared: list[Variable] = []
        self.helpers: set[str] = set()
        # Storage buffers that atomics touch; they hold Atomic<T>, which WGSL needs.
        self.atomic_buffers: set[str] = set()
        self.raw_atomic = False
        # (set, binding) in the GLSL -> (set, binding) in the Slang output, where they differ.
        self.remaps: dict[tuple[int, int], tuple[int, int]] = {}

    # Types ------------------------------------------------------------------------------------

    def resolve(self, glsl_type: str, array: str = "") -> Type:
        t = parse_type_name(glsl_type)
        if t is None:
            t = Type(glsl_type) if glsl_type in self.structs else UNKNOWN
        if array:
            t = Type(t.base + "[]", t.rows, t.cols)
        return t

    def lookup(self, name: str) -> Type:
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        if name in self.globals:
            return self.globals[name]
        return BUILTIN_TYPES.get(name, UNKNOWN)

    def member_type(self, t: Type, name: str) -> Type:
        if t.is_vector or (t.cols == 1 and t.rows == 1 and t.base in SCALARS.values()):
            if re.fullmatch(r"[xyzw]{1,4}|[rgba]{1,4}|[stpq]{1,4}", name):
                return Type(t.base, len(name)) if len(name) > 1 else Type(t.base)
        if t.base in self.structs and name in self.structs[t.base]:
            member = self.structs[t.base][name]
            return self.resolve(*split_array(member))
        return UNKNOWN

    def call_type(self, name: str, args: list[Expr]) -> Type:
        constructed = parse_type_name(name)
        if constructed is not None:
            return constructed
        if name in self.structs:
            return Type(name)
        if name in SCALAR_BUILTINS:
            return Type(args[0].type.base) if args else UNKNOWN
        if name in SAME_TYPE_BUILTINS and args:
            return args[0].type
        if name == "cross":
            return Type("float", 3)
        if name == "transpose" and args:
            t = args[0].type
            return Type(t.base, t.cols, t.rows)
        if name in ("texture", "texture2D", "textureLod", "texelFetch"):
            return Type("float", 4)
        if name in self.functions:
            return self.resolve(self.functions[name])
        return UNKNOWN

    # Expression output ------------------------------------------------------------------------

    def emit(self, e: Expr) -> str:
        if e.kind == "num":
            return e.text
        if e.kind == "id":
            return self.emit_identifier(e.text)
        if e.kind in ("unary", "postfix", "paren", "comma", "ternary"):
            return self.emit_simple(e)
        emitter = {
            "member": self.emit_member,
            "index": self.emit_index,
            "assign": self.emit_assign,
            "binary": self.emit_binary,
            "call": self.emit_call,
        }.get(e.kind)
        if emitter is None:
            raise ConversionError(f"cannot emit {e.kind}")
        return emitter(e)

    def emit_simple(self, e: Expr) -> str:
        """Expressions whose operands only need converting: (a), -a, a++, a, b and a ? b : c."""
        a = [self.emit(arg) for arg in e.args]
        if e.kind == "unary":
            return e.text + a[0]
        if e.kind == "postfix":
            return a[0] + e.text
        if e.kind == "paren":
            return f"({a[0]})"
        if e.kind == "comma":
            return f"{a[0]}, {a[1]}"
        return f"{a[0]} ? {a[1]} : {a[2]}"

    def emit_member(self, e: Expr) -> str:
        reference = self.storage_reference(e)
        return (
            reference if reference is not None else self.emit(e.args[0]) + "." + e.text
        )

    def emit_index(self, e: Expr) -> str:
        text = self.emit(e.args[0]) + "[" + self.emit(e.args[1]) + "]"
        if self.atomic_target(e) and not self.raw_atomic:
            return text + ".load()"
        return text

    def emit_assign(self, e: Expr) -> str:
        lhs, rhs = e.args
        if self.atomic_target(lhs):
            if e.text != "=":
                raise ConversionError(
                    f"{e.text} on an atomic buffer element; use atomic*()"
                )
            return f"{self.emit_raw(lhs)}.store({self.emit(rhs)})"
        if e.text == "*=" and (lhs.type.is_matrix or rhs.type.is_matrix):
            product = Expr(
                "binary", "*", [lhs, rhs], binary_type("*", lhs.type, rhs.type)
            )
            return f"{self.emit(lhs)} = {self.emit(product)}"
        return f"{self.emit(lhs)} {e.text} {self.emit(rhs)}"

    def emit_identifier(self, name: str) -> str:
        if self.lookup_local(name):
            return name
        if name in self.implicit_members:
            return self.implicit_members[name]
        if name.startswith("gl_") and name in BUILTIN_TYPES | {"gl_FragColor": UNKNOWN}:
            return builtin_name(name)
        return name

    def lookup_local(self, name: str) -> bool:
        return any(name in scope for scope in self.scopes)

    def buffer_name(self, e: Expr) -> str | None:
        """The storage buffer an expression names: `buffer` or `instance.member`."""
        if e.kind == "id" and not self.lookup_local(e.text):
            return e.text
        if e.kind == "member" and e.args[0].kind == "id":
            return self.storage_arrays.get(f"{e.args[0].text}.{e.text}")
        return None

    def atomic_target(self, e: Expr) -> bool:
        return e.kind == "index" and self.buffer_name(e.args[0]) in self.atomic_buffers

    def emit_raw(self, e: Expr) -> str:
        self.raw_atomic = True
        try:
            return self.emit(e)
        finally:
            self.raw_atomic = False

    def storage_reference(self, e: Expr) -> str | None:
        base = e.args[0]
        if base.kind == "id":
            key = f"{base.text}.{e.text}"
            if key in self.storage_arrays:
                return self.storage_arrays[key]
        return None

    def emit_binary(self, e: Expr) -> str:
        a, b = e.args
        if e.text == "*" and (a.type.is_matrix or b.type.is_matrix):
            if a.type.is_matrix and b.type.is_matrix:
                return f"mul({self.emit(b)}, {self.emit(a)})"
            if a.type.is_matrix and b.type.is_vector:
                return f"mul({self.emit(b)}, {self.emit(a)})"
            if a.type.is_vector and b.type.is_matrix:
                return f"mul({self.emit(b)}, {self.emit(a)})"
        if (
            e.text == "*"
            and UNKNOWN in (a.type, b.type)
            and (a.type.is_matrix or b.type.is_matrix)
        ):
            raise ConversionError("a matrix product with an operand of unknown type")
        if e.text == "^^":
            return f"({self.emit(a)} != {self.emit(b)})"
        return f"{self.emit(a)} {e.text} {self.emit(b)}"

    def emit_call(self, e: Expr) -> str:
        name = e.text
        if name in ATOMIC_METHODS and e.args and self.atomic_target(e.args[0]):
            rest = ", ".join(self.emit(a) for a in e.args[1:])
            return f"{self.emit_raw(e.args[0])}.{ATOMIC_METHODS[name]}({rest})"
        args = [self.emit(a) for a in e.args]
        if name in TEXTURE_FUNCTIONS:
            return self.emit_texture_call(e, args)
        if name in UNCONVERTED_FUNCTIONS:
            raise ConversionError(f"{name}() has no conversion yet")
        if name in COMPARISONS:
            return f"({args[0]} {COMPARISONS[name]} {args[1]})"
        if name == "not":
            return f"!({args[0]})"
        if name == "atan" and len(args) == 2:
            return f"atan2({', '.join(args)})"
        if name == "mod":
            self.helpers.add("mod")
            return f"glsl_mod({', '.join(args)})"
        if name == "matrixCompMult":
            return f"({args[0]} * {args[1]})"
        constructed = parse_type_name(name)
        if constructed is not None:
            if constructed.is_matrix and len(e.args) == 1 and e.args[0].type.is_matrix:
                return f"(({slang_type_name(name)}){args[0]})"
            return f"{slang_type_name(name)}({', '.join(args)})"
        return f"{RENAMED_FUNCTIONS.get(name, name)}({', '.join(args)})"

    def emit_texture_call(self, e: Expr, args: list[str]) -> str:
        sampler = e.args[0].text if e.args and e.args[0].kind == "id" else ""
        if sampler not in self.textures:
            raise ConversionError(
                f"{e.text}() of something other than a sampler2D uniform"
            )
        macro, arity = TEXTURE_FUNCTIONS[e.text]
        if len(args) != arity:
            raise ConversionError(
                f"{e.text}() with {len(args)} arguments is not supported"
            )
        return f"{macro}({', '.join([sampler] + args[1:])})"


def split_array(declarator: str) -> tuple[str, str]:
    m = re.fullmatch(r"(\w+)\s*(\[.*\])?", declarator)
    if not m:
        return declarator, ""
    return m.group(1), m.group(2) or ""


# ---------------------------------------------------------------------------------------------
# Declarations


def significant(tokens: list[Token]) -> list[Token]:
    return [t for t in tokens if t.kind not in ("ws", "comment")]


def parse_layout(tokens: list[Token]) -> dict[str, str]:
    """The key=value pairs of layout(...) tokens (without the parentheses)."""
    layout: dict[str, str] = {}
    text = "".join(t.text for t in tokens if t.kind not in ("ws", "comment"))
    for item in text.split(","):
        if "=" in item:
            key, value = item.split("=", 1)
            layout[key.strip()] = value.strip()
        elif item.strip():
            layout[item.strip()] = ""
    return layout


@dataclass
class Declaration:
    tokens: list[Token]  # the whole declaration, including its terminating ';' or '}'
    start: int
    end: int


def split_top_level(tokens: list[Token]) -> list[Declaration]:
    """Splits a translation unit into top-level declarations, functions and preprocessor lines.

    A function ends at the brace closing its body; a struct or block ends at the ';' after it.
    """
    declarations: list[Declaration] = []
    i = 0
    n = len(tokens)
    while i < n and tokens[i].kind != "eof":
        start = i
        if tokens[i].kind in ("ws", "comment", "pp"):
            declarations.append(Declaration(tokens[i : i + 1], i, i + 1))
            i += 1
            continue
        depth = 0
        is_function = False
        previous: Token | None = None
        while i < n and tokens[i].kind != "eof":
            t = tokens[i]
            i += 1
            if t.kind == "op" and t.text in "([{":
                if t.text == "{" and depth == 0 and previous is not None:
                    is_function = previous.text == ")"
                depth += 1
            elif t.kind == "op" and t.text in ")]}":
                depth -= 1
                if depth == 0 and t.text == "}" and is_function:
                    break
            elif t.kind == "op" and t.text == ";" and depth == 0:
                break
            if depth == 0 and t.kind not in ("ws", "comment", "pp"):
                previous = t
        declarations.append(Declaration(tokens[start:i], start, i))
    return declarations


# ---------------------------------------------------------------------------------------------
# Conversion


class StageConverter:
    def __init__(self, converter: Converter, stage: Stage) -> None:
        self.c = converter
        self.stage = stage

    def run(self) -> None:
        tokens = tokenize(self.stage.source)
        for decl in split_top_level(tokens):
            self.declaration(decl)

    def declaration(self, decl: Declaration) -> None:
        sig = significant(decl.tokens)
        if self.passes_through(decl.tokens, sig):
            return
        layout, words, rest = self.qualifiers(sig)
        self.dispatch(decl.tokens, layout, words, rest)

    def passes_through(self, toks: list[Token], sig: list[Token]) -> bool:
        """Keeps or drops what declares nothing: trivia, preprocessor lines and precision."""
        if not sig:
            self.stage.body.extend(toks)
            return True
        if sig[0].kind == "pp":
            if not re.match(r"\s*#\s*(version|extension)\b", sig[0].text):
                self.stage.body.extend(toks)
            return True
        return sig[0].text == "precision"

    def qualifiers(
        self, sig: list[Token]
    ) -> tuple[dict[str, str], list[str], list[Token]]:
        """The layout qualifier, the other qualifiers, and the declaration after them."""
        layout: dict[str, str] = {}
        rest = sig
        if sig[0].text == "layout":
            close = next(i for i, t in enumerate(sig) if t.text == ")")
            layout = parse_layout(sig[2:close])
            rest = sig[close + 1 :]
        i = 0
        while (
            i < len(rest)
            and rest[i].kind == "id"
            and rest[i].text in DECLARATION_QUALIFIERS
        ):
            i += 1
        words = [t.text for t in rest[:i]]
        # GLSL ES 1.0 storage qualifiers.
        if "attribute" in words:
            words.append("in")
        if "varying" in words:
            words.append("out" if self.stage.name == "vertex" else "in")
        return layout, words, rest[i:]

    def dispatch(
        self,
        toks: list[Token],
        layout: dict[str, str],
        words: list[str],
        rest: list[Token],
    ) -> None:
        if "in" in words and (not rest or rest[0].text == ";"):
            if "local_size_x" in layout:
                self.stage.local_size = (
                    layout.get("local_size_x", "1"),
                    layout.get("local_size_y", "1"),
                    layout.get("local_size_z", "1"),
                )
        elif (
            ("uniform" in words or "buffer" in words)
            and len(rest) > 1
            and rest[1].text == "{"
        ):
            self.block(words, layout, rest)
        elif rest and rest[0].text == "struct":
            self.struct(toks, rest)
        elif "uniform" in words:
            self.uniform(layout, rest)
        elif "in" in words or "out" in words:
            self.interface(words, layout, rest)
        elif "shared" in words:
            name, array = declarator(rest[1:])
            self.c.shared.append(Variable(name, rest[0].text, array))
            self.c.globals[name] = self.c.resolve(rest[0].text, array)
        elif len(rest) >= 3 and rest[2].text == "(":
            self.function(toks)
        else:
            self.global_variable(toks, words, rest)

    def block(
        self, words: list[str], layout: dict[str, str], rest: list[Token]
    ) -> None:
        name = rest[0].text
        close = index_of(rest, "}")
        members = parse_members(rest[2:close])
        after = [t for t in rest[close + 1 :] if t.text != ";"]
        instance = after[0].text if after else ""
        kind = "buffer" if "buffer" in words else "uniform"
        binding = self.c.buffer_indices.get(instance or name)
        if binding is not None and "binding" in layout:
            glsl_binding = (int(layout.get("set", "0")), int(layout["binding"]))
            self.c.remaps[glsl_binding] = (1, binding)
        if binding is None and "binding" in layout:
            binding = int(layout["binding"])
        if binding is None:
            raise ConversionError(
                f"{kind} block {name} has no binding; pass --buffer {instance or name}=INDEX"
            )
        block = Block(kind, name, instance, members, binding, "readonly" in words)
        existing = next((b for b in self.c.blocks if b.name == name), None)
        if existing is not None:
            if (existing.kind, existing.binding, existing.members) != (
                kind,
                binding,
                members,
            ):
                raise ConversionError(f"block {name} differs between stages")
            block = existing
        else:
            self.c.blocks.append(block)
        if kind == "buffer":
            arrays = [m for m in members if m.array == "[]"]
            if len(members) != 1 or not arrays:
                raise ConversionError(
                    f"storage block {name}: only a block holding one runtime-sized array converts"
                )
            buffer_name = instance or members[0].name
            if instance:
                self.c.storage_arrays[f"{instance}.{members[0].name}"] = buffer_name
                self.c.globals[instance] = Type(name)
                self.c.structs[name] = {members[0].name: members[0].glsl_type + "[]"}
            else:
                self.c.globals[buffer_name] = self.c.resolve(members[0].glsl_type, "[]")
            return
        self.c.structs[name] = {m.name: m.glsl_type + m.array for m in members}
        if instance:
            self.c.globals[instance] = Type(name)
        else:
            block.instance = name[0].lower() + name[1:]
            if block.instance == name:
                block.instance += "_"
            for m in members:
                self.c.implicit_members[m.name] = f"{block.instance}.{m.name}"
                self.c.globals[m.name] = self.c.resolve(m.glsl_type, m.array)

    def struct(self, toks: list[Token], rest: list[Token]) -> None:
        """Structs are global: blocks may use them, and stages share them."""
        name = rest[1].text
        close = index_of(rest, "}")
        members = parse_members(rest[3:close])
        fields = {m.name: m.glsl_type + m.array for m in members}
        if name in self.c.struct_definitions:
            if self.c.structs[name] != fields:
                raise ConversionError(f"struct {name} differs between stages")
            return
        self.c.structs[name] = fields
        self.c.struct_definitions[name] = toks

    def uniform(self, layout: dict[str, str], rest: list[Token]) -> None:
        glsl_type = rest[0].text
        name, array = declarator(rest[1:])
        if glsl_type in SAMPLER_2D:
            unit = self.c.texture_units.get(name)
            if unit is not None and "binding" in layout:
                glsl_binding = (int(layout.get("set", "0")), int(layout["binding"]))
                self.c.remaps[glsl_binding] = (0, unit)
            if unit is None and "binding" in layout:
                unit = int(layout["binding"])
            if unit is None:
                raise ConversionError(
                    f"sampler {name} has no binding; pass --texture {name}=UNIT"
                )
            self.c.textures[name] = unit
            return
        if glsl_type in UNSUPPORTED_TYPES:
            raise ConversionError(
                f"{glsl_type} {name}: the IGL Slang ABI has 2D textures only"
            )
        if all(v.name != name for v in self.c.loose_uniforms):
            self.c.loose_uniforms.append(Variable(name, glsl_type, array))
        self.c.implicit_members[name] = f"uniforms.{name}"
        self.c.globals[name] = self.c.resolve(glsl_type, array)

    def interface(
        self, words: list[str], layout: dict[str, str], rest: list[Token]
    ) -> None:
        glsl_type = rest[0].text
        name, array = declarator(rest[1:])
        location = int(layout["location"]) if "location" in layout else None
        interpolation = ""
        if "flat" in words:
            interpolation = "nointerpolation"
        elif "noperspective" in words:
            interpolation = "noperspective"
        elif "centroid" in words:
            interpolation = "centroid"
        variable = Variable(name, glsl_type, array, location, interpolation)
        (self.stage.inputs if "in" in words else self.stage.outputs).append(variable)
        self.c.globals[name] = self.c.resolve(glsl_type, array)

    def global_variable(
        self, toks: list[Token], words: list[str], rest: list[Token]
    ) -> None:
        if rest:
            name, array = declarator(rest[1:])
            self.c.globals[name] = self.c.resolve(rest[0].text, array)
        self.stage.body.extend(toks)

    def function(self, toks: list[Token]) -> None:
        sig = significant(toks)
        return_type, name = sig[0].text, sig[1].text
        self.c.functions[name] = return_type
        self.stage.body.extend(toks)


def index_of(tokens: list[Token], text: str) -> int:
    depth = 0
    for i, t in enumerate(tokens):
        if t.text in "({[" and t.kind == "op":
            depth += 1
        if t.text in ")}]" and t.kind == "op":
            depth -= 1
            if depth < 0 and t.text == text:
                return i
        if t.text == text and depth == 0:
            return i
    raise ConversionError(f"missing {text!r}")


def declarator(tokens: list[Token]) -> tuple[str, str]:
    name = tokens[0].text
    array = ""
    if len(tokens) > 1 and tokens[1].text == "[":
        close = next((i for i, t in enumerate(tokens) if t.text == "]"), None)
        if close is None:
            raise ConversionError(
                f"line {tokens[0].line}: unclosed [ in the declaration of {name}"
            )
        array = "[" + "".join(t.text for t in tokens[2:close]) + "]"
    return name, array


def parse_members(tokens: list[Token]) -> list[Variable]:
    members: list[Variable] = []
    current: list[Token] = []
    for t in tokens:
        if t.text == ";":
            words = [w for w in current if w.text not in QUALIFIERS_DROPPED]
            if words and words[0].text == "layout":
                close = index_of(words, ")")
                words = words[close + 1 :]
            if len(words) >= 2:
                glsl_type = words[0].text
                rest = words[1:]
                # int a, b;
                parts: list[list[Token]] = [[]]
                for w in rest:
                    if w.text == ",":
                        parts.append([])
                    else:
                        parts[-1].append(w)
                for part in parts:
                    name, array = declarator(part)
                    members.append(Variable(name, glsl_type, array))
            current = []
        else:
            current.append(t)
    return members


# ---------------------------------------------------------------------------------------------
# Function bodies: statements, with expressions re-emitted


class BodyRewriter:
    """Re-emits code tokens, rewriting expressions and types, and keeping everything else."""

    def __init__(self, converter: Converter, stage: Stage) -> None:
        self.c = converter
        self.stage = stage

    def rewrite(self, tokens: list[Token]) -> str:
        self.tokens = tokens
        self.pos = 0
        out: list[str] = []
        while self.pos < len(tokens) and tokens[self.pos].kind != "eof":
            out.append(self.top_level())
        return "".join(out)

    def peek_sig(self, offset: int = 0) -> Token:
        i = self.pos
        seen = -1
        while i < len(self.tokens):
            if self.tokens[i].kind not in ("ws", "comment"):
                seen += 1
                if seen == offset:
                    return self.tokens[i]
            i += 1
        return Token("eof", "", 0)

    def take_trivia(self) -> str:
        out = []
        while self.pos < len(self.tokens) and self.tokens[self.pos].kind in (
            "ws",
            "comment",
        ):
            out.append(self.tokens[self.pos].text)
            self.pos += 1
        return "".join(out)

    def take(self) -> Token:
        token = self.tokens[self.pos]
        self.pos += 1
        return token

    def take_until(self, stops: set[str], depth_aware: bool = True) -> list[Token]:
        """Tokens up to (not including) a stop token at nesting depth 0."""
        depth = 0
        start = self.pos
        while self.pos < len(self.tokens) and self.tokens[self.pos].kind != "eof":
            t = self.tokens[self.pos]
            if t.kind == "op" and t.text in "([{" and depth_aware:
                depth += 1
            elif t.kind == "op" and t.text in ")]}" and depth_aware:
                if depth == 0 and t.text in stops:
                    break
                depth -= 1
            elif depth == 0 and t.text in stops and t.kind == "op":
                break
            self.pos += 1
        return self.tokens[start : self.pos]

    def expression(self, tokens: list[Token]) -> str:
        if not significant(tokens):
            return "".join(t.text for t in tokens)
        lead = ""
        trail = ""
        while tokens and tokens[0].kind in ("ws", "comment"):
            lead += tokens[0].text
            tokens = tokens[1:]
        while tokens and tokens[-1].kind in ("ws", "comment"):
            trail = tokens[-1].text + trail
            tokens = tokens[:-1]
        expr = ExprParser(tokens, self.c).parse()
        return lead + self.c.emit(expr) + trail

    def top_level(self) -> str:
        t = self.tokens[self.pos]
        if t.kind in ("ws", "comment"):
            self.pos += 1
            return t.text
        if t.kind == "pp":
            self.pos += 1
            return t.text
        sig = self.peek_sig(0)
        if sig.text == "struct":
            return self.struct()
        if self.peek_sig(2).text == "(" and self.peek_sig(1).kind == "id":
            return self.function()
        return self.declaration_statement(top=True)

    def struct(self) -> str:
        out = [self.take().text]  # struct
        out.append(self.take_trivia())
        out.append(self.take().text)  # name
        out.append(self.take_trivia())
        out.append(self.take().text)  # {
        while self.peek_sig(0).text != "}":
            out.append(self.take_trivia())
            member = self.take_until({";"})
            out.append(self.type_tokens(member))
            out.append(self.take().text)  # ;
        out.append(self.take_trivia())
        out.append(self.take().text)  # }
        out.append(self.take_trivia())
        out.append(self.take().text)  # ;
        return "".join(out)

    def type_tokens(self, tokens: list[Token]) -> str:
        out = []
        for t in tokens:
            if t.kind == "id" and t.text in QUALIFIERS_DROPPED:
                continue
            if t.kind == "id" and t.text == "flat":
                out.append("nointerpolation")
                continue
            out.append(slang_type_name(t.text) if t.kind == "id" else t.text)
        return "".join(out)

    def function(self) -> str:
        out: list[str] = []
        return_type = self.take().text
        name_trivia = self.take_trivia()
        name = self.take().text
        out.append(
            slang_type_name(return_type)
            + name_trivia
            + (self.stage.main_name if name == "main" else name)
        )
        out.append(self.take_trivia())
        out.append(self.take().text)  # (
        params = self.take_until({")"})
        self.c.scopes.append({})
        out.append(self.parameters(params))
        out.append(self.take().text)  # )
        out.append(self.take_trivia())
        if self.peek_sig(0).text == ";":
            out.append(self.take().text)
            self.c.scopes.pop()
            return "".join(out)
        out.append(self.block())
        self.c.scopes.pop()
        return "".join(out)

    def parameters(self, tokens: list[Token]) -> str:
        parts: list[list[Token]] = [[]]
        for t in tokens:
            if t.text == "," and t.kind == "op":
                parts.append([])
            else:
                parts[-1].append(t)
        out = []
        for part in parts:
            sig = [t for t in significant(part) if t.text not in QUALIFIERS_DROPPED]
            words = [
                t.text for t in sig if t.text not in ("in", "out", "inout", "const")
            ]
            if len(words) >= 2:
                name, array = split_array("".join(words[1:]))
                self.c.scopes[-1][name] = self.c.resolve(words[0], array)
            out.append(self.type_tokens(part))
        return ",".join(out)

    def block(self) -> str:
        out = [self.take().text]  # {
        self.c.scopes.append({})
        while self.peek_sig(0).text != "}":
            if self.peek_sig(0).kind == "eof":
                raise ConversionError("unterminated block")
            out.append(self.statement())
        out.append(self.take_trivia())
        out.append(self.take().text)  # }
        self.c.scopes.pop()
        return "".join(out)

    def statement(self) -> str:
        lead = self.take_trivia()
        t = self.tokens[self.pos]
        if t.kind == "pp":
            self.pos += 1
            return lead + t.text
        text = t.text
        if text == "{":
            return lead + self.block()
        if text in ("if", "while", "switch"):
            self.pos += 1
            head = self.take_trivia() + self.take().text  # (
            condition = self.take_until({")"})
            head += self.expression(condition) + self.take().text
            body = self.statement()
            result = lead + text + head + body
            if text == "if" and self.peek_sig(0).text == "else":
                result += self.take_trivia() + self.take().text + self.statement()
            return result
        if text == "for":
            self.pos += 1
            head = self.take_trivia() + self.take().text  # (
            self.c.scopes.append({})
            init = self.take_until({";"})
            head += self.declaration_or_expression(init) + self.take().text
            condition = self.take_until({";"})
            head += self.expression(condition) + self.take().text
            step = self.take_until({")"})
            head += self.expression(step) + self.take().text
            body = self.statement()
            self.c.scopes.pop()
            return lead + text + head + body
        if text == "do":
            self.pos += 1
            body = self.statement()
            tail = self.take_trivia() + self.take().text  # while
            tail += self.take_trivia() + self.take().text  # (
            condition = self.take_until({")"})
            tail += self.expression(condition) + self.take().text
            tail += self.take_trivia() + self.take().text  # ;
            return lead + "do" + body + tail
        if text in ("case", "default"):
            self.pos += 1
            label = self.take_until({":"}, depth_aware=False)
            return lead + text + self.expression(label) + self.take().text
        if text in ("break", "continue", "discard"):
            self.pos += 1
            return lead + text + self.take_trivia() + self.take().text
        if text == "return":
            self.pos += 1
            value = self.take_until({";"})
            return lead + "return" + self.expression(value) + self.take().text
        return lead + self.declaration_statement(top=False)

    def is_declaration(self, sig: list[Token]) -> bool:
        words = [
            t for t in sig if t.text not in QUALIFIERS_DROPPED and t.text != "const"
        ]
        if len(words) < 2 or words[0].kind != "id" or words[1].kind != "id":
            return False
        return (
            parse_type_name(words[0].text) is not None
            or words[0].text in self.c.structs
        )

    def declaration_or_expression(self, tokens: list[Token]) -> str:
        if self.is_declaration(significant(tokens)):
            return self.declaration(tokens)
        return self.expression(tokens)

    def declaration_statement(self, top: bool) -> str:
        tokens = self.take_until({";"})
        end = self.take().text if self.pos < len(self.tokens) else ""
        return self.declaration_or_expression(tokens) + end

    def declaration(self, tokens: list[Token]) -> str:
        """`T a = x, b[2];` with types renamed, names declared and initializers rewritten."""
        out = []
        i = 0
        # Qualifiers and the type.
        while i < len(tokens):
            t = tokens[i]
            i += 1
            if t.kind in ("ws", "comment"):
                out.append(t.text)
                continue
            if t.text in QUALIFIERS_DROPPED:
                continue
            if t.text == "const":
                out.append("const")
                continue
            glsl_type = t.text
            out.append(slang_type_name(glsl_type))
            break
        # Declarators.
        declarators: list[list[Token]] = [[]]
        depth = 0
        for t in tokens[i:]:
            if t.kind == "op" and t.text in "([{":
                depth += 1
            elif t.kind == "op" and t.text in ")]}":
                depth -= 1
            if t.text == "," and depth == 0:
                declarators.append([])
            else:
                declarators[-1].append(t)
        rendered = []
        for d in declarators:
            sig = significant(d)
            name = sig[0].text
            eq = next(
                (k for k, t in enumerate(d) if t.text == "=" and t.kind == "op"), None
            )
            head = d if eq is None else d[:eq]
            array = ""
            head_sig = significant(head)
            if len(head_sig) > 1 and head_sig[1].text == "[":
                array = "[" + "".join(t.text for t in head_sig[2:-1]) + "]"
            scope = self.c.scopes[-1] if self.c.scopes else self.c.globals
            scope[name] = self.c.resolve(glsl_type, array)
            text = "".join(t.text for t in head)
            if eq is not None:
                text += "=" + self.expression(d[eq + 1 :])
            rendered.append(text)
        return "".join(out) + ",".join(rendered)


# ---------------------------------------------------------------------------------------------
# Output


def vertex_semantic(name: str, used: dict[str, int]) -> str:
    """A vertex input's semantic from its name; the D3D12 backend matches attributes by it."""
    semantic = next(
        (sem for pattern, sem in SEMANTICS if pattern.search(name)), "TEXCOORD"
    )
    index = used.get(semantic, 0)
    used[semantic] = index + 1
    return f"{semantic}{index}" if index else semantic


def field_line(v: Variable, semantic: str, location: int | None) -> str:
    attrs = f"[[vk::location({location})]] " if location is not None else ""
    interpolation = f"{v.interpolation} " if v.interpolation else ""
    type_name = slang_type_name(v.glsl_type)
    return f"  {attrs}{interpolation}{type_name} {v.name}{v.array} : {semantic};"


def builtin_name(name: str) -> str:
    """Stage built-ins become statics; Slang reserves nothing named gl_*, but keep clear of it."""
    return "glsl_" + name[3:]


def emit_program(converter: Converter, stages: list[Stage]) -> str:
    bodies = [emit_stage(converter, stage) for stage in stages]
    out: list[str] = [
        "// Converted from GLSL by tools/slang/glsl2slang.py; review before use.",
        '#include "igl_abi.slangh"',
        "",
    ]
    if "mod" in converter.helpers:
        out += ["#define glsl_mod(x, y) ((x) - (y) * floor((x) / (y)))", ""]
    for tokens in converter.struct_definitions.values():
        out += [BodyRewriter(converter, stages[0]).rewrite(tokens).strip(), ""]
    out += emit_textures(converter) + emit_uniform_buffers(converter)
    out += emit_storage_buffers(converter, stages)
    out += [
        f"groupshared {slang_type_name(v.glsl_type)} {v.name}{v.array};"
        for v in converter.shared
    ]
    return "\n".join(out + [""] + bodies).rstrip() + "\n"


def emit_textures(converter: Converter) -> list[str]:
    units = sorted(converter.textures.items(), key=lambda kv: kv[1])
    lines = [f"IGL_TEXTURE2D({name}, {unit});" for name, unit in units]
    return lines + [""] if lines else []


def emit_uniform_buffers(converter: Converter) -> list[str]:
    out: list[str] = []
    if converter.loose_uniforms:
        out.append("struct Uniforms {")
        out += [
            f"  {slang_type_name(v.glsl_type)} {v.name}{v.array};"
            for v in converter.loose_uniforms
        ]
        index = converter.uniforms_index
        out += ["};", f"IGL_UNIFORM_BUFFER(Uniforms, uniforms, {index});", ""]
    for b in converter.blocks:
        if b.kind == "uniform":
            out.append(f"struct {b.name} {{")
            out += [
                f"  {slang_type_name(m.glsl_type)} {m.name}{m.array};"
                for m in b.members
            ]
            out += [
                "};",
                f"IGL_UNIFORM_BUFFER({b.name}, {b.instance}, {b.binding});",
                "",
            ]
    return out


def emit_storage_buffers(converter: Converter, stages: list[Stage]) -> list[str]:
    out: list[str] = []
    graphics = any(stage.name != "compute" for stage in stages)
    for b in converter.blocks:
        if b.kind != "buffer":
            continue
        element = slang_type_name(b.members[0].glsl_type)
        name = b.instance or b.members[0].name
        if name in converter.atomic_buffers:
            element = f"Atomic<{element}>"
        # D3D12 binds read-only buffers as t registers next to textures, and compute buffers as
        # dense UAVs; only graphics stages need the read-only declaration (on Vulkan).
        if b.readonly and graphics:
            out.append("// Read-only: high-end programs only (see igl_abi.slangh).")
            out += [f"IGL_READONLY_STORAGE_BUFFER({element}, {name}, {b.binding});", ""]
        else:
            out += [f"IGL_STORAGE_BUFFER({element}, {name}, {b.binding});", ""]
    return out


def stage_statics(
    stage: Stage, builtins: dict[str, tuple[str, str]], used: set[str]
) -> list[str]:
    statics = [
        f"static {slang_type_name(v.glsl_type)} {v.name}{v.array};"
        for v in stage.inputs + stage.outputs
    ]
    writable = {
        "vertex": {"gl_Position", "gl_PointSize"},
        "fragment": {"gl_FragColor", "gl_FragDepth"},
    }
    for b in sorted(used):
        if b in builtins:
            statics.append(f"static {builtins[b][0]} {builtin_name(b)};")
        elif b in writable.get(stage.name, set()):
            type_name = "float4" if b in ("gl_Position", "gl_FragColor") else "float"
            statics.append(f"static {type_name} {builtin_name(b)};")
        else:
            raise ConversionError(f"{stage.name} stage: {b} has no conversion")
    return statics


def emit_compute_entry(
    stage: Stage, builtins: dict[str, tuple[str, str]], used: set[str]
) -> list[str]:
    params = [f"{t} {b}_ : {sem}" for b, (t, sem) in builtins.items() if b in used]
    x, y, z = stage.local_size
    lines = ['[shader("compute")]', f"[numthreads({x}, {y}, {z})]"]
    lines.append(f"void {ENTRY_NAMES['compute']}({', '.join(params)}) {{")
    q = f"{stage.namespace}::" if stage.namespace else ""
    lines += [f"  {q}{builtin_name(b)} = {b}_;" for b in builtins if b in used]
    return lines + [f"  {q}{stage.main_name}();", "}", ""]


def emit_graphics_entry(
    stage: Stage, builtins: dict[str, tuple[str, str]], used: set[str]
) -> list[str]:
    prefix = stage.name.capitalize()
    lines: list[str] = []
    attribute_semantics: dict[str, int] = {}
    in_fields = [
        field_line(
            v,
            vertex_semantic(v.name, attribute_semantics)
            if stage.name == "vertex"
            else f"TEXCOORD{v.location}",
            v.location,
        )
        for v in stage.inputs
    ]
    if in_fields:
        lines += [f"struct {prefix}Input {{"] + in_fields + ["};", ""]
    if stage.name == "vertex":
        out_fields = ["  float4 glsl_Position : SV_Position;"]
        out_fields += [
            field_line(v, f"TEXCOORD{v.location}", v.location) for v in stage.outputs
        ]
    else:
        out_fields = [
            f"  {slang_type_name(v.glsl_type)} {v.name} : SV_Target{v.location or 0};"
            for v in stage.outputs
        ]
        if "gl_FragColor" in used:
            if stage.outputs:
                raise ConversionError(
                    "the fragment shader writes gl_FragColor and declares outputs; GLSL forbids mixing them"
                )
            out_fields.append("  float4 glsl_FragColor : SV_Target0;")
        if "gl_FragDepth" in used:
            out_fields.append("  float glsl_FragDepth : SV_Depth;")
    lines += [f"struct {prefix}Output {{"] + out_fields + ["};", ""]
    params = [f"{prefix}Input input"] if in_fields else []
    params += [f"{t} {b}_ : {sem}" for b, (t, sem) in builtins.items() if b in used]
    lines.append(f'[shader("{stage.name}")]')
    lines.append(f"{prefix}Output {ENTRY_NAMES[stage.name]}({', '.join(params)}) {{")
    q = f"{stage.namespace}::" if stage.namespace else ""
    lines += [f"  {q}{v.name} = input.{v.name};" for v in stage.inputs]
    lines += [f"  {q}{builtin_name(b)} = {b}_;" for b in builtins if b in used]
    lines += [f"  {q}{stage.main_name}();", f"  {prefix}Output output;"]
    if stage.name == "vertex":
        lines.append(f"  output.glsl_Position = {q}glsl_Position;")
    lines += [f"  output.{v.name} = {q}{v.name};" for v in stage.outputs]
    for b in ("gl_FragColor", "gl_FragDepth"):
        if b in used and stage.name == "fragment":
            lines.append(f"  output.{builtin_name(b)} = {q}{builtin_name(b)};")
    return lines + ["  return output;", "}", ""]


def emit_stage(converter: Converter, stage: Stage) -> str:
    builtins = {
        "vertex": VERTEX_BUILTINS,
        "fragment": FRAGMENT_BUILTINS,
        "compute": COMPUTE_BUILTINS,
    }[stage.name]
    text = "".join(t.text for t in stage.body)
    used = set(re.findall(r"\bgl_\w+", text))
    lines = [f"// {stage.name} stage", ""]
    if stage.namespace:
        lines += [f"namespace {stage.namespace} {{", ""]
    lines += stage_statics(stage, builtins, used) + [""]
    lines.append(BodyRewriter(converter, stage).rewrite(stage.body).strip("\n"))
    lines.append("")
    if stage.namespace:
        lines += [f"}} // namespace {stage.namespace}", ""]
    if stage.name == "compute":
        lines += emit_compute_entry(stage, builtins, used)
    else:
        lines += emit_graphics_entry(stage, builtins, used)
    return "\n".join(lines)


def find_atomic_buffers(converter: Converter, stage: Stage) -> None:
    text = "".join(t.text for t in stage.body if t.kind != "comment")
    for _, name, member in ATOMIC_CALL_RE.findall(text):
        buffer = converter.storage_arrays.get(f"{name}.{member}") if member else name
        if buffer is not None and any(
            b.kind == "buffer" and (b.instance or b.members[0].name) == buffer
            for b in converter.blocks
        ):
            converter.atomic_buffers.add(buffer)


def assign_locations(stage: Stage) -> None:
    """Gives every stage input and output a location; GLSL ES 3.0 may leave them implicit."""
    for variables in (stage.inputs, stage.outputs):
        taken = {v.location for v in variables if v.location is not None}
        next_location = 0
        for v in variables:
            if v.location is None:
                while next_location in taken:
                    next_location += 1
                v.location = next_location
                taken.add(next_location)


def check_varyings(vertex: Stage, fragment: Stage) -> None:
    outputs = {v.location: v for v in vertex.outputs}
    for v in fragment.inputs:
        match = outputs.get(v.location)
        if match is None or match.glsl_type != v.glsl_type:
            raise ConversionError(
                f"fragment input {v.name} (location {v.location}) has no matching vertex output"
            )


def convert(
    sources: dict[str, str],
    textures: dict[str, int] | None = None,
    buffers: dict[str, int] | None = None,
    uniforms_index: int = 1,
) -> str:
    """Converts GLSL stages ({"vertex": src, "fragment": src} or {"compute": src}) to Slang."""
    return convert_details(sources, textures, buffers, uniforms_index)[0]


def convert_details(
    sources: dict[str, str],
    textures: dict[str, int] | None = None,
    buffers: dict[str, int] | None = None,
    uniforms_index: int = 1,
) -> tuple[str, Converter]:
    converter = Converter(textures or {}, buffers or {}, uniforms_index)
    stages = []
    for name in ("vertex", "fragment", "compute"):
        if name in sources:
            stage = Stage(name, sources[name])
            if len(sources) > 1:
                stage.namespace = f"{name}_glsl"
            StageConverter(converter, stage).run()
            assign_locations(stage)
            find_atomic_buffers(converter, stage)
            stages.append(stage)
    if not stages:
        raise ConversionError("no stages to convert")
    if "vertex" in sources and "fragment" in sources:
        check_varyings(stages[0], stages[1])
    has_vertex_inputs = any(s.inputs for s in stages if s.name == "vertex")
    if has_vertex_inputs and any(b.binding == 0 for b in converter.blocks):
        print(
            "glsl2slang: warning: a buffer at IGL index 0 shares Metal's index with the vertex "
            "stream; move it with --buffer NAME=INDEX",
            file=sys.stderr,
        )
    return emit_program(converter, stages), converter


# ---------------------------------------------------------------------------------------------
# Verification


@dataclass
class VerifyTools:
    glslang: str
    slangc: str
    spirv_cross: str
    abi_dir: str


def reflect(spirv_cross: str, spv: str) -> dict:
    result = subprocess.run(
        [spirv_cross, spv, "--reflect"], capture_output=True, text=True
    )
    if result.returncode != 0:
        raise ConversionError(f"verify: spirv-cross --reflect failed:\n{result.stderr}")
    return json.loads(result.stdout)


def interface_summary(reflection: dict) -> dict[str, object]:
    """What must match between the GLSL and the Slang module: stage I/O and resources."""

    def io(key: str) -> dict[int, str]:
        return {
            v["location"]: v["type"] for v in reflection.get(key, []) if "location" in v
        }

    def binding(resource: dict) -> tuple[int, int]:
        return resource.get("set", 0), resource.get("binding", 0)

    return {
        "inputs": io("inputs"),
        "outputs": io("outputs"),
        "uniform buffers (set, binding) -> size": {
            binding(b): b.get("block_size", 0) for b in reflection.get("ubos", [])
        },
        "storage buffers (set, binding)": sorted(
            binding(b) for b in reflection.get("ssbos", [])
        ),
        "samplers (set, binding) -> type": {
            binding(t): t["type"] for t in reflection.get("textures", [])
        },
    }


def remap_summary(
    summary: dict[str, object], remaps: dict[tuple[int, int], tuple[int, int]]
) -> None:
    for key, value in summary.items():
        if isinstance(value, dict) and "binding" in key:
            summary[key] = {remaps.get(b, b): v for b, v in value.items()}
        elif isinstance(value, list):
            summary[key] = sorted(remaps.get(b, b) for b in value)


def verify(
    program: str,
    sources: dict[str, str],
    tools: VerifyTools,
    remaps: dict[tuple[int, int], tuple[int, int]],
) -> list[str]:
    """Compiles each GLSL stage and the matching Slang entry point to SPIR-V and compares them."""
    problems: list[str] = []
    with tempfile.TemporaryDirectory() as tmp:
        slang_path = os.path.join(tmp, "program.slang")
        with open(slang_path, "w") as f:
            f.write(program)
        for stage, source in sources.items():
            glsl_path = os.path.join(tmp, f"shader.{STAGE_EXTENSIONS[stage]}")
            with open(glsl_path, "w") as f:
                f.write(
                    source
                    if source.lstrip().startswith("#version")
                    else "#version 450\n" + source
                )
            glsl_spv = os.path.join(tmp, f"{stage}.glsl.spv")
            slang_spv = os.path.join(tmp, f"{stage}.slang.spv")
            steps = [
                [
                    tools.glslang,
                    "-V",
                    "--target-env",
                    "vulkan1.1",
                    "--auto-map-locations",
                    glsl_path,
                    "-o",
                    glsl_spv,
                ],
                [
                    tools.slangc,
                    slang_path,
                    "-I",
                    tools.abi_dir,
                    "-D",
                    "IGL_TARGET_SPIRV",
                    "-matrix-layout-row-major",
                    "-target",
                    "spirv",
                    "-profile",
                    "spirv_1_3",
                    "-entry",
                    ENTRY_NAMES[stage],
                    "-stage",
                    stage,
                    "-o",
                    slang_spv,
                ],
            ]
            failed = False
            for step in steps:
                result = subprocess.run(step, capture_output=True, text=True)
                if result.returncode != 0 and step[0] == tools.glslang:
                    problems.append(
                        f"{stage}: glslang cannot compile the GLSL for Vulkan, so there is nothing"
                        " to compare with (GLSL ES 1.0, loose uniforms and unbound samplers have no"
                        " Vulkan form); check the Slang output by rendering it instead"
                    )
                    failed = True
                    break
                if result.returncode != 0:
                    problems.append(
                        f"{stage}: {os.path.basename(step[0])} failed:\n{result.stdout}{result.stderr}"
                    )
                    failed = True
            if failed:
                continue
            expected = interface_summary(reflect(tools.spirv_cross, glsl_spv))
            remap_summary(expected, remaps)
            actual = interface_summary(reflect(tools.spirv_cross, slang_spv))
            for key, value in expected.items():
                if actual[key] != value:
                    problems.append(
                        f"{stage}: {key}: GLSL {value}, Slang {actual[key]}"
                    )
    return problems


STAGE_EXTENSIONS = {"vertex": "vert", "fragment": "frag", "compute": "comp"}


def parse_assignments(items: list[str], what: str) -> dict[str, int]:
    result = {}
    for item in items:
        name, _, value = item.partition("=")
        if not name or not value.isdigit():
            raise SystemExit(f"--{what} expects NAME=INDEX, got {item!r}")
        result[name] = int(value)
    return result


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--vertex")
    parser.add_argument("--fragment")
    parser.add_argument("--compute")
    parser.add_argument("-o", "--output")
    parser.add_argument("--texture", action="append", default=[])
    parser.add_argument("--buffer", action="append", default=[])
    parser.add_argument("--uniforms-index", type=int, default=1)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="compare the stage interface and resources of the GLSL and the Slang output",
    )
    parser.add_argument("--glslang", default="glslangValidator")
    parser.add_argument("--slangc", default="slangc")
    parser.add_argument("--spirv-cross", default="spirv-cross")
    args = parser.parse_args(argv)
    sources = {}
    for stage in ("vertex", "fragment", "compute"):
        path = getattr(args, stage)
        if path:
            with open(path) as f:
                sources[stage] = f.read()
    try:
        program, converter = convert_details(
            sources,
            parse_assignments(args.texture, "texture"),
            parse_assignments(args.buffer, "buffer"),
            args.uniforms_index,
        )
    except ConversionError as e:
        print(f"glsl2slang: {e}", file=sys.stderr)
        return 1
    if args.output:
        with open(args.output, "w") as f:
            f.write(program)
    else:
        sys.stdout.write(program)
    if args.verify:
        tools = VerifyTools(
            args.glslang,
            args.slangc,
            args.spirv_cross,
            os.path.dirname(os.path.abspath(__file__)),
        )
        problems = verify(program, sources, tools, converter.remaps)
        for problem in problems:
            print(f"glsl2slang: verify: {problem}", file=sys.stderr)
        if problems:
            return 1
        print(
            "glsl2slang: verify: stage interfaces and resources match", file=sys.stderr
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
