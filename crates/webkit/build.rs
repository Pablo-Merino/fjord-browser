use std::{collections::BTreeSet, env, path::Path, process::Command};

fn main() {
    println!("cargo:rerun-if-changed=native/wpe_smoke.c");
    println!("cargo:rerun-if-changed=native/wpe_smoke.h");
    println!("cargo:rerun-if-changed=native/fjord_wpe_platform.c");
    println!("cargo:rerun-if-changed=native/fjord_wpe_platform.h");

    let webkit = pkg_config::Config::new()
        .cargo_metadata(false)
        .atleast_version("2.52")
        .probe("wpe-webkit-2.0")
        .expect("WPE WebKit 2.52.x development files are required");
    let platform = pkg_config::Config::new()
        .cargo_metadata(false)
        .atleast_version("2.52")
        .probe("wpe-platform-2.0")
        .expect("WPEPlatform 2.52.x development files are required");
    let headless = pkg_config::Config::new()
        .cargo_metadata(false)
        .atleast_version("2.52")
        .probe("wpe-platform-headless-2.0")
        .expect("WPEPlatform headless 2.52.x development files are required");
    let egl = pkg_config::Config::new()
        .cargo_metadata(false)
        .probe("egl")
        .expect("EGL development files are required");
    let protocol_directory = pkg_config::get_variable("wayland-protocols", "pkgdatadir")
        .expect("wayland-protocols pkg-config metadata is required");
    let protocol =
        Path::new(&protocol_directory).join("unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml");
    let generated_header = env::var_os("OUT_DIR")
        .map(|directory| Path::new(&directory).join("linux-dmabuf-client-protocol.h"))
        .expect("Cargo must set OUT_DIR");
    let generated_source = generated_header.with_extension("c");

    println!("cargo:rerun-if-changed={}", protocol.display());
    if !Command::new("wayland-scanner")
        .args([
            "client-header",
            protocol.to_str().expect("protocol path is UTF-8"),
        ])
        .arg(&generated_header)
        .status()
        .expect("wayland-scanner must be installed")
        .success()
    {
        panic!("failed to generate linux-dmabuf client protocol header");
    }
    if !Command::new("wayland-scanner")
        .args([
            "private-code",
            protocol.to_str().expect("protocol path is UTF-8"),
        ])
        .arg(&generated_source)
        .status()
        .expect("wayland-scanner must be installed")
        .success()
    {
        panic!("failed to generate linux-dmabuf client protocol code");
    }

    let mut native = cc::Build::new();
    native
        .file("native/fjord_wpe_platform.c")
        .file("native/wpe_smoke.c")
        .file(&generated_source)
        .warnings(true);
    native.include(
        generated_header
            .parent()
            .expect("generated header has a parent"),
    );
    let wayland = pkg_config::Config::new()
        .cargo_metadata(false)
        .probe("wayland-client")
        .expect("Wayland client development files are required");
    for library in [&webkit, &platform, &headless, &egl, &wayland] {
        for include_path in &library.include_paths {
            native.include(include_path);
        }
    }
    native.compile("fjord_wpe_smoke");

    let mut link_paths = BTreeSet::new();
    let mut libraries = BTreeSet::new();
    for library in [&webkit, &platform, &headless, &egl, &wayland] {
        for link_path in &library.link_paths {
            link_paths.insert(link_path);
        }
        for name in &library.libs {
            libraries.insert(name);
        }
    }
    for link_path in link_paths {
        println!("cargo:rustc-link-search=native={}", link_path.display());
    }
    for library in libraries {
        println!("cargo:rustc-link-lib=dylib={library}");
    }
    println!("cargo:rustc-link-lib=dylib=EGL");

    println!("cargo:rustc-env=FJORD_WPE_BUILD_VERSION={}", webkit.version);
}
