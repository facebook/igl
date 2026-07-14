/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <functional>
#include <igl/Shader.h>

namespace igl::tests {

// Default-constructed options must preserve IGL's historical behavior so existing clients
// (HelloWorld, TinyMesh, IGLU samples, screenshot tests) are byte-for-byte unaffected.
TEST(ShaderCompilerOptionsTest, DefaultConstruction) {
  const ShaderCompilerOptions options;
  EXPECT_TRUE(options.fastMathEnabled);
  EXPECT_EQ(options.optimization, ShaderOptimization::Default);
}

TEST(ShaderCompilerOptionsTest, EqualityOpReflexive) {
  const ShaderCompilerOptions options;
  EXPECT_EQ(options, options);
}

TEST(ShaderCompilerOptionsTest, EqualityOpSameValues) {
  const ShaderCompilerOptions a;
  const ShaderCompilerOptions b;
  EXPECT_EQ(a, b);
}

TEST(ShaderCompilerOptionsTest, InequalityOpDifferentOptimization) {
  ShaderCompilerOptions a;
  ShaderCompilerOptions b;
  b.optimization = ShaderOptimization::NoOpt;
  EXPECT_NE(a, b);
}

TEST(ShaderCompilerOptionsTest, InequalityOpDifferentFastMath) {
  ShaderCompilerOptions a;
  ShaderCompilerOptions b;
  b.fastMathEnabled = false;
  EXPECT_NE(a, b);
}

TEST(ShaderCompilerOptionsTest, HashConsistency) {
  const ShaderCompilerOptions options;
  const std::hash<ShaderCompilerOptions> hasher;
  EXPECT_EQ(hasher(options), hasher(options));
}

// Default-constructed options compare equal AND hash-equal, so existing clients keyed on the
// default still share a single shader-stages cache entry exactly as before.
TEST(ShaderCompilerOptionsTest, HashEqualObjectsHaveSameHash) {
  const ShaderCompilerOptions a;
  const ShaderCompilerOptions b;
  const std::hash<ShaderCompilerOptions> hasher;
  EXPECT_EQ(a, b);
  EXPECT_EQ(hasher(a), hasher(b));
}

// The cache-key fix: options that differ only in `optimization` must hash differently so debug
// (NoOpt) and release (Performance) shaders no longer collide in the shader-stages cache.
TEST(ShaderCompilerOptionsTest, HashDistinguishesOptimization) {
  const std::hash<ShaderCompilerOptions> hasher;

  const ShaderCompilerOptions defaultOpt;

  ShaderCompilerOptions noOpt;
  noOpt.optimization = ShaderOptimization::NoOpt;

  ShaderCompilerOptions performance;
  performance.optimization = ShaderOptimization::Performance;

  EXPECT_NE(hasher(defaultOpt), hasher(noOpt));
  EXPECT_NE(hasher(defaultOpt), hasher(performance));
  EXPECT_NE(hasher(noOpt), hasher(performance));
}

TEST(ShaderCompilerOptionsTest, HashDistinguishesFastMath) {
  const std::hash<ShaderCompilerOptions> hasher;
  ShaderCompilerOptions a;
  ShaderCompilerOptions b;
  b.fastMathEnabled = false;
  EXPECT_NE(hasher(a), hasher(b));
}

// Descriptor-level: an option-only change must propagate through `hash<ShaderInput>` so it stays
// consistent with `ShaderInput::operator==` (which already compares `options`). `ShaderInput` is
// what carries the options into `ShaderModuleDesc`/`ShaderLibraryDesc`, i.e. the actual cache key.
TEST(ShaderCompilerOptionsTest, ShaderInputHashDistinguishesOptimization) {
  const std::hash<ShaderInput> hasher;

  ShaderInput a;
  a.source = "void main() {}";
  a.type = ShaderInputType::String;

  ShaderInput b = a;
  b.options.optimization = ShaderOptimization::Performance;

  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

// The shader-stages cache keys on ShaderModuleDesc; an option-only change must change its hash.
TEST(ShaderCompilerOptionsTest, ShaderModuleDescHashDistinguishesOptimization) {
  const std::hash<ShaderModuleDesc> hasher;

  ShaderModuleDesc a;
  a.input.source = "void main() {}";
  a.input.type = ShaderInputType::String;

  ShaderModuleDesc b = a;
  b.input.options.optimization = ShaderOptimization::Performance;

  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

// Same propagation must hold for ShaderLibraryDesc, which also keys on its ShaderInput.
TEST(ShaderCompilerOptionsTest, ShaderLibraryDescHashDistinguishesOptimization) {
  const std::hash<ShaderLibraryDesc> hasher;

  ShaderLibraryDesc a;
  a.input.source = "void main() {}";
  a.input.type = ShaderInputType::String;

  ShaderLibraryDesc b = a;
  b.input.options.optimization = ShaderOptimization::Performance;

  EXPECT_NE(a, b);
  EXPECT_NE(hasher(a), hasher(b));
}

} // namespace igl::tests
