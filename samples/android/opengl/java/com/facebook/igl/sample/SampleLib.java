/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.igl.sample.opengl;

// Wrapper for our native C++ library, which implements the actual rendering.
public class SampleLib {

  static {
    System.loadLibrary("sampleOpenGLJni");
  }

  public static native void init();

  public static native void surfaceChanged();

  public static native void render();
}
