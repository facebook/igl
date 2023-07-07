/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.igl.shell.openxr.gles;

import android.app.NativeActivity;
import android.os.Bundle;

public class MainActivity extends NativeActivity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Uncomment the line below and attach debugger
    // android.os.Debug.waitForDebugger();
    super.onCreate(savedInstanceState);
  }

  static {
    System.loadLibrary("openxr-gles-Jni");
  }
}
