use std::ffi::c_uint;

unsafe extern "C" {
    fn webkit_get_major_version() -> c_uint;
    fn webkit_get_minor_version() -> c_uint;
    fn webkit_get_micro_version() -> c_uint;
}

pub const BUILD_VERSION: &str = env!("FJORD_WPE_BUILD_VERSION");

#[must_use]
pub fn version() -> (u32, u32, u32) {
    // These process-global version functions have no arguments or mutable state.
    unsafe {
        (
            webkit_get_major_version(),
            webkit_get_minor_version(),
            webkit_get_micro_version(),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::{BUILD_VERSION, version};

    #[test]
    fn links_supported_wpe_version() {
        let (major, minor, _) = version();

        assert_eq!((major, minor), (2, 52));
        assert!(BUILD_VERSION.starts_with("2.52."));
    }
}
