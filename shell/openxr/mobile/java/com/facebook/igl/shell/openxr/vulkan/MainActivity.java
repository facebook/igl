/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.shell.openxr.vulkan;

public class MainActivity extends android.app.NativeActivity {
  static {
    System.loadLibrary("openxr-vulkan-Jni");
  }
}
