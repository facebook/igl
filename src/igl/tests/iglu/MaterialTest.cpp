/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/simple_renderer/Material.h>

TEST(BlendModeTest, OpaqueFactory) {
  const auto mode = iglu::material::BlendMode::Opaque();
  EXPECT_EQ(mode.srcRGB, igl::BlendFactor::One);
  EXPECT_EQ(mode.dstRGB, igl::BlendFactor::Zero);
  EXPECT_EQ(mode.opRGB, igl::BlendOp::Add);
  EXPECT_EQ(mode.srcAlpha, igl::BlendFactor::One);
  EXPECT_EQ(mode.dstAlpha, igl::BlendFactor::Zero);
  EXPECT_EQ(mode.opAlpha, igl::BlendOp::Add);
}

TEST(BlendModeTest, TranslucentFactory) {
  const auto mode = iglu::material::BlendMode::Translucent();
  EXPECT_EQ(mode.srcRGB, igl::BlendFactor::SrcAlpha);
  EXPECT_EQ(mode.dstRGB, igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.opRGB, igl::BlendOp::Add);
  EXPECT_EQ(mode.srcAlpha, igl::BlendFactor::One);
  EXPECT_EQ(mode.dstAlpha, igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.opAlpha, igl::BlendOp::Add);
}

TEST(BlendModeTest, AdditiveFactory) {
  const auto mode = iglu::material::BlendMode::Additive();
  EXPECT_EQ(mode.srcRGB, igl::BlendFactor::SrcAlpha);
  EXPECT_EQ(mode.dstRGB, igl::BlendFactor::One);
  EXPECT_EQ(mode.opRGB, igl::BlendOp::Add);
  EXPECT_EQ(mode.srcAlpha, igl::BlendFactor::SrcAlpha);
  EXPECT_EQ(mode.dstAlpha, igl::BlendFactor::One);
  EXPECT_EQ(mode.opAlpha, igl::BlendOp::Add);
}

TEST(BlendModeTest, PremultipliedFactory) {
  const auto mode = iglu::material::BlendMode::Premultiplied();
  EXPECT_EQ(mode.srcRGB, igl::BlendFactor::One);
  EXPECT_EQ(mode.dstRGB, igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.opRGB, igl::BlendOp::Add);
  EXPECT_EQ(mode.srcAlpha, igl::BlendFactor::One);
  EXPECT_EQ(mode.dstAlpha, igl::BlendFactor::OneMinusSrcAlpha);
  EXPECT_EQ(mode.opAlpha, igl::BlendOp::Add);
}

TEST(BlendModeTest, EqualityOperator) {
  using BF = igl::BlendFactor;
  using BO = igl::BlendOp;
  const auto opaque = iglu::material::BlendMode::Opaque();

  EXPECT_EQ(opaque, iglu::material::BlendMode::Opaque());

  EXPECT_NE(opaque,
            iglu::material::BlendMode(BF::SrcAlpha, BF::Zero, BO::Add, BF::One, BF::Zero, BO::Add));
  EXPECT_NE(opaque,
            iglu::material::BlendMode(BF::One, BF::One, BO::Add, BF::One, BF::Zero, BO::Add));
  EXPECT_NE(opaque,
            iglu::material::BlendMode(BF::One, BF::Zero, BO::Subtract, BF::One, BF::Zero, BO::Add));
  EXPECT_NE(opaque,
            iglu::material::BlendMode(BF::One, BF::Zero, BO::Add, BF::SrcAlpha, BF::Zero, BO::Add));
  EXPECT_NE(opaque,
            iglu::material::BlendMode(BF::One, BF::Zero, BO::Add, BF::One, BF::One, BO::Add));
  EXPECT_NE(opaque,
            iglu::material::BlendMode(BF::One, BF::Zero, BO::Add, BF::One, BF::Zero, BO::Subtract));
}
