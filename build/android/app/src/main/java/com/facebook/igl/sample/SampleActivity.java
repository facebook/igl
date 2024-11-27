/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.shell;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class SampleActivity extends Activity implements View.OnClickListener {
  // UI
  LinearLayout mMainView;
  LinearLayout mTabBar;
  FrameLayout mBackendViewFrame;

  private static String TAG = "SampleActivity";

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

    boolean hasCopy = getSharedPreferences("data", Context.MODE_PRIVATE).getBoolean("HasCopyAssets", false);

    if (!hasCopy) {
      copyAssetsDirToSDCard(this, "", getFilesDir().getAbsolutePath());

      SharedPreferences.Editor editor = getSharedPreferences("data", Context.MODE_PRIVATE).edit();
      editor.putBoolean("HasCopyAssets",true);
      editor.commit();
    }
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

  public static void copyAssetsDirToSDCard(Context context, String assetsDirName, String sdCardPath) {
    Log.d(TAG, "copyAssetsDirToSDCard() called with: context = [" + context + "], assetsDirName = [" + assetsDirName + "], sdCardPath = [" + sdCardPath + "]");
    try {
      String list[] = context.getAssets().list(assetsDirName);
      if (list.length == 0) {
        InputStream inputStream = context.getAssets().open(assetsDirName);
        byte[] mByte = new byte[1024];
        int bt = 0;
        File file = new File(sdCardPath + File.separator
                + assetsDirName);
        if (!file.exists()) {
          file.createNewFile();
        } else {
          return;
        }
        FileOutputStream fos = new FileOutputStream(file);
        while ((bt = inputStream.read(mByte)) != -1) {
          fos.write(mByte, 0, bt);
        }
        fos.flush();
        inputStream.close();
        fos.close();
      } else {
        String subDirName = assetsDirName;
        if (assetsDirName.contains("/")) {
          subDirName = assetsDirName.substring(assetsDirName.lastIndexOf('/') + 1);
        }
        sdCardPath = sdCardPath + File.separator + subDirName;
        File file = new File(sdCardPath);
        if (!file.exists())
          file.mkdirs();
        for (String s : list) {
          String fileName = assetsDirName.length() > 0 ?  assetsDirName + File.separator + s : s;
          copyAssetsDirToSDCard(context, fileName , sdCardPath);
        }
      }
    } catch (
            Exception e) {
      e.printStackTrace();
    }
  }

}
