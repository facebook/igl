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
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Choreographer;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

// Simple view that sets up a Vulkan Render view
public class VulkanView extends SurfaceView
    implements SurfaceHolder.Callback2, Choreographer.FrameCallback {
  private static String TAG = "VulkanView";
  private float lastTouchX = 0.0f;
  private float lastTouchY = 0.0f;
  Context mContext;
  RenderThread mRenderThread;
  private final SampleLib.BackendVersion mBackendVersion;
  private final int mSwapchainColorTextureFormat;

  public VulkanView(
      Context context, SampleLib.BackendVersion backendVersion, int swapchainColorTextureFormat) {

    super(context);

    // Set the view to be translucent since we provide an alpha channel below.
    this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
    this.getHolder().addCallback(this);

    mContext = context;
    mBackendVersion = backendVersion;
    mSwapchainColorTextureFormat = swapchainColorTextureFormat;
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

  @Override
  public void surfaceRedrawNeeded(SurfaceHolder surfaceHolder) {}

  @Override
  public void surfaceCreated(SurfaceHolder surfaceHolder) {
    // Start rendering on the RenderThread
    mRenderThread =
        new RenderThread(mContext, surfaceHolder, mBackendVersion, mSwapchainColorTextureFormat);
    mRenderThread.setName("Vulkan Render Thread");
    mRenderThread.start();
    mRenderThread.waitUntilReady();

    RenderHandler rh = mRenderThread.getHandler();
    if (rh != null) {
      rh.sendSurfaceCreated();
      // start the draw events
      Choreographer.getInstance().postFrameCallback(this);
    }
  }

  @Override
  public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height) {
    RenderHandler rh = mRenderThread.getHandler();
    if (rh != null) {
      rh.sendSurfaceChanged(format, width, height);
    }
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
    // Stop render thread
    RenderHandler rh = mRenderThread.getHandler();
    if (rh != null) {
      rh.sendShutdown();
      try {
        mRenderThread.join();
      } catch (InterruptedException ie) {
        // not expected
        throw new RuntimeException("join was interrupted", ie);
      }
    }
    mRenderThread = null;

    // If the callback was posted, remove it.  Without this, we could get one more
    // call on doFrame().
    Choreographer.getInstance().removeFrameCallback(this);
  }

  /*
   * Choreographer callback, called near vsync.
   *
   * @see android.view.Choreographer.FrameCallback#doFrame(long)
   */
  @Override
  public void doFrame(long frameTimeNanos) {
    RenderHandler rh = mRenderThread.getHandler();
    if (rh != null) {
      Choreographer.getInstance().postFrameCallback(this);
      rh.sendDoFrame(frameTimeNanos);
    }
  }

  // Rendering thread
  private class RenderThread extends Thread {
    private Context mContext;
    private SurfaceHolder mSurfaceHolder;
    private volatile RenderHandler mHandler;

    private Object mStartLock = new Object();
    private boolean mReady = false;
    private final SampleLib.BackendVersion mBackendVersion;
    private final int mSwapchainColorTextureFormat;

    public RenderThread(
        Context context,
        SurfaceHolder surfaceHolder,
        SampleLib.BackendVersion backendVersion,
        int swapchainColorTextureformat) {
      mContext = context;
      mSurfaceHolder = surfaceHolder;
      mBackendVersion = backendVersion;
      mSwapchainColorTextureFormat = swapchainColorTextureformat;
    }

    public RenderHandler getHandler() {
      return mHandler;
    }

    @Override
    public void run() {
      Looper.prepare();
      mHandler = new RenderHandler(this);
      synchronized (mStartLock) {
        mReady = true;
        mStartLock.notify(); // signal waitUntilReady()
      }

      Looper.loop();

      // surface has been destroyed
      synchronized (mStartLock) {
        mReady = false;
      }
      Surface surface = mSurfaceHolder.getSurface();
      SampleLib.surfaceDestroyed(surface);
    }

    public void waitUntilReady() {
      synchronized (mStartLock) {
        while (!mReady) {
          try {
            mStartLock.wait();
          } catch (InterruptedException ie) {
            /* not expected */
          }
        }
      }
    }

    public void surfaceCreated() {
      Log.d(TAG, "SurfaceCreated");
      Surface surface = mSurfaceHolder.getSurface();
      SampleLib.init(mBackendVersion, mSwapchainColorTextureFormat, mContext.getAssets(), surface);
    }

    public void surfaceChanged(int width, int height) {
      Surface surface = mSurfaceHolder.getSurface();
      SampleLib.surfaceChanged(surface, width, height);
    }

    /** draw frame in response to a vsync event. */
    private void doFrame(long timeStampNanos) {
      long diff = System.nanoTime() - timeStampNanos;
      long max = 16666667l - 2000000; // if we're within 2ms, don't bother
      if (diff > max) {
        // too much, drop a frame
        Log.d(
            TAG,
            "diff is "
                + (diff / 1000000.0)
                + " ms, max "
                + (max / 1000000.0)
                + ", skipping render");
        return;
      }

      SampleLib.render(mContext.getResources().getDisplayMetrics().density);
    }

    private void shutdown() {
      Log.d(TAG, "shutdown");
      Looper.myLooper().quit();
    }
  }

  /**
   * Handler for RenderThread. Used for messages sent from the UI thread to the render thread.
   *
   * <p>The object is created on the render thread, and the various "send" methods are called from
   * the UI thread.
   */
  private static class RenderHandler extends Handler {
    private static final int MSG_SURFACE_CREATED = 0;
    private static final int MSG_SURFACE_CHANGED = 1;
    private static final int MSG_DO_FRAME = 2;
    private static final int MSG_SHUTDOWN = 3;

    private RenderThread mRenderThread;

    public RenderHandler(RenderThread rt) {
      mRenderThread = rt;
    }

    public void sendSurfaceCreated() {
      sendMessage(obtainMessage(RenderHandler.MSG_SURFACE_CREATED));
    }

    public void sendSurfaceChanged(@SuppressWarnings("unused") int format, int width, int height) {
      // ignore format
      sendMessage(obtainMessage(RenderHandler.MSG_SURFACE_CHANGED, width, height));
    }

    public void sendDoFrame(long frameTimeNanos) {
      sendMessage(
          obtainMessage(
              RenderHandler.MSG_DO_FRAME, (int) (frameTimeNanos >> 32), (int) frameTimeNanos));
    }

    public void sendShutdown() {
      sendMessage(obtainMessage(RenderHandler.MSG_SHUTDOWN));
    }

    @Override // runs on RenderThread
    public void handleMessage(Message msg) {
      int what = msg.what;
      // Log.d(TAG, "RenderHandler [" + this + "]: what=" + what);

      if (mRenderThread == null) {
        Log.w(TAG, "RenderHandler.handleMessage: mRenderThread is null");
        return;
      }

      switch (what) {
        case MSG_SURFACE_CREATED:
          mRenderThread.surfaceCreated();
          break;
        case MSG_SURFACE_CHANGED:
          mRenderThread.surfaceChanged(msg.arg1, msg.arg2);
          break;
        case MSG_DO_FRAME:
          long timestamp = (((long) msg.arg1) << 32) | (((long) msg.arg2) & 0xffffffffL);
          mRenderThread.doFrame(timestamp);
          break;
        case MSG_SHUTDOWN:
          mRenderThread.shutdown();
          break;
        default:
          throw new RuntimeException("unknown message " + what);
      }
    }
  }
}
