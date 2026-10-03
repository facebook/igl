/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Loads triangle.html in a WKWebView (WebKit's WebGPU, no JSPI) and prints the page log once its
// title shows the result. The window must be on screen, or WebKit stops running
// requestAnimationFrame callbacks. Exit status: 0 all passed, 1 failed, 2 timed out.
//
// usage: xcrun swift run_triangle_webkit.swift <url> [timeout seconds]

import AppKit
import WebKit

let arguments = CommandLine.arguments
guard arguments.count > 1, let url = URL(string: arguments[1]) else {
  print("usage: xcrun swift run_triangle_webkit.swift <url> [timeout seconds]")
  exit(2)
}
let timeout = arguments.count > 2 ? Double(arguments[2]) ?? 60 : 60
let app = NSApplication.shared
app.setActivationPolicy(.accessory)
let frame = NSRect(x: 0, y: 0, width: 800, height: 600)
let window = NSWindow(contentRect: frame, styleMask: [.titled], backing: .buffered, defer: false)
let webView = WKWebView(frame: frame, configuration: WKWebViewConfiguration())
window.contentView = webView
window.orderFrontRegardless()
webView.load(URLRequest(url: url))

let start = Date()
Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { _ in
  webView.evaluateJavaScript("document.title") { result, _ in
    let title = (result as? String) ?? ""
    if title == "ALL PASSED" || title.hasSuffix("FAILED") {
      webView.evaluateJavaScript("document.getElementById('log').textContent") { log, _ in
        print((log as? String) ?? "", terminator: "")
        exit(title == "ALL PASSED" ? 0 : 1)
      }
    } else if Date().timeIntervalSince(start) > timeout {
      print("TIMEOUT: page title is '\(title)'")
      exit(2)
    }
  }
}
app.run()
