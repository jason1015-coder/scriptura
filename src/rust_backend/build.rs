fn main() {
    use std::env;

    let qt_include_dir = env::var("SCRIPTURA_QT_INCLUDE_DIR").unwrap_or_else(|_| {
        "/usr/include/x86_64-linux-gnu/qt6".to_string()
    });
    let qt_lib_dir = env::var("SCRIPTURA_QT_LIB_DIR").unwrap_or_else(|_| {
        "/usr/lib/x86_64-linux-gnu".to_string()
    });

    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let qt_bridge_path = format!("{}/../../src/qt_bridge.cpp", manifest_dir);

    let mut build = cc::Build::new();
    build.file(&qt_bridge_path)
        .cpp(true)
        .include(&qt_include_dir)
        .include(format!("{}/QtCore", qt_include_dir))
        .include(format!("{}/QtWidgets", qt_include_dir))
        .include(format!("{}/QtGui", qt_include_dir))
        .include(format!("{}/QtNetwork", qt_include_dir))
        .include(format!("{}/src", manifest_dir))
        .flag("-std=c++17");

    if let Ok(include_core) = env::var("SCRIPTURA_QT_INCLUDE_CORE") {
        build.include(include_core);
    }
    if let Ok(include_gui) = env::var("SCRIPTURA_QT_INCLUDE_GUI") {
        build.include(include_gui);
    }

    build.compile("qt_bridge");

    println!("cargo:rustc-link-search=native={}", qt_lib_dir);
    println!("cargo:rustc-link-lib=dylib=Qt6Core");
    println!("cargo:rustc-link-lib=dylib=Qt6Gui");
    println!("cargo:rustc-link-lib=dylib=Qt6Widgets");
    println!("cargo:rustc-link-lib=dylib=Qt6Network");

    #[cfg(target_os = "linux")]
    {
        println!("cargo:rustc-link-lib=dylib=pthread");
        println!("cargo:rustc-link-lib=dylib=dl");
        println!("cargo:rustc-link-lib=dylib=m");
    }
    #[cfg(target_os = "macos")]
    {
        println!("cargo:rustc-link-framework=CoreFoundation");
        println!("cargo:rustc-link-framework=Security");
        println!("cargo:rustc-link-framework=SystemConfiguration");
    }
    #[cfg(target_os = "windows")]
    {
        println!("cargo:rustc-link-lib=bcrypt");
        println!("cargo:rustc-link-lib=user32");
        println!("cargo:rustc-link-lib=advapi32");
        println!("cargo:rustc-link-lib=ws2_32");
        println!("cargo:rustc-link-lib=crypt32");
        println!("cargo:rustc-link-lib=userenv");
        println!("cargo:rustc-link-lib=ntdll");
        println!("cargo:rustc-link-lib=dwmapi");
    }
}