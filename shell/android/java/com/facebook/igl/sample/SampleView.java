/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

package com.facebook.igl.shell;

import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.EGL15;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.MotionEvent;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

/// Simple view that sets up a GLES 2.0 rendering context
public class SampleView extends GLSurfaceView {
  private static String TAG = "SampleView";
  private float lastTouchX = 0.0f;
  private float lastTouchY = 0.0f;

  public SampleView(Context context, int backendTypeID) {

    super(context);
    // Uncomment to attach debugging
    // android.os.Debug.waitForDebugger();

    setEGLContextFactory(new ContextFactory(backendTypeID));

    // Set the view to be transluscent since we provide an alpha channel below.
    this.getHolder().setFormat(PixelFormat.TRANSLUCENT);

    setEGLConfigChooser(new ConfigChooser(backendTypeID));

    setRenderer(new Renderer(context));
  }

  @Override
  public void setBackgroundColor(int color) {
    int A = (color >> 24) & 0xff;
    int R = (color >> 16) & 0xff;
    int G = (color >> 8) & 0xff;
    int B = (color) & 0xff;
    SampleLib.setClearColorValue(R, G, B, A);
  }

  @Override
  public boolean onTouchEvent(MotionEvent e) {
    float x = e.getX();
    float y = e.getY();
    float dx = x - lastTouchX;
    float dy = y - lastTouchY;

    lastTouchX = x;
    lastTouchY = y;

    switch (e.getAction()) {
      case MotionEvent.ACTION_DOWN:
        SampleLib.touchEvent(true, x, y, 0, 0);
        return true;

      case MotionEvent.ACTION_MOVE:
        SampleLib.touchEvent(true, x, y, dx, dy);
        return true;

      case MotionEvent.ACTION_UP:
        SampleLib.touchEvent(false, x, y, 0, 0);
        return true;
    }

    return false;
  }

  /// Context factory: handles creating the EGL context for this view with the correct settings.
  private static class ContextFactory implements GLSurfaceView.EGLContextFactory {

    private final int mBackendTypeID;

    public ContextFactory(int backendTypeID) {
      mBackendTypeID = backendTypeID;
    }

    public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
      final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
      int[] attrib_list = {
        EGL_CONTEXT_CLIENT_VERSION, (mBackendTypeID == SampleLib.gl3ID) ? 3 : 2, EGL10.EGL_NONE
      };
      EGLContext context =
          egl.eglCreateContext(display, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
      checkEglError("Error creating EGL context", egl);
      return context;
    }

    public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
      egl.eglDestroyContext(display, context);
    }
  }

  private static void checkEglError(String prompt, EGL10 egl) {
    int error;
    while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
      Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
    }
  }

  /// Config chooser: handles specifying the requirements for the EGL config and choosing the
  // correct one.
  private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

    private final int mBackendTypeID;

    public ConfigChooser(int backendTypeID) {
      mBackendTypeID = backendTypeID;
    }

    public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {
      final int EGL_OPENGL_ES2_BIT = 4;

      // Set ourselves a strict configuration: RGBA8888, 16-bit depth buffer, no stencil.
      final int[] configAttribs = {
        EGL10.EGL_RED_SIZE, 8,
        EGL10.EGL_GREEN_SIZE, 8,
        EGL10.EGL_BLUE_SIZE, 8,
        EGL10.EGL_ALPHA_SIZE, 8,
        EGL10.EGL_DEPTH_SIZE, 16,
        EGL10.EGL_RENDERABLE_TYPE,
            (mBackendTypeID == SampleLib.gl3ID) ? EGL15.EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT,
        EGL10.EGL_NONE
      };

      int[] numConfigs = new int[1];
      egl.eglChooseConfig(display, configAttribs, null, 0, numConfigs);

      if (numConfigs[0] <= 0) {
        throw new IllegalArgumentException("Couldn't find an appropriate EGL config");
      }

      EGLConfig[] configs = new EGLConfig[1];
      egl.eglChooseConfig(display, configAttribs, configs, 1, numConfigs);

      return configs[0];
    }
  }

  /// Renderer: This class communicates with our JNI library to implement the OpenGL rendering.
  private static class Renderer implements GLSurfaceView.Renderer {
    private final Context mContext;

    Renderer(Context context) {
      mContext = context;
    }

    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
      SampleLib.init(mContext.getAssets(), null);
    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {
      SampleLib.surfaceChanged(null, width, height);
    }

    public void onDrawFrame(GL10 gl) {
      SampleLib.render(mContext.getResources().getDisplayMetrics().density);
    }
  }
}
