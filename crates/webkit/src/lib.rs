use std::ffi::{CStr, c_char, c_uint, c_void};

#[link(name = "fjord_wpe_smoke", kind = "static")]
#[link(name = "EGL")]
#[link(name = "gbm")]
#[link(name = "drm")]
#[link(name = "wayland-client")]
unsafe extern "C" {
    fn webkit_get_major_version() -> c_uint;
    fn webkit_get_minor_version() -> c_uint;
    fn webkit_get_micro_version() -> c_uint;
    fn fjord_wayland_subsurface_probe(
        display: *mut c_void,
        parent_surface: *mut c_void,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_smoke_free_error(error_message: *mut c_char);
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

/// # Safety
/// `display` and `parent_surface` must be live Wayland client pointers owned by
/// the caller for the duration of this call.
pub unsafe fn probe_wayland_subsurface(
    display: *mut c_void,
    parent_surface: *mut c_void,
) -> Result<(), String> {
    let mut error_message = std::ptr::null_mut();
    let result =
        unsafe { fjord_wayland_subsurface_probe(display, parent_surface, &mut error_message) };

    if result == 0 {
        Ok(())
    } else if error_message.is_null() {
        Err("Wayland subsurface probe failed without an error message".into())
    } else {
        let message = unsafe { CStr::from_ptr(error_message) }
            .to_string_lossy()
            .into_owned();
        unsafe { fjord_wpe_smoke_free_error(error_message) };
        Err(message)
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
