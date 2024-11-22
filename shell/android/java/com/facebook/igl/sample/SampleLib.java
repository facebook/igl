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

    public BackendVersion(BackendFlavor flavor, byte majorVersion, byte minorVersion) {
      this.flavor = flavor;
      this.majorVersion = majorVersion;
      this.minorVersion = minorVersion;
    }
  }

  public static native RenderSessionConfig[] getRenderSessionConfigs();

  public static native void init(
      BackendVersion backendVersion,
      int swapchainColorTextureFormat,
      AssetManager assetManager,
      Surface surface);

  public static native void setActiveBackendVersion(BackendVersion backendVersion);

  public static native void surfaceChanged(Surface surface, int width, int height);

  public static native void render(float displayScale);

  public static native void touchEvent(boolean isDown, float x, float y, float dx, float dy);

  public static native void setClearColorValue(float r, float g, float b, float a);

  public static native boolean isSRGBTextureFormat(int textureFormat);

  public static native void surfaceDestroyed(Surface surface);

  public static class RenderSessionConfig {
    String displayName;
    BackendVersion version;
    int swapchainColorTextureFormat;

    public RenderSessionConfig(
        String displayName, BackendVersion version, int swapchainColorTextureFormat) {
      this.displayName = displayName;
      this.version = version;
      this.swapchainColorTextureFormat = swapchainColorTextureFormat;
    }
  }
}
