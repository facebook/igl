# IGL Android Shell

Sample Activity, JNI bridge, and renderer scaffolding that hosts an
`igl::shell::RenderSession` on Android (Vulkan or OpenGL ES). Used by the
MediaPrism demo and other IGL sample apps.

## Layout

- `java/com/facebook/igl/sample/` — `SampleActivity`, `SampleLib`,
  `SampleView`, `VulkanView`. The Activity owns the `Intent` and forwards it
  to the surface views, which hand it to the JNI layer at first surface
  creation.
- `jni/` — `Jni.cpp` (Java ↔ native bridge, Intent extra parsing) and
  `TinyRenderer` (per-backend `IDevice` setup + frame loop).

## Passing CLI flags via adb

The shell exposes one Intent extra to native CLI parsers: a single string
named `args` whose value is forwarded to
`igl::shell::Platform::argc()` / `argv()` after whitespace tokenization.
RenderSessions consume those tokens through the same shared flag parsers
used on Mac, iOS, and Windows.

Example — launch the MediaPrism demo with the file input source instead of
the default Camera:

```sh
adb shell am start -n com.facebook.igl.sample/com.facebook.igl.shell.SampleActivity \
  --es "args" "--input-source=file"
```

Multiple flags go into the same string, separated by spaces:

```sh
adb shell am start -n com.facebook.igl.sample/com.facebook.igl.shell.SampleActivity \
  --es "args" "--input-source=file --frames=120"
```

Notes:

- Other Intent extras (e.g. `debug.iglshell.<session>.*` system properties or
  custom Bundle keys) stay on the existing `ShellParams` path and are not
  surfaced through `Platform::argv()`.
- When no `args` extra is supplied, `Platform::argc()` returns `0` and
  `Platform::argv()` returns `nullptr` — preserving the historical Android
  behavior.
- The placeholder `argv[0]` ("`igl_android_shell`") is reserved as a program
  name so `findCliFlag()`-style parsers (which skip index 0) see the real
  tokens at indices ≥ 1.
