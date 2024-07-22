/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.shell;

import android.content.res.AssetManager;
import android.view.Surface;

// Wrapper for our native C++ library, which implements the actual rendering.
public class SampleLib {

  static {
    System.loadLibrary("sampleJni");
  }

  // should match with IDs in TinyRenderer.h
  protected static final int gl3ID = 0;
  protected static final int gl2ID = 1;
  protected static final int vulkanID = 2;
  protected static final String gl3Label = "OpenGL ES 3";
  protected static final String gl2Label = "OpenGL ES 2";
  protected static final String vulkanLabel = "Vulkan";

  protected static final BackendTypeContext[] backendTypeContexts =
      new BackendTypeContext[] {
        new BackendTypeContext(gl3ID, gl3Label),
        new BackendTypeContext(gl2ID, gl2Label),
        new BackendTypeContext(vulkanID, vulkanLabel),
      };

  public static native void init(AssetManager assetManager, Surface surface);

  public static native boolean isBackendTypeIDSupported(int backendTypeID);

  public static native void setActiveBackendTypeID(int backendTypeID);

  public static native void surfaceChanged(Surface surface, int width, int height);

  public static native void render(float displayScale);

  public static native void touchEvent(boolean isDown, float x, float y, float dx, float dy);

  public static native void setClearColorValue(float r, float g, float b, float a);

  public static native void surfaceDestroyed(Surface surface);

  protected static class BackendTypeContext {
    int ID;
    String label;

    protected BackendTypeContext(int ID, String label) {
      this.ID = ID;
      this.label = label;
    }
  }
}
