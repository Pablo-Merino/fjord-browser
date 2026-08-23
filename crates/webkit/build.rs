fn main() {
    let library = pkg_config::Config::new()
        .atleast_version("2.52")
        .probe("wpe-webkit-2.0")
        .expect("WPE WebKit 2.52.x development files are required");

    println!(
        "cargo:rustc-env=FJORD_WPE_BUILD_VERSION={}",
        library.version
    );
}
