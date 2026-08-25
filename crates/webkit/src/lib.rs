use std::{
    ffi::{CStr, c_char, c_uint, c_void},
    ptr::NonNull,
};

#[repr(C)]
struct FjordWpeSubsurfaceBridgeOpaque {
    _private: [u8; 0],
}

#[link(name = "fjord_wpe_smoke", kind = "static")]
#[link(name = "EGL")]
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
    fn fjord_wpe_subsurface_bridge_new(
        display: *mut c_void,
        parent_surface: *mut c_void,
        error_message: *mut *mut c_char,
    ) -> *mut FjordWpeSubsurfaceBridgeOpaque;
    fn fjord_wpe_subsurface_bridge_pump(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_resize(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        width: u32,
        height: u32,
        scale: u32,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_switch_tab(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        tab: u32,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_pointer_button(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        pressed: bool,
        x: f64,
        y: f64,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_pointer_move(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        x: f64,
        y: f64,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_scroll(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        x: f64,
        y: f64,
        delta_x: f64,
        delta_y: f64,
        precise: bool,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_keyboard(
        bridge: *mut FjordWpeSubsurfaceBridgeOpaque,
        pressed: bool,
        keyval: u32,
        modifiers: u32,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_subsurface_bridge_free(bridge: *mut FjordWpeSubsurfaceBridgeOpaque);
    fn fjord_wpe_smoke_free_error(error_message: *mut c_char);
}

pub const BUILD_VERSION: &str = env!("FJORD_WPE_BUILD_VERSION");
pub const KEYBOARD_MODIFIER_CONTROL: u32 = 1 << 0;
pub const KEYBOARD_MODIFIER_SHIFT: u32 = 1 << 1;
pub const KEYBOARD_MODIFIER_ALT: u32 = 1 << 2;
pub const KEYBOARD_MODIFIER_META: u32 = 1 << 3;

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

/// A same-thread WPE view attached to a GPUI-owned Wayland child surface.
pub struct WaylandSubsurfaceBridge(NonNull<FjordWpeSubsurfaceBridgeOpaque>);

impl WaylandSubsurfaceBridge {
    /// # Safety
    /// `display` and `parent_surface` must remain live Wayland client pointers
    /// for the lifetime of the returned bridge. Call [`Self::pump`] on that
    /// same thread until the bridge is dropped.
    pub unsafe fn new(display: *mut c_void, parent_surface: *mut c_void) -> Result<Self, String> {
        let mut error_message = std::ptr::null_mut();
        let bridge =
            unsafe { fjord_wpe_subsurface_bridge_new(display, parent_surface, &mut error_message) };

        NonNull::new(bridge)
            .map(Self)
            .ok_or_else(|| take_error(error_message))
    }

    /// Dispatch queued WPE and private-Wayland events without reading or waiting
    /// on the Wayland connection.
    pub fn pump(&mut self) -> Result<(), String> {
        let mut error_message = std::ptr::null_mut();
        let result =
            unsafe { fjord_wpe_subsurface_bridge_pump(self.0.as_ptr(), &mut error_message) };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }

    /// Resize the WPE view in logical pixels at an integer Wayland buffer scale.
    pub fn resize(&mut self, width: u32, height: u32, scale: u32) -> Result<(), String> {
        if width == 0 || height == 0 || scale == 0 {
            return Err("WPE bridge size and scale must be non-zero".into());
        }

        let mut error_message = std::ptr::null_mut();
        let result = unsafe {
            fjord_wpe_subsurface_bridge_resize(
                self.0.as_ptr(),
                width,
                height,
                scale,
                &mut error_message,
            )
        };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }

    /// Select a live development tab.
    pub fn switch_tab(&mut self, tab: u32) -> Result<(), String> {
        let mut error_message = std::ptr::null_mut();
        let result = unsafe {
            fjord_wpe_subsurface_bridge_switch_tab(self.0.as_ptr(), tab, &mut error_message)
        };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }

    /// Inject a left pointer-button press or release at the view coordinates.
    pub fn pointer_button(&mut self, pressed: bool, x: f64, y: f64) -> Result<(), String> {
        if !x.is_finite() || !y.is_finite() {
            return Err("WPE bridge pointer coordinates must be finite".into());
        }

        let mut error_message = std::ptr::null_mut();
        let result = unsafe {
            fjord_wpe_subsurface_bridge_pointer_button(
                self.0.as_ptr(),
                pressed,
                x,
                y,
                &mut error_message,
            )
        };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }

    /// Inject pointer movement at the view coordinates.
    pub fn pointer_move(&mut self, x: f64, y: f64) -> Result<(), String> {
        if !x.is_finite() || !y.is_finite() {
            return Err("WPE bridge pointer coordinates must be finite".into());
        }

        let mut error_message = std::ptr::null_mut();
        let result = unsafe {
            fjord_wpe_subsurface_bridge_pointer_move(self.0.as_ptr(), x, y, &mut error_message)
        };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }

    /// Inject a scroll event at the view coordinates.
    pub fn scroll(
        &mut self,
        x: f64,
        y: f64,
        delta_x: f64,
        delta_y: f64,
        precise: bool,
    ) -> Result<(), String> {
        if !x.is_finite() || !y.is_finite() || !delta_x.is_finite() || !delta_y.is_finite() {
            return Err("WPE bridge scroll values must be finite".into());
        }

        let mut error_message = std::ptr::null_mut();
        let result = unsafe {
            fjord_wpe_subsurface_bridge_scroll(
                self.0.as_ptr(),
                x,
                y,
                delta_x,
                delta_y,
                precise,
                &mut error_message,
            )
        };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }

    /// Inject a keyboard press or release for an XKB keysym.
    pub fn keyboard(&mut self, pressed: bool, keyval: u32, modifiers: u32) -> Result<(), String> {
        if keyval == 0 {
            return Err("WPE bridge keyboard keyval must not be zero".into());
        }

        let mut error_message = std::ptr::null_mut();
        let result = unsafe {
            fjord_wpe_subsurface_bridge_keyboard(
                self.0.as_ptr(),
                pressed,
                keyval,
                modifiers,
                &mut error_message,
            )
        };

        if result == 0 {
            Ok(())
        } else {
            Err(take_error(error_message))
        }
    }
}

impl Drop for WaylandSubsurfaceBridge {
    fn drop(&mut self) {
        unsafe { fjord_wpe_subsurface_bridge_free(self.0.as_ptr()) };
    }
}

fn take_error(error_message: *mut c_char) -> String {
    if error_message.is_null() {
        return "WPE Wayland subsurface bridge failed without an error message".into();
    }

    let message = unsafe { CStr::from_ptr(error_message) }
        .to_string_lossy()
        .into_owned();
    unsafe { fjord_wpe_smoke_free_error(error_message) };
    message
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
