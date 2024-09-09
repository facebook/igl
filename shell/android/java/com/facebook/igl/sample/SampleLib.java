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

  // Must match igl/Common.h
  public static enum BackendFlavor {
    Invalid,
    OpenGL,
    OpenGL_ES,
    Metal,
    Vulkan,
    // @fb-only
  }

  // Must match igl/DeviceFeatures.h
  public static class BackendVersion {
    BackendFlavor flavor;
    byte majorVersion;
    byte minorVersion;

    BackendVersion(BackendFlavor flavor, byte majorVersion, byte minorVersion) {
      this.flavor = flavor;
      this.majorVersion = majorVersion;
      this.minorVersion = minorVersion;
    }
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
        new BackendTypeContext(
            new BackendVersion(BackendFlavor.OpenGL_ES, (byte) 3, (byte) 0), gl3Label),
        new BackendTypeContext(
            new BackendVersion(BackendFlavor.OpenGL_ES, (byte) 2, (byte) 0), gl2Label),
        new BackendTypeContext(
            new BackendVersion(BackendFlavor.Vulkan, (byte) 1, (byte) 3), vulkanLabel),
      };

  public static native void init(
      BackendVersion backendVersion, AssetManager assetManager, Surface surface);

  public static native boolean isBackendVersionSupported(BackendVersion backendVersion);

  public static native void setActiveBackendVersion(BackendVersion backendVersion);

  public static native void surfaceChanged(Surface surface, int width, int height);

  public static native void render(float displayScale);

  public static native void touchEvent(boolean isDown, float x, float y, float dx, float dy);

  public static native void setClearColorValue(float r, float g, float b, float a);

  public static native void surfaceDestroyed(Surface surface);

  protected static class BackendTypeContext {
    BackendVersion version;
    String label;

    protected BackendTypeContext(BackendVersion version, String label) {
      this.version = version;
      this.label = label;
    }
  }
}
