/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only: 

package com.facebook.igl.shell.openxr.vulkan;

import android.content.Intent;

public class MainActivity extends android.app.NativeActivity {
  static {
    System.loadLibrary("openxr-vulkan-Jni");
  }

  @Override
  protected void onNewIntent(Intent intent) {
    super.onNewIntent(intent);
    if (intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
      onActionView(intent.getStringExtra("data"));
    }
  }

  private native void onActionView(String data);
}
