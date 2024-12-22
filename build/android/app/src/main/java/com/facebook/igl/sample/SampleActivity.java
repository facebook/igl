/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.shell;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

public class SampleActivity extends Activity implements View.OnClickListener {
  // UI
  LinearLayout mMainView;
  LinearLayout mTabBar;
  FrameLayout mBackendViewFrame;

  // initialize runtime backend configuration / context
  private int curConfig = 0;
  private SampleLib.RenderSessionConfig[] mConfigs;
  private SurfaceView[] mTabViews;

  private final int selectedTabColor = Color.BLUE;
  private final int unSelectedTabColor = Color.GRAY;

  @Override
  protected void onCreate(Bundle icicle) {
    super.onCreate(icicle);

    // configure the mainview
    mMainView = new LinearLayout(this);
    mMainView.setOrientation(LinearLayout.VERTICAL);

    // build tab bar from a horizontal linearlayout
    mTabBar = new LinearLayout(this);
    mTabBar.setOrientation(LinearLayout.HORIZONTAL);
    mTabBar.setGravity(Gravity.CENTER);
    mTabBar.setPadding(10, 5, 10, 5);

    // set up tab and cached sample view for different backend types
    mBackendViewFrame = new FrameLayout(this);
    mConfigs = SampleLib.getRenderSessionConfigs();
    mTabViews = new SurfaceView[mConfigs.length];
    for (int i = 0; i < mConfigs.length; i++) {
      // configure and insert tab
      TextView item = new TextView(this);
      item.setId(i);
      item.setText(mConfigs[i].displayName);
      item.setPadding(20, 0, 20, 0);
      item.setOnClickListener(this);
      mTabBar.addView(item);

      // initialize sampleView for each backend type
      SurfaceView backendView = null;
      if (mConfigs[i].version.flavor == SampleLib.BackendFlavor.Vulkan) {
        backendView =
            new VulkanView(
                getApplication(), mConfigs[i].version, mConfigs[i].swapchainColorTextureFormat);
      } else if (mConfigs[i].version.flavor == SampleLib.BackendFlavor.OpenGL_ES) {
        backendView =
            new SampleView(
                getApplication(), mConfigs[i].version, mConfigs[i].swapchainColorTextureFormat);
        ((SampleView) backendView).onPause();
      }

      // set current backend tab as selected
      if (curConfig == i) {
        item.setTextColor(selectedTabColor);
        mBackendViewFrame.addView(backendView);
      } else {
        item.setTextColor(unSelectedTabColor);
      }

      // cache sampleView in map for reference
      mTabViews[i] = backendView;
    }

    // setup and display the mainview
    mMainView.addView(mTabBar);
    mMainView.addView(mBackendViewFrame);
    setContentView(mMainView);
  }

  @Override
  public void onClick(View view) {
    int prevConfig = curConfig;
    for (int i = 0; i < mTabBar.getChildCount(); i++) {
      TextView item = (TextView) mTabBar.getChildAt(i);

      // if not the selected tab, reset the tab status and skip
      if (view.getId() != item.getId()) {
        item.setTextColor(unSelectedTabColor);
        continue;
      }

      // reset main view if the selected backend is not the current
      if (prevConfig != i) {
        if (mConfigs[prevConfig].version.flavor != SampleLib.BackendFlavor.Vulkan) {
          ((SampleView) mTabViews[prevConfig]).onPause();
        }
        item.setTextColor(selectedTabColor);
        curConfig = i;
        mBackendViewFrame.removeAllViews();
        SurfaceView surfaceView = mTabViews[curConfig];
        mBackendViewFrame.addView(surfaceView);
        SampleLib.setActiveBackendVersion(mConfigs[curConfig].version);
        if (mConfigs[curConfig].version.flavor != SampleLib.BackendFlavor.Vulkan) {
          ((SampleView) mTabViews[curConfig]).onResume();
        }
      }
    }
  }

  @Override
  protected void onPause() {
    super.onPause();
    if (mConfigs[curConfig].version.flavor != SampleLib.BackendFlavor.Vulkan) {
      ((SampleView) mTabViews[curConfig]).onPause();
    }
  }

  @Override
  protected void onResume() {
    super.onResume();
    if (mConfigs[curConfig].version.flavor != SampleLib.BackendFlavor.Vulkan) {
      ((SampleView) mTabViews[curConfig]).onResume();
    }
  }
}
