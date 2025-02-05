/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.sample.vulkan;

import android.app.NativeActivity;
import android.os.Bundle;

public class SampleActivity extends NativeActivity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
  }

  static {
    System.loadLibrary("sampleVulkan");
  }
}
