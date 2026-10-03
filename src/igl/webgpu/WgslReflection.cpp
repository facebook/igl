/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/WgslReflection.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace igl::webgpu {

namespace {

enum class TokenKind : uint8_t { Ident, Number, Punct, End };

struct Token {
  TokenKind kind = TokenKind::End;
  std::string_view text;
  uint32_t line = 1;
};

bool isIdentStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isDigit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

Result tokenize(std::string_view src, std::vector<Token>& out) {
  uint32_t line = 1;
  size_t i = 0;
  while (i < src.size()) {
    const char c = src[i];
    if (c == '\n') {
      ++line;
      ++i;
    } else if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      ++i;
    } else if (src.substr(i, 2) == "//") {
      while (i < src.size() && src[i] != '\n') {
        ++i;
      }
    } else if (src.substr(i, 2) == "/*") {
      // Block comments nest in WGSL.
      int depth = 0;
      do {
        if (src.substr(i, 2) == "/*") {
          ++depth;
          i += 2;
        } else if (src.substr(i, 2) == "*/") {
          --depth;
          i += 2;
        } else {
          line += src[i] == '\n' ? 1 : 0;
          ++i;
        }
      } while (depth > 0 && i < src.size());
      if (depth > 0) {
        return Result(Result::Code::ArgumentInvalid, "Unterminated block comment");
      }
    } else if (isIdentStart(c)) {
      const size_t begin = i;
      while (i < src.size() && isIdentChar(src[i])) {
        ++i;
      }
      out.push_back({TokenKind::Ident, src.substr(begin, i - begin), line});
    } else if (isDigit(c) || (c == '.' && i + 1 < src.size() && isDigit(src[i + 1]))) {
      const size_t begin = i;
      while (i < src.size()) {
        const char d = src[i];
        const bool exponentSign =
            (d == '+' || d == '-') && i > begin &&
            (src[i - 1] == 'e' || src[i - 1] == 'E' || src[i - 1] == 'p' || src[i - 1] == 'P');
        if (!isIdentChar(d) && d != '.' && !exponentSign) {
          break;
        }
        ++i;
      }
      out.push_back({TokenKind::Number, src.substr(begin, i - begin), line});
    } else if (src.substr(i, 2) == "->") {
      out.push_back({TokenKind::Punct, src.substr(i, 2), line});
      i += 2;
    } else {
      out.push_back({TokenKind::Punct, src.substr(i, 1), line});
      ++i;
    }
  }
  out.push_back({TokenKind::End, {}, line});
  return Result();
}

std::optional<uint32_t> parseInteger(std::string_view text) {
  std::string digits(text);
  while (!digits.empty() && (digits.back() == 'i' || digits.back() == 'u')) {
    digits.pop_back();
  }
  if (digits.empty() || digits.find_first_of(".pP") != std::string::npos ||
      (digits.find_first_of("eE") != std::string::npos && digits.rfind("0x", 0) != 0)) {
    return std::nullopt;
  }
  char* end = nullptr;
  const unsigned long long value = std::strtoull(digits.c_str(), &end, 0);
  if (end == nullptr || *end != '\0' || value > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(value);
}

/// A type or template argument: `name<args...>`, or a bare identifier or number.
struct TypeNode {
  std::string name;
  std::vector<TypeNode> args;
  bool isNumber = false;
};

// Recursion depth is bounded by the nesting in the source.
// NOLINTNEXTLINE(misc-no-recursion)
std::string toString(const TypeNode& type) {
  std::string result = type.name;
  if (!type.args.empty()) {
    result += '<';
    for (size_t i = 0; i < type.args.size(); ++i) {
      result += (i == 0 ? "" : ", ") + toString(type.args[i]);
    }
    result += '>';
  }
  return result;
}

struct Attribute {
  std::string_view name;
  // Argument token ranges, split at top-level commas.
  std::vector<std::vector<Token>> args;
};

struct RawMember {
  std::string name;
  TypeNode type;
  std::optional<uint32_t> align;
  std::optional<uint32_t> size;
};

struct RawStruct {
  std::string name;
  std::vector<RawMember> members;
};

struct RawBinding {
  std::string name;
  uint32_t group = 0;
  uint32_t binding = 0;
  std::string addressSpace;
  std::string access;
  TypeNode type;
  uint32_t line = 0;
};

struct Layout {
  uint32_t size = 0;
  uint32_t align = 0;
  UniformType uniformType = UniformType::Invalid;
  uint32_t arrayLength = 1;
  uint32_t arrayStride = 0;
};

uint32_t roundUp(uint32_t value, uint32_t alignment) {
  return alignment == 0 ? value : (value + alignment - 1) / alignment * alignment;
}

class Parser {
 public:
  explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

  Result parse(WgslReflection& out) {
    while (peek().kind != TokenKind::End) {
      Result result = parseDeclaration(out);
      if (!result.isOk()) {
        return result;
      }
    }
    return finish(out);
  }

 private:
  [[nodiscard]] const Token& peek(size_t ahead = 0) const {
    return tokens_[std::min(pos_ + ahead, tokens_.size() - 1)];
  }
  const Token& next() {
    const Token& token = peek();
    pos_ = std::min(pos_ + 1, tokens_.size() - 1);
    return token;
  }
  [[nodiscard]] bool isPunct(std::string_view text, size_t ahead = 0) const {
    return peek(ahead).kind == TokenKind::Punct && peek(ahead).text == text;
  }
  [[nodiscard]] bool isIdent(std::string_view text) const {
    return peek().kind == TokenKind::Ident && peek().text == text;
  }
  [[nodiscard]] Result error(const std::string& message) const {
    return Result(Result::Code::ArgumentInvalid,
                  "WGSL reflection, line " + std::to_string(peek().line) + ": " + message);
  }
  Result expectPunct(std::string_view text) {
    if (!isPunct(text)) {
      return error("expected '" + std::string(text) + "'");
    }
    next();
    return Result();
  }
  Result expectIdent(std::string& out) {
    if (peek().kind != TokenKind::Ident) {
      return error("expected an identifier");
    }
    out = std::string(next().text);
    return Result();
  }

  // Skips a balanced token run up to (and including) `close`, which must follow at depth 0.
  Result skipPast(std::string_view close) {
    int depth = 0;
    while (peek().kind != TokenKind::End) {
      const Token& token = next();
      if (token.kind != TokenKind::Punct) {
        continue;
      }
      if (depth == 0 && token.text == close) {
        return Result();
      }
      if (token.text == "(" || token.text == "[" || token.text == "{") {
        ++depth;
      } else if (token.text == ")" || token.text == "]" || token.text == "}") {
        --depth;
      }
    }
    return error("expected '" + std::string(close) + "'");
  }

  Result parseAttributes(std::vector<Attribute>& out) {
    while (isPunct("@")) {
      next();
      Attribute attribute;
      if (peek().kind != TokenKind::Ident) {
        return error("expected an attribute name");
      }
      attribute.name = next().text;
      if (isPunct("(")) {
        next();
        std::vector<Token> arg;
        int depth = 0;
        while (true) {
          if (peek().kind == TokenKind::End) {
            return error("unterminated attribute");
          }
          const Token token = next();
          if (token.kind == TokenKind::Punct && depth == 0 &&
              (token.text == ")" || token.text == ",")) {
            if (!arg.empty()) {
              attribute.args.push_back(std::move(arg));
              arg.clear();
            }
            if (token.text == ")") {
              break;
            }
            continue;
          }
          if (token.kind == TokenKind::Punct && token.text == "(") {
            ++depth;
          } else if (token.kind == TokenKind::Punct && token.text == ")") {
            --depth;
          }
          arg.push_back(token);
        }
      }
      out.push_back(std::move(attribute));
    }
    return Result();
  }

  // A single integer literal or `const` identifier; std::nullopt for anything else.
  [[nodiscard]] std::optional<uint32_t> evalInteger(const std::vector<Token>& tokens) const {
    if (tokens.size() != 1) {
      return std::nullopt;
    }
    if (tokens[0].kind == TokenKind::Number) {
      return parseInteger(tokens[0].text);
    }
    if (const auto it = consts_.find(std::string(tokens[0].text)); it != consts_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<uint32_t> attributeInteger(const std::vector<Attribute>& attributes,
                                                         std::string_view name) const {
    for (const Attribute& attribute : attributes) {
      if (attribute.name == name && attribute.args.size() == 1) {
        return evalInteger(attribute.args[0]);
      }
    }
    return std::nullopt;
  }

  static bool hasAttribute(const std::vector<Attribute>& attributes, std::string_view name) {
    return std::any_of(attributes.begin(), attributes.end(), [name](const Attribute& a) {
      return a.name == name;
    });
  }

  // Recursion depth is bounded by the nesting in the source.
  // NOLINTNEXTLINE(misc-no-recursion)
  Result parseType(TypeNode& out) {
    if (peek().kind == TokenKind::Number) {
      out.name = std::string(next().text);
      out.isNumber = true;
      return Result();
    }
    if (peek().kind != TokenKind::Ident) {
      return error("expected a type");
    }
    out.name = std::string(next().text);
    if (!isPunct("<")) {
      return Result();
    }
    next();
    while (!isPunct(">")) {
      TypeNode arg;
      Result result = parseType(arg);
      if (!result.isOk()) {
        return result;
      }
      out.args.push_back(std::move(arg));
      if (isPunct(",")) {
        next();
      } else if (!isPunct(">")) {
        return error("expected ',' or '>' in a template argument list");
      }
    }
    next();
    return Result();
  }

  Result parseDeclaration(WgslReflection& out) {
    std::vector<Attribute> attributes;
    Result result = parseAttributes(attributes);
    if (!result.isOk()) {
      return result;
    }
    if (isPunct(";")) {
      next();
      return Result();
    }
    if (isIdent("struct")) {
      return parseStruct();
    }
    if (isIdent("var")) {
      return parseVar(attributes);
    }
    if (isIdent("override")) {
      return parseOverride(attributes, out);
    }
    if (isIdent("const")) {
      return parseConst();
    }
    if (isIdent("alias")) {
      next();
      std::string name;
      TypeNode type;
      result = expectIdent(name);
      if (result.isOk()) {
        result = expectPunct("=");
      }
      if (result.isOk()) {
        result = parseType(type);
      }
      if (!result.isOk()) {
        return result;
      }
      aliases_[name] = std::move(type);
      return expectPunct(";");
    }
    if (isIdent("fn")) {
      return parseFunction(attributes, out);
    }
    if (isIdent("enable") || isIdent("requires") || isIdent("diagnostic") ||
        isIdent("const_assert")) {
      return skipPast(";");
    }
    return error("unexpected '" + std::string(peek().text) + "'");
  }

  Result parseStruct() {
    next();
    RawStruct raw;
    Result result = expectIdent(raw.name);
    if (result.isOk()) {
      result = expectPunct("{");
    }
    if (!result.isOk()) {
      return result;
    }
    while (!isPunct("}")) {
      std::vector<Attribute> attributes;
      RawMember member;
      result = parseAttributes(attributes);
      if (result.isOk()) {
        result = expectIdent(member.name);
      }
      if (result.isOk()) {
        result = expectPunct(":");
      }
      if (result.isOk()) {
        result = parseType(member.type);
      }
      if (!result.isOk()) {
        return result;
      }
      member.align = attributeInteger(attributes, "align");
      member.size = attributeInteger(attributes, "size");
      raw.members.push_back(std::move(member));
      if (isPunct(",") || isPunct(";")) {
        next();
      } else if (!isPunct("}")) {
        return error("expected ',' or '}' in a struct");
      }
    }
    next();
    if (isPunct(";")) {
      next();
    }
    rawStructs_.push_back(std::move(raw));
    return Result();
  }

  Result parseVar(const std::vector<Attribute>& attributes) {
    RawBinding raw;
    raw.line = next().line;
    Result result;
    if (isPunct("<")) {
      next();
      result = expectIdent(raw.addressSpace);
      if (!result.isOk()) {
        return result;
      }
      if (isPunct(",")) {
        next();
        result = expectIdent(raw.access);
        if (!result.isOk()) {
          return result;
        }
      }
      result = expectPunct(">");
      if (!result.isOk()) {
        return result;
      }
    }
    result = expectIdent(raw.name);
    if (!result.isOk()) {
      return result;
    }
    if (isPunct(":")) {
      next();
      result = parseType(raw.type);
      if (!result.isOk()) {
        return result;
      }
    }
    result = skipPast(";");
    if (!result.isOk()) {
      return result;
    }
    const std::optional<uint32_t> group = attributeInteger(attributes, "group");
    const std::optional<uint32_t> binding = attributeInteger(attributes, "binding");
    // An attribute that is present but not a literal or const would otherwise drop the binding.
    if (group.has_value() != binding.has_value() ||
        (!group && (hasAttribute(attributes, "group") || hasAttribute(attributes, "binding")))) {
      return Result(Result::Code::ArgumentInvalid,
                    "WGSL reflection, line " + std::to_string(raw.line) +
                        ": @group and @binding must be literal integers or consts");
    }
    if (group) {
      raw.group = *group;
      raw.binding = *binding;
      rawBindings_.push_back(std::move(raw));
    }
    return Result();
  }

  Result parseOverride(const std::vector<Attribute>& attributes, WgslReflection& out) {
    next();
    WgslOverride value;
    Result result = expectIdent(value.name);
    if (!result.isOk()) {
      return result;
    }
    if (isPunct(":")) {
      next();
      TypeNode type;
      result = parseType(type);
      if (!result.isOk()) {
        return result;
      }
      value.type = toString(type);
    }
    value.id = attributeInteger(attributes, "id");
    out.overrides.push_back(std::move(value));
    return skipPast(";");
  }

  Result parseConst() {
    next();
    std::string name;
    Result result = expectIdent(name);
    if (!result.isOk()) {
      return result;
    }
    if (isPunct(":")) {
      next();
      TypeNode type;
      result = parseType(type);
      if (!result.isOk()) {
        return result;
      }
    }
    result = expectPunct("=");
    if (!result.isOk()) {
      return result;
    }
    std::vector<Token> value;
    while (!isPunct(";") && peek().kind != TokenKind::End) {
      value.push_back(next());
    }
    if (const std::optional<uint32_t> integer = evalInteger(value)) {
      consts_[name] = *integer;
    }
    return expectPunct(";");
  }

  Result parseFunction(const std::vector<Attribute>& attributes, WgslReflection& out) {
    next();
    std::string name;
    Result result = expectIdent(name);
    if (result.isOk()) {
      result = expectPunct("(");
    }
    if (result.isOk()) {
      result = skipPast(")");
    }
    if (!result.isOk()) {
      return result;
    }
    while (!isPunct("{") && peek().kind != TokenKind::End) {
      next();
    }
    result = expectPunct("{");
    if (result.isOk()) {
      result = skipPast("}");
    }
    if (!result.isOk()) {
      return result;
    }

    std::optional<ShaderStage> stage;
    if (hasAttribute(attributes, "vertex")) {
      stage = ShaderStage::Vertex;
    } else if (hasAttribute(attributes, "fragment")) {
      stage = ShaderStage::Fragment;
    } else if (hasAttribute(attributes, "compute")) {
      stage = ShaderStage::Compute;
    }
    if (!stage) {
      return Result();
    }
    WgslEntryPoint entryPoint{.name = name, .stage = *stage};
    for (const Attribute& attribute : attributes) {
      if (attribute.name != "workgroup_size") {
        continue;
      }
      entryPoint.workgroupSize = {1, 1, 1};
      for (size_t i = 0; i < std::min<size_t>(attribute.args.size(), 3); ++i) {
        entryPoint.workgroupSize[i] = evalInteger(attribute.args[i]).value_or(0);
      }
    }
    out.entryPoints.push_back(std::move(entryPoint));
    return Result();
  }

  [[nodiscard]] const TypeNode& resolveAlias(const TypeNode& type) const {
    const TypeNode* current = &type;
    for (int i = 0; i < 64; ++i) {
      const auto it = aliases_.find(current->name);
      if (!current->args.empty() || it == aliases_.end()) {
        break;
      }
      current = &it->second;
    }
    return *current;
  }

  // Scalar size of a component type name, 0 if not a scalar.
  static uint32_t scalarSize(std::string_view name) {
    if (name == "f32" || name == "i32" || name == "u32" || name == "bool") {
      return 4;
    }
    return name == "f16" ? 2 : 0;
  }

  static UniformType vectorUniformType(std::string_view scalar, uint32_t count) {
    if (scalar == "f32") {
      constexpr UniformType kFloat[] = {
          UniformType::Float, UniformType::Float2, UniformType::Float3, UniformType::Float4};
      return kFloat[count - 1];
    }
    if (scalar == "i32" || scalar == "u32") {
      constexpr UniformType kInt[] = {
          UniformType::Int, UniformType::Int2, UniformType::Int3, UniformType::Int4};
      return kInt[count - 1];
    }
    return scalar == "bool" && count == 1 ? UniformType::Boolean : UniformType::Invalid;
  }

  // Splits `vec3f`, `mat4x4h`, `vec2<u32>` and `mat3x3<f32>` into columns, rows and scalar.
  static bool splitVectorOrMatrix(const TypeNode& type,
                                  uint32_t& columns,
                                  uint32_t& rows,
                                  std::string& scalar) {
    const std::string& name = type.name;
    std::string suffix;
    if (name.size() >= 4 && name.rfind("vec", 0) == 0 && isDigit(name[3])) {
      columns = 1;
      rows = static_cast<uint32_t>(name[3] - '0');
      suffix = name.substr(4);
    } else if (name.size() >= 6 && name.rfind("mat", 0) == 0 && isDigit(name[3]) &&
               name[4] == 'x' && isDigit(name[5])) {
      columns = static_cast<uint32_t>(name[3] - '0');
      rows = static_cast<uint32_t>(name[5] - '0');
      suffix = name.substr(6);
    } else {
      return false;
    }
    if (suffix.empty() && type.args.size() == 1) {
      scalar = type.args[0].name;
    } else if (suffix == "f") {
      scalar = "f32";
    } else if (suffix == "h") {
      scalar = "f16";
    } else if (suffix == "i") {
      scalar = "i32";
    } else if (suffix == "u") {
      scalar = "u32";
    } else {
      return false;
    }
    return rows >= 2 && rows <= 4 && columns >= 1 && columns <= 4;
  }

  // Recursion depth is bounded by the nesting in the source.
  // NOLINTNEXTLINE(misc-no-recursion)
  Result layoutOf(const TypeNode& unresolved, Layout& out) {
    const TypeNode& type = resolveAlias(unresolved);
    if (const uint32_t size = scalarSize(type.name)) {
      out = {.size = size, .align = size, .uniformType = vectorUniformType(type.name, 1)};
      return Result();
    }
    if (type.name == "atomic" && type.args.size() == 1) {
      out = {.size = 4, .align = 4, .uniformType = vectorUniformType(type.args[0].name, 1)};
      return Result();
    }
    uint32_t columns = 0;
    uint32_t rows = 0;
    std::string scalar;
    if (splitVectorOrMatrix(type, columns, rows, scalar)) {
      const uint32_t s = scalarSize(scalar);
      if (s == 0) {
        return error("unknown component type '" + scalar + "'");
      }
      const uint32_t vectorAlign = s * (rows == 3 ? 4 : rows);
      const uint32_t vectorSize = s * rows;
      if (columns == 1) {
        out = {.size = vectorSize,
               .align = vectorAlign,
               .uniformType = vectorUniformType(scalar, rows)};
        return Result();
      }
      UniformType uniformType = UniformType::Invalid;
      if (scalar == "f32" && columns == rows) {
        uniformType = columns == 2   ? UniformType::Mat2x2
                      : columns == 3 ? UniformType::Mat3x3
                                     : UniformType::Mat4x4;
      }
      out = {.size = columns * roundUp(vectorSize, vectorAlign),
             .align = vectorAlign,
             .uniformType = uniformType};
      return Result();
    }
    if (type.name == "array" && !type.args.empty()) {
      Layout element;
      Result result = layoutOf(type.args[0], element);
      if (!result.isOk()) {
        return result;
      }
      uint32_t count = 0;
      if (type.args.size() > 1) {
        const TypeNode& countNode = type.args[1];
        std::optional<uint32_t> value;
        if (countNode.isNumber) {
          value = parseInteger(countNode.name);
        } else if (const auto it = consts_.find(countNode.name); it != consts_.end()) {
          value = it->second;
        }
        if (!value) {
          return error("array count '" + countNode.name + "' is not a literal or const");
        }
        count = *value;
      }
      const uint32_t stride = roundUp(element.size, element.align);
      const uint64_t size = uint64_t{stride} * std::max(count, 1u);
      if (size > std::numeric_limits<uint32_t>::max()) {
        return error("array '" + toString(type) + "' is too large");
      }
      out = {.size = static_cast<uint32_t>(size),
             .align = element.align,
             .uniformType = element.arrayLength == 1 ? element.uniformType : UniformType::Invalid,
             .arrayLength = count,
             .arrayStride = stride};
      return Result();
    }
    if (const auto it = structIndices_.find(type.name); it != structIndices_.end()) {
      const WgslStruct* s = nullptr;
      Result result = layoutStruct(it->second, s);
      if (!result.isOk()) {
        return result;
      }
      out = {.size = s->size, .align = s->align};
      return Result();
    }
    return error("unknown type '" + toString(type) + "'");
  }

  // Recursion depth is bounded by the nesting in the source.
  // NOLINTNEXTLINE(misc-no-recursion)
  Result layoutStruct(size_t index, const WgslStruct*& out) {
    if (const auto it = laidOut_.find(index); it != laidOut_.end()) {
      out = &structs_[it->second];
      return Result();
    }
    if (!inProgress_.insert(index).second) {
      return error("struct '" + rawStructs_[index].name + "' contains itself");
    }
    const RawStruct& raw = rawStructs_[index];
    WgslStruct result{.name = raw.name};
    uint32_t offset = 0;
    uint32_t structAlign = 1;
    for (const RawMember& member : raw.members) {
      Layout layout;
      Result ret = layoutOf(member.type, layout);
      if (!ret.isOk()) {
        return ret;
      }
      const uint32_t align = member.align.value_or(layout.align);
      const uint32_t size = member.size.value_or(layout.size);
      offset = roundUp(offset, align);
      result.members.push_back({
          .name = member.name,
          .type = toString(resolveAlias(member.type)),
          .offset = offset,
          .size = size,
          .align = align,
          .uniformType = layout.uniformType,
          .arrayLength = layout.arrayLength,
          .arrayStride = layout.arrayStride,
      });
      offset += size;
      structAlign = std::max(structAlign, align);
    }
    result.align = structAlign;
    result.size = roundUp(offset, structAlign);
    inProgress_.erase(index);
    laidOut_[index] = structs_.size();
    structs_.push_back(std::move(result));
    out = &structs_.back();
    return Result();
  }

  static std::optional<WGPUTextureViewDimension> viewDimension(std::string_view suffix) {
    if (suffix == "1d") {
      return WGPUTextureViewDimension_1D;
    }
    if (suffix == "2d") {
      return WGPUTextureViewDimension_2D;
    }
    if (suffix == "2d_array") {
      return WGPUTextureViewDimension_2DArray;
    }
    if (suffix == "3d") {
      return WGPUTextureViewDimension_3D;
    }
    if (suffix == "cube") {
      return WGPUTextureViewDimension_Cube;
    }
    if (suffix == "cube_array") {
      return WGPUTextureViewDimension_CubeArray;
    }
    return std::nullopt;
  }

  static std::optional<WGPUTextureFormat> storageFormat(std::string_view name) {
    constexpr std::pair<std::string_view, WGPUTextureFormat> kFormats[] = {
        {"r8unorm", WGPUTextureFormat_R8Unorm},
        {"rgba8unorm", WGPUTextureFormat_RGBA8Unorm},
        {"rgba8snorm", WGPUTextureFormat_RGBA8Snorm},
        {"rgba8uint", WGPUTextureFormat_RGBA8Uint},
        {"rgba8sint", WGPUTextureFormat_RGBA8Sint},
        {"bgra8unorm", WGPUTextureFormat_BGRA8Unorm},
        {"rgba16uint", WGPUTextureFormat_RGBA16Uint},
        {"rgba16sint", WGPUTextureFormat_RGBA16Sint},
        {"rgba16float", WGPUTextureFormat_RGBA16Float},
        {"r32uint", WGPUTextureFormat_R32Uint},
        {"r32sint", WGPUTextureFormat_R32Sint},
        {"r32float", WGPUTextureFormat_R32Float},
        {"rg32uint", WGPUTextureFormat_RG32Uint},
        {"rg32sint", WGPUTextureFormat_RG32Sint},
        {"rg32float", WGPUTextureFormat_RG32Float},
        {"rgba32uint", WGPUTextureFormat_RGBA32Uint},
        {"rgba32sint", WGPUTextureFormat_RGBA32Sint},
        {"rgba32float", WGPUTextureFormat_RGBA32Float},
    };
    for (const auto& [formatName, format] : kFormats) {
      if (formatName == name) {
        return format;
      }
    }
    return std::nullopt;
  }

  Result makeBinding(const RawBinding& raw, WgslBinding& out) {
    const TypeNode& type = resolveAlias(raw.type);
    out = {.name = raw.name, .group = raw.group, .binding = raw.binding, .type = toString(type)};
    const auto fail = [&raw](const std::string& message) {
      return Result(Result::Code::ArgumentInvalid,
                    "WGSL reflection, line " + std::to_string(raw.line) + ": " + message);
    };
    if (raw.addressSpace == "uniform" || raw.addressSpace == "storage") {
      out.kind = raw.addressSpace == "uniform" ? WgslBindingKind::UniformBuffer
                 : raw.access == "read_write"  ? WgslBindingKind::StorageBuffer
                                               : WgslBindingKind::ReadOnlyStorageBuffer;
      Layout layout;
      Result result = layoutOf(type, layout);
      if (!result.isOk()) {
        return result;
      }
      out.bufferSize = layout.size;
      return Result();
    }
    if (!raw.addressSpace.empty() && raw.addressSpace != "handle") {
      return fail("unexpected address space '" + raw.addressSpace + "' on a binding");
    }
    const std::string& name = type.name;
    if (name == "sampler" || name == "sampler_comparison") {
      out.kind = name == "sampler" ? WgslBindingKind::Sampler : WgslBindingKind::ComparisonSampler;
      return Result();
    }
    const auto setSampledType = [&out, &type, &fail]() {
      const std::string component = type.args.size() == 1 ? type.args[0].name : "";
      if (component == "f32") {
        out.sampledType = WgslSampledType::Float;
      } else if (component == "i32") {
        out.sampledType = WgslSampledType::Sint;
      } else if (component == "u32") {
        out.sampledType = WgslSampledType::Uint;
      } else {
        return fail("texture component type must be f32, i32 or u32");
      }
      return Result();
    };
    if (name == "texture_multisampled_2d") {
      out.kind = WgslBindingKind::MultisampledTexture;
      out.viewDimension = WGPUTextureViewDimension_2D;
      return setSampledType();
    }
    if (name.rfind("texture_depth_", 0) == 0) {
      const auto dimension = viewDimension(std::string_view(name).substr(14));
      if (!dimension) {
        return fail("unsupported texture type '" + name + "'");
      }
      out.kind = WgslBindingKind::DepthTexture;
      out.viewDimension = *dimension;
      return Result();
    }
    if (name.rfind("texture_storage_", 0) == 0) {
      const auto dimension = viewDimension(std::string_view(name).substr(16));
      const auto format = type.args.size() == 2 ? storageFormat(type.args[0].name) : std::nullopt;
      if (!dimension || !format) {
        return fail("unsupported storage texture '" + toString(type) + "'");
      }
      const std::string& access = type.args[1].name;
      if (access != "read" && access != "write" && access != "read_write") {
        return fail("unsupported storage texture access '" + access + "'");
      }
      out.kind = WgslBindingKind::StorageTexture;
      out.viewDimension = *dimension;
      out.storageFormat = *format;
      out.storageAccess = access == "read"         ? WGPUStorageTextureAccess_ReadOnly
                          : access == "read_write" ? WGPUStorageTextureAccess_ReadWrite
                                                   : WGPUStorageTextureAccess_WriteOnly;
      return Result();
    }
    if (name.rfind("texture_", 0) == 0) {
      const auto dimension = viewDimension(std::string_view(name).substr(8));
      if (!dimension) {
        return fail("unsupported texture type '" + name + "'");
      }
      out.kind = WgslBindingKind::Texture;
      out.viewDimension = *dimension;
      return setSampledType();
    }
    return fail("unsupported binding type '" + toString(type) + "'");
  }

  Result finish(WgslReflection& out) {
    for (size_t i = 0; i < rawStructs_.size(); ++i) {
      if (!structIndices_.emplace(rawStructs_[i].name, i).second) {
        return Result(Result::Code::ArgumentInvalid,
                      "WGSL reflection: struct '" + rawStructs_[i].name + "' is declared twice");
      }
    }
    for (size_t i = 0; i < rawStructs_.size(); ++i) {
      const WgslStruct* ignored = nullptr;
      Result result = layoutStruct(i, ignored);
      if (!result.isOk()) {
        return result;
      }
    }
    std::set<std::pair<uint32_t, uint32_t>> seen;
    for (const RawBinding& raw : rawBindings_) {
      if (!seen.emplace(raw.group, raw.binding).second) {
        return Result(Result::Code::ArgumentInvalid,
                      "WGSL reflection: @group(" + std::to_string(raw.group) + ") @binding(" +
                          std::to_string(raw.binding) + ") is declared twice");
      }
      WgslBinding binding;
      Result result = makeBinding(raw, binding);
      if (!result.isOk()) {
        return result;
      }
      out.bindings.push_back(std::move(binding));
    }
    std::sort(out.bindings.begin(), out.bindings.end(), [](const auto& a, const auto& b) {
      return std::tie(a.group, a.binding) < std::tie(b.group, b.binding);
    });
    // Declaration order.
    out.structs.resize(rawStructs_.size());
    for (const auto& [rawIndex, index] : laidOut_) {
      out.structs[rawIndex] = structs_[index];
    }
    return Result();
  }

  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::map<std::string, uint32_t> consts_;
  std::map<std::string, TypeNode> aliases_;
  std::vector<RawStruct> rawStructs_;
  std::vector<RawBinding> rawBindings_;
  std::map<std::string, size_t> structIndices_;
  // Raw struct index -> index in structs_.
  std::map<size_t, size_t> laidOut_;
  std::set<size_t> inProgress_;
  std::vector<WgslStruct> structs_;
};

} // namespace

const WgslEntryPoint* IGL_NULLABLE WgslReflection::findEntryPoint(std::string_view name) const {
  const auto it = std::find_if(
      entryPoints.begin(), entryPoints.end(), [name](const auto& e) { return e.name == name; });
  return it != entryPoints.end() ? &*it : nullptr;
}

const WgslStruct* IGL_NULLABLE WgslReflection::findStruct(std::string_view name) const {
  const auto it = std::find_if(
      structs.begin(), structs.end(), [name](const auto& s) { return s.name == name; });
  return it != structs.end() ? &*it : nullptr;
}

const WgslBinding* IGL_NULLABLE WgslReflection::findBinding(uint32_t group,
                                                            uint32_t binding) const {
  const auto it = std::find_if(bindings.begin(), bindings.end(), [group, binding](const auto& b) {
    return b.group == group && b.binding == binding;
  });
  return it != bindings.end() ? &*it : nullptr;
}

Result parseWgslReflection(std::string_view source, WgslReflection& outReflection) {
  outReflection = {};
  std::vector<Token> tokens;
  Result result = tokenize(source, tokens);
  if (!result.isOk()) {
    return result;
  }
  Parser parser(std::move(tokens));
  WgslReflection reflection;
  result = parser.parse(reflection);
  if (result.isOk()) {
    outReflection = std::move(reflection);
  }
  return result;
}

} // namespace igl::webgpu
