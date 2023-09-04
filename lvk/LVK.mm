/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// clang-format off
#if __APPLE__
#  define GLFW_EXPOSE_NATIVE_COCOA
#else
#  error Unsupported OS
#endif
// clang-format on

#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

void* createCocoaWindowView(GLFWwindow* window) {
  NSWindow* nswindow = glfwGetCocoaWindow(window);
  CAMetalLayer* layer = [CAMetalLayer layer];
  layer.device = MTLCreateSystemDefaultDevice();
  layer.opaque = YES;
  layer.displaySyncEnabled = YES;
  NSScreen* screen = [NSScreen mainScreen];
  CGFloat factor = [screen backingScaleFactor];
  layer.contentsScale = factor;
  nswindow.contentView.layer = layer;
  nswindow.contentView.wantsLayer = YES;

  return nswindow.contentView;
}
