use std::collections::BTreeSet;

fn main() {
    println!("cargo:rerun-if-changed=native/wpe_smoke.c");
    println!("cargo:rerun-if-changed=native/wpe_smoke.h");

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

    let mut native = cc::Build::new();
    native.file("native/wpe_smoke.c").warnings(true);
    for library in [&webkit, &platform, &headless] {
        for include_path in &library.include_paths {
            native.include(include_path);
        }
    }
    native.compile("fjord_wpe_smoke");

    let mut link_paths = BTreeSet::new();
    let mut libraries = BTreeSet::new();
    for library in [&webkit, &platform, &headless] {
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

    println!("cargo:rustc-env=FJORD_WPE_BUILD_VERSION={}", webkit.version);
}
