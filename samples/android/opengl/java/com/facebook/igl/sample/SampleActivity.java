/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.sample.opengl;

import android.app.Activity;
import android.os.Bundle;

public class SampleActivity extends Activity {

  SampleView mView;

  @Override
  protected void onCreate(Bundle icicle) {
    super.onCreate(icicle);
    mView = new SampleView(getApplication());
    setContentView(mView);
  }

  @Override
  protected void onPause() {
    super.onPause();
    mView.onPause();
  }

  @Override
  protected void onResume() {
    super.onResume();
    mView.onResume();
  }
}
