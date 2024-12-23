/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.shell;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class NanovgSampleActivity extends SampleActivity {
  private static  String TAG = "NanovgSampleActivity";

  @Override
  protected void onCreate(Bundle icicle) {
    mEnableStencilBuffer = true;

    super.onCreate(icicle);

    boolean hasCopy = getSharedPreferences("data", Context.MODE_PRIVATE).getBoolean("HasCopyAssets", false);

    if (!hasCopy) {
      copyAssetsDirToSDCard(this, "", getFilesDir().getAbsolutePath());

      SharedPreferences.Editor editor = getSharedPreferences("data", Context.MODE_PRIVATE).edit();
      editor.putBoolean("HasCopyAssets",true);
      editor.commit();
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
