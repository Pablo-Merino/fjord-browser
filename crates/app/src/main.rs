use gpui::{App, Context, Render, Window, WindowOptions, div, prelude::*};
use gpui_platform::application;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle, RawDisplayHandle, RawWindowHandle};

struct Fjord {
    handles_logged: bool,
}

impl Render for Fjord {
    fn render(&mut self, window: &mut Window, _cx: &mut Context<Self>) -> impl IntoElement {
        if !self.handles_logged {
            let display = window
                .display_handle()
                .expect("GPUI window has a display handle");
            let surface = window
                .window_handle()
                .expect("GPUI window has a surface handle");

            let RawDisplayHandle::Wayland(display) = display.as_raw() else {
                panic!("GPUI window is not backed by Wayland");
            };
            let RawWindowHandle::Wayland(surface) = surface.as_raw() else {
                panic!("GPUI window is not backed by Wayland");
            };

            unsafe {
                fjord_webkit::probe_wayland_subsurface(
                    display.display.as_ptr(),
                    surface.surface.as_ptr(),
                )
            }
            .expect("GPUI Wayland subsurface probe");
            println!("GPUI Wayland subsurface protocol ready");
            self.handles_logged = true;
        }

        div().size_full()
    }
}

fn main() {
    application().run(|cx: &mut App| {
        cx.open_window(WindowOptions::default(), |_, cx| {
            cx.new(|_| Fjord {
                handles_logged: false,
            })
        })
        .expect("open Fjord window");
        cx.activate(true);
    });
}
