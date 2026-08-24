use std::{
    ffi::{CStr, CString, c_char},
    fs,
    path::PathBuf,
    thread,
};

#[repr(C)]
struct Report {
    committed: bool,
    finished: bool,
    title_changed: bool,
    uri_changed: bool,
    buffers_changed: bool,
    buffer_rendered: bool,
    buffer_released: bool,
    web_process_terminated: bool,
    sandbox_tools_available: bool,
    sandbox_verified: bool,
    explicit_sync: bool,
    dma_buf_advertised: bool,
    egl_imported: bool,
    termination_reason: i32,
    platform_major: u32,
    platform_minor: u32,
    platform_micro: u32,
    width: u32,
    height: u32,
    format: u32,
    stride: u32,
    planes: u32,
    preferred_format: u32,
    preferred_format_count: u32,
    views: u32,
    fd_baseline: u32,
    fd_after: u32,
    modifier: u64,
    preferred_modifier: u64,
    dma_buf_fds: [i32; 4],
    offsets: [u32; 4],
    strides: [u32; 4],
    buffer_kind: [c_char; 16],
    primary_node: [c_char; 96],
    render_node: [c_char; 96],
}

impl Default for Report {
    fn default() -> Self {
        // Every field is a C-compatible scalar or integer array, so all-zero
        // initialization is the matching initial state for the C report.
        unsafe { std::mem::zeroed() }
    }
}

#[link(name = "fjord_wpe_smoke", kind = "static")]
#[link(name = "WPEWebKit-2.0")]
#[link(name = "wpe-1.0")]
#[link(name = "gobject-2.0")]
#[link(name = "gio-2.0")]
#[link(name = "glib-2.0")]
#[link(name = "EGL")]
unsafe extern "C" {
    fn fjord_wpe_smoke_run(
        data_directory: *const c_char,
        cache_directory: *const c_char,
        uri: *const c_char,
        views: u32,
        report: *mut Report,
        error_message: *mut *mut c_char,
    ) -> i32;
    fn fjord_wpe_smoke_free_error(error_message: *mut c_char);
    fn fjord_wpe_smoke_close_fds(report: *mut Report);
    fn fjord_wpe_smoke_report_size() -> usize;
}

fn profile_directory() -> PathBuf {
    std::env::temp_dir().join(format!("fjord-wpe-smoke-{}", std::process::id()))
}

fn run_smoke(profile: PathBuf, uri: Option<&str>, views: u32) -> Result<Report, String> {
    if std::mem::size_of::<Report>() != unsafe { fjord_wpe_smoke_report_size() } {
        return Err("Rust and C WPE smoke report layouts differ".into());
    }

    let data = profile.join("data");
    let cache = profile.join("cache");
    let data =
        CString::new(data.to_string_lossy().as_bytes()).map_err(|error| error.to_string())?;
    let cache =
        CString::new(cache.to_string_lossy().as_bytes()).map_err(|error| error.to_string())?;
    let uri = uri
        .map(|uri| CString::new(uri).map_err(|error| error.to_string()))
        .transpose()?;
    let mut report = Report::default();
    let mut error_message = std::ptr::null_mut();

    // The C shim owns every GLib and WPE object on this dedicated thread.
    let result = unsafe {
        fjord_wpe_smoke_run(
            data.as_ptr(),
            cache.as_ptr(),
            uri.as_ref().map_or(std::ptr::null(), |uri| uri.as_ptr()),
            views,
            &mut report,
            &mut error_message,
        )
    };

    if result == 0 {
        Ok(report)
    } else {
        let message = if error_message.is_null() {
            "WPE smoke failed without an error message".to_owned()
        } else {
            let message = unsafe { CStr::from_ptr(error_message) }
                .to_string_lossy()
                .into_owned();
            unsafe { fjord_wpe_smoke_free_error(error_message) };
            message
        };
        Err(message)
    }
}

fn run_stress(profile: PathBuf, uri: Option<&str>) -> Result<Report, String> {
    run_smoke(profile, uri, 7)
}

fn main() -> Result<(), String> {
    let mut uri = None;
    let mut stress = false;
    let mut hardware = false;

    for argument in std::env::args().skip(1) {
        match argument.as_str() {
            "--network" => uri = Some("https://example.com/"),
            "--stress" => stress = true,
            "--hardware" => hardware = true,
            _ => return Err(format!("unknown argument: {argument}")),
        }
    }

    let profile = profile_directory();
    let worker_profile = profile.clone();
    let result = thread::Builder::new()
        .name("fjord-wpe-smoke".into())
        .spawn(move || {
            if stress {
                run_stress(worker_profile, uri).map(|report| (report, true))
            } else {
                run_smoke(worker_profile, uri, 1).map(|report| (report, false))
            }
        })
        .map_err(|error| error.to_string())?
        .join()
        .map_err(|_| "WPE smoke thread panicked".to_owned())?;
    let cleanup = fs::remove_dir_all(&profile).map_err(|error| error.to_string());
    let (mut report, stress) = result?;
    cleanup?;

    let buffer_kind = unsafe { CStr::from_ptr(report.buffer_kind.as_ptr()) }
        .to_string_lossy()
        .into_owned();
    println!("committed={}", report.committed);
    println!("finished={}", report.finished);
    println!("title_changed={}", report.title_changed);
    println!("uri_changed={}", report.uri_changed);
    println!("buffers_changed={}", report.buffers_changed);
    println!("buffer_rendered={}", report.buffer_rendered);
    println!("buffer_released={}", report.buffer_released);
    println!("web_process_terminated={}", report.web_process_terminated);
    println!("sandbox_tools_available={}", report.sandbox_tools_available);
    println!("sandbox_verified={}", report.sandbox_verified);
    println!(
        "platform_version={}.{}.{}",
        report.platform_major, report.platform_minor, report.platform_micro
    );
    println!("explicit_sync={}", report.explicit_sync);
    println!("buffer_kind={buffer_kind}");
    println!("buffer_size={}x{}", report.width, report.height);
    println!("format=0x{:08x}", report.format);
    println!("stride={}", report.stride);
    println!("planes={}", report.planes);
    println!("modifier=0x{:016x}", report.modifier);
    println!("dma_buf_advertised={}", report.dma_buf_advertised);
    println!("egl_imported={}", report.egl_imported);
    println!("preferred_format_count={}", report.preferred_format_count);
    println!("preferred_format=0x{:08x}", report.preferred_format);
    println!("preferred_modifier=0x{:016x}", report.preferred_modifier);
    println!(
        "primary_node={}",
        unsafe { CStr::from_ptr(report.primary_node.as_ptr()) }.to_string_lossy()
    );
    println!(
        "render_node={}",
        unsafe { CStr::from_ptr(report.render_node.as_ptr()) }.to_string_lossy()
    );
    if report.web_process_terminated {
        println!("termination_reason={}", report.termination_reason);
    }
    if stress {
        println!("in_process_views={}", report.views);
        println!("fd_baseline={}", report.fd_baseline);
        println!("fd_after={}", report.fd_after);
    }
    if !report.sandbox_tools_available {
        return Err("WPE sandbox tools are unavailable".into());
    }
    if !report.sandbox_verified {
        return Err("WPE sandbox did not create an isolated child process".into());
    }
    if hardware && (buffer_kind != "dma-buf" || !report.egl_imported) {
        return Err("WPE hardware run did not import a dma-buf through EGL".into());
    }
    if !report.committed
        || !report.finished
        || !report.title_changed
        || !report.uri_changed
        || !report.buffers_changed
        || !report.buffer_rendered
        || !report.buffer_released
        || (!stress && !report.web_process_terminated)
    {
        return Err("WPE smoke report is missing required lifecycle events".into());
    }

    unsafe { fjord_wpe_smoke_close_fds(&mut report) };
    Ok(())
}
