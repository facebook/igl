use std::env;
use std::path::PathBuf;

fn main() {
    // Get the project root (5 levels up from igl-sys)
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let project_root = manifest_dir
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .parent()
        .unwrap();

    let build_dir = project_root.join("build");

    // Tell cargo to look for libraries in the build directory
    println!("cargo:rustc-link-search=native={}/shell/rust/igl-c-wrapper/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/shell/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/shell/mac/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/src/igl/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/src/igl/metal/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/src/igl/opengl/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/src/igl/glslang/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/src/igl/glslang/glslang/SPIRV/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/src/igl/glslang/glslang/glslang/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/IGLU/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/IGLU/SPIRV-Cross/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/fmt/Debug", build_dir.display());
    println!("cargo:rustc-link-search=native={}/Debug", build_dir.display());

    // Link the C wrapper library
    println!("cargo:rustc-link-lib=static=igl_c_wrapper");

    // Link IGL libraries
    println!("cargo:rustc-link-lib=static=IGLShellShared");
    println!("cargo:rustc-link-lib=static=IGLShellPlatform");
    println!("cargo:rustc-link-lib=static=IGLLibrary");
    println!("cargo:rustc-link-lib=static=IGLMetal");
    println!("cargo:rustc-link-lib=static=IGLOpenGL");
    println!("cargo:rustc-link-lib=static=IGLGlslang");
    println!("cargo:rustc-link-lib=static=SPIRV");
    println!("cargo:rustc-link-lib=static=glslang");
    println!("cargo:rustc-link-lib=static=glslang-default-resource-limits");
    println!("cargo:rustc-link-lib=static=IGLUimgui");
    println!("cargo:rustc-link-lib=static=IGLUmanagedUniformBuffer");
    println!("cargo:rustc-link-lib=static=IGLUsimple_renderer");
    println!("cargo:rustc-link-lib=static=IGLUshaderCross");
    println!("cargo:rustc-link-lib=static=spirv-cross-util");
    println!("cargo:rustc-link-lib=static=spirv-cross-c");
    println!("cargo:rustc-link-lib=static=spirv-cross-msl");
    println!("cargo:rustc-link-lib=static=spirv-cross-glsl");
    println!("cargo:rustc-link-lib=static=spirv-cross-core");
    println!("cargo:rustc-link-lib=static=IGLUtexture_accessor");
    println!("cargo:rustc-link-lib=static=IGLUtexture_loader");
    println!("cargo:rustc-link-lib=static=IGLUuniform");
    println!("cargo:rustc-link-lib=static=IGLstb");
    println!("cargo:rustc-link-lib=static=fmtd");

    // Link system frameworks
    println!("cargo:rustc-link-lib=framework=Metal");
    println!("cargo:rustc-link-lib=framework=MetalKit");
    println!("cargo:rustc-link-lib=framework=AppKit");
    println!("cargo:rustc-link-lib=framework=QuartzCore");
    println!("cargo:rustc-link-lib=framework=CoreGraphics");
    println!("cargo:rustc-link-lib=framework=Foundation");

    // Link C++ standard library
    println!("cargo:rustc-link-lib=c++");

    // Rebuild if the C wrapper changes
    println!("cargo:rerun-if-changed=../igl-c-wrapper/include/igl_c_wrapper.h");
    println!("cargo:rerun-if-changed=../igl-c-wrapper/src/igl_c_wrapper.cpp");
}
