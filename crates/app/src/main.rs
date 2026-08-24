use gpui::{App, Context, Render, Window, WindowOptions, div, prelude::*};
use gpui_platform::application;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle, RawDisplayHandle, RawWindowHandle};

struct Fjord {
    handles_logged: bool,
    subsurface_probe_pending: bool,
}

impl Render for Fjord {
    fn render(&mut self, window: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        if !self.handles_logged {
            println!("GPUI Wayland handles ready");
            self.handles_logged = true;
            cx.on_next_frame(window, |view, _window, cx| {
                view.subsurface_probe_pending = true;
                cx.notify();
            });
        } else if self.subsurface_probe_pending {
            let display = window
                .display_handle()
                .expect("GPUI window has a display handle");
            let surface = window
                .window_handle()
                .expect("GPUI window has a surface handle");

            let RawDisplayHandle::Wayland(display) = display.as_raw() else {
                eprintln!("GPUI Wayland subsurface probe unavailable: non-Wayland display");
                self.subsurface_probe_pending = false;
                return div().size_full();
            };
            let RawWindowHandle::Wayland(surface) = surface.as_raw() else {
                eprintln!("GPUI Wayland subsurface probe unavailable: non-Wayland surface");
                self.subsurface_probe_pending = false;
                return div().size_full();
            };

            unsafe {
                fjord_webkit::probe_wayland_subsurface(
                    display.display.as_ptr(),
                    surface.surface.as_ptr(),
                )
            }
            .map_or_else(
                |error| eprintln!("GPUI Wayland subsurface probe unavailable: {error}"),
                |_| println!("GPUI Wayland subsurface protocol ready"),
            );
            self.subsurface_probe_pending = false;
        }

        div().size_full()
    }
}

fn main() {
    application().run(|cx: &mut App| {
        cx.open_window(WindowOptions::default(), |_, cx| {
            cx.new(|_| Fjord {
                handles_logged: false,
                subsurface_probe_pending: false,
            })
        })
        .expect("open Fjord window");
        cx.activate(true);
    });
}
