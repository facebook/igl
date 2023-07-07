/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
import java.util.HashMap;

public class SampleActivity extends Activity implements View.OnClickListener {
  // UI
  LinearLayout mMainView;
  LinearLayout mTabBar;
  FrameLayout mBackendViewFrame;

  // initialize runtime backend configuration / context
  private String curBackendType = SampleLib.gl3Label;
  private HashMap<String, SurfaceView> mTabBackendViewMap;

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
    mTabBackendViewMap = new HashMap<String, SurfaceView>();
    for (int i = 0; i < SampleLib.backendTypeContexts.length; i++) {
      if (!SampleLib.isBackendTypeIDSupported(SampleLib.backendTypeContexts[i].ID)) {
        continue;
      }

      // configure and insert tab
      TextView item = new TextView(this);
      item.setId(i);
      item.setText(SampleLib.backendTypeContexts[i].label);
      item.setPadding(20, 0, 20, 0);
      item.setOnClickListener(this);
      mTabBar.addView(item);

      // initialize sampleView for each backend type
      SurfaceView backendView;
      if (SampleLib.backendTypeContexts[i].ID == SampleLib.vulkanID) {
        backendView = new VulkanView(getApplication());
      } else {
        backendView = new SampleView(getApplication(), SampleLib.backendTypeContexts[i].ID);
        ((SampleView) backendView).onPause();
      }

      // set current backend tab as selected
      if (SampleLib.backendTypeContexts[i].label.equals(curBackendType)) {
        item.setTextColor(selectedTabColor);
        mBackendViewFrame.addView(backendView);
      } else {
        item.setTextColor(unSelectedTabColor);
      }

      // cache sampleView in map for reference
      mTabBackendViewMap.put(SampleLib.backendTypeContexts[i].label, backendView);
    }

    // setup and display the mainview
    mMainView.addView(mTabBar);
    mMainView.addView(mBackendViewFrame);
    setContentView(mMainView);
  }

  @Override
  public void onClick(View view) {
    String prevBackendType = curBackendType;
    for (int i = 0; i < mTabBar.getChildCount(); i++) {
      TextView item = (TextView) mTabBar.getChildAt(i);

      // if not the selected tab, reset the tab status and skip
      if (view.getId() != item.getId()) {
        item.setTextColor(unSelectedTabColor);
        continue;
      }

      // reset main view if the selected backend is not the current
      if (!prevBackendType.equals(item.getText().toString())) {
        if (!prevBackendType.equals(SampleLib.vulkanLabel)) {
          ((SampleView) mTabBackendViewMap.get(prevBackendType)).onPause();
        }
        item.setTextColor(selectedTabColor);
        curBackendType = item.getText().toString();
        mBackendViewFrame.removeAllViews();
        SurfaceView surfaceView = mTabBackendViewMap.get(curBackendType);
        mBackendViewFrame.addView(surfaceView);
        SampleLib.setActiveBackendTypeID(i);
        if (!curBackendType.equals(SampleLib.vulkanLabel)) {
          ((SampleView) mTabBackendViewMap.get(curBackendType)).onResume();
        }
      }
    }
  }

  @Override
  protected void onPause() {
    super.onPause();
    if (!curBackendType.equals(SampleLib.vulkanLabel)) {
      ((SampleView) mTabBackendViewMap.get(curBackendType)).onPause();
    }
  }

  @Override
  protected void onResume() {
    super.onResume();
    if (!curBackendType.equals(SampleLib.vulkanLabel)) {
      ((SampleView) mTabBackendViewMap.get(curBackendType)).onResume();
    }
  }
}
