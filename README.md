<div align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/facebook/igl/blob/main/.github/igl-full-color-white.svg?raw=true">
    <source media="(prefers-color-scheme: light)" srcset="https://github.com/facebook/igl/blob/main/.github/igl-full-color-black.svg?raw=true">
    <img alt="IGL Logo" src=".github/igl-full-color-black.svg" width="500">
  </picture>

  [![Build Status](https://github.com/facebook/igl/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/facebook/igl/actions)

</div>

Intermediate Graphics Library (IGL) is a cross-platform library that commands the GPU. It encapsulates
common GPU functionality with a low-level cross-platform interface. IGL is designed to support multiple
backends implemented on top of various graphics APIs (e.g. OpenGL, Metal and Vulkan) with a common interface.

There are a lot of good options for abstracting GPU API's; each making different trade-offs. We designed IGL around the following priorities:

1. *Low-level, forward-looking API.* IGL embraces modern abstractions (command buffers, state containers, bindless, etc) and is designed to give more control than OpenGL's state machine API. As a result, IGL can have leaner backends for modern API's (e.g. Metal, Vulkan).
2. *Minimal overhead for C++.* IGL supports new or existing native rendering code without overhead of language interop or the need for other language runtimes.
3. *Reach + scale in production.* IGL has been globally battle-tested for broad device reliability (especially the long-tail of Android devices as well as Quest 2/3/Pro compatibility for OpenGL/Vulkan) *and* performance-tuned on our apps.

## Supported rendering backends

 * Metal 2+
 * OpenGL 2.x (requires [GL_ARB_framebuffer_object](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_framebuffer_object.txt))
 * OpenGL 3.1+
 * OpenGL ES 2.0+
 * Vulkan 1.2
 * WebGL 2.0
 * Direct3D 12

## Supported platforms

 * Android
 * iOS
 * Linux
 * macOS
 * Windows
 * WebAssembly

## API Support

|                          | Windows                    | Linux                      | macOS                         | iOS                           | Android                            |
| ------------------------ | -------------------------- | -------------------------- | ----------------------------- | ----------------------------- | ---------------------------------- |
| Vulkan 1.2               | :heavy_check_mark:         | :heavy_check_mark:         | :heavy_check_mark: (MoltenVK) | :heavy_multiplication_x:      | :heavy_check_mark: (Quest 2/3/Pro) |
| OpenGL ES 2.0 - 3.0      | :heavy_check_mark: (Angle) | :heavy_check_mark: (Angle) | :heavy_multiplication_x:      | :heavy_check_mark:            | :heavy_check_mark:                 |
| OpenGL ES 3.1 - 3.2      | :heavy_check_mark: (Angle) | :heavy_check_mark: (Angle) | :heavy_multiplication_x:      | :heavy_multiplication_x:      | :heavy_check_mark:                 |
| OpenGL 3.1 - 4.6         | :heavy_check_mark:         | :heavy_check_mark:         | :heavy_check_mark:            | :heavy_multiplication_x:      | :heavy_multiplication_x:           |
| Metal 2                  | :heavy_multiplication_x:   | :heavy_multiplication_x:   | :heavy_check_mark:            | :heavy_check_mark:            | :heavy_multiplication_x:           |
| Direct3D 12              | :heavy_check_mark:         | :heavy_multiplication_x:   | :heavy_multiplication_x:      | :heavy_multiplication_x:      | :heavy_multiplication_x:           |

## Build

Before building, run the deployment scripts:

```
python3 deploy_content.py
python3 deploy_deps.py

```

These scripts download external third-party dependencies. Please check [Dependencies](./LICENSE.md) for the full list.

* Windows

```
cd build
cmake .. -G "Visual Studio 17 2022"
```

To enable Direct3D 12 support on Windows (requires Windows 10 SDK):

```
cd build
cmake .. -G "Visual Studio 17 2022" -DIGL_WITH_D3D12=ON
```

Note: D3D12 support requires:
- Windows 10 SDK (version 10.0.22621.0 or later recommended)
- Visual Studio 2022 with C++ development tools
- Windows 10 version 1909 or later

* Linux

```
sudo apt-get install clang xorg-dev libxinerama-dev libxcursor-dev libgles2-mesa-dev libegl1-mesa-dev libglfw3-dev libglew-dev libstdc++-12-dev
cd build
cmake .. -G "Unix Makefiles"
```

* macOS

```
cd build
cmake .. -G "Xcode" -DIGL_WITH_VULKAN=OFF
```

* iOS

```
cd build
cmake .. -G Xcode -DCMAKE_TOOLCHAIN_FILE=../third-party/deps/src/ios-cmake/ios.toolchain.cmake -DDEPLOYMENT_TARGET=13.0 -DPLATFORM=OS64
```

* Android

The Gradle project is located within the [build/android](./build/android/) folder.

* WebAssembly

Please install [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) and [Ninja](https://ninja-build.org/).

```
cd build
emcmake cmake .. -G Ninja
cmake --build .
```

## Screenshots

![image](.github/screenshot01.png)

![image](.github/screenshot02.png)

## License

IGL is released under the MIT license, see [LICENSE.md](./LICENSE.md) for the full text as well as third-party library
acknowledgements. SparkSL Compiler is released under the SparkSL Compiler License, see [LICENSE](https://github.com/facebook/igl/releases/download/SparkSL/SparkSL.LICENSE) for full text.
