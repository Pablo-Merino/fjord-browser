use gpui::{
    App, Context, MouseButton, MouseDownEvent, MouseUpEvent, Render, Window, WindowOptions, div,
    prelude::*,
};
use gpui_platform::application;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle, RawDisplayHandle, RawWindowHandle};

struct Fjord {
    handles_logged: bool,
    bridge: Option<fjord_webkit::WaylandSubsurfaceBridge>,
}

impl Render for Fjord {
    fn render(&mut self, window: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        if !self.handles_logged {
            println!("GPUI Wayland handles ready");
            self.handles_logged = true;

            let display = window
                .display_handle()
                .expect("GPUI window has a display handle");
            let surface = window
                .window_handle()
                .expect("GPUI window has a surface handle");

            let RawDisplayHandle::Wayland(display) = display.as_raw() else {
                eprintln!("GPUI Wayland subsurface bridge unavailable: non-Wayland display");
                return div().size_full();
            };
            let RawWindowHandle::Wayland(surface) = surface.as_raw() else {
                eprintln!("GPUI Wayland subsurface bridge unavailable: non-Wayland surface");
                return div().size_full();
            };

            self.bridge = unsafe {
                fjord_webkit::WaylandSubsurfaceBridge::new(
                    display.display.as_ptr(),
                    surface.surface.as_ptr(),
                )
            }
            .map_or_else(
                |error| {
                    eprintln!("GPUI Wayland subsurface bridge unavailable: {error}");
                    None
                },
                |bridge| {
                    println!("GPUI Wayland live WPE subsurface bridge ready");
                    Some(bridge)
                },
            );
            if self.bridge.is_some() {
                cx.on_next_frame(window, Fjord::pump_bridge);
            }
        }

        div()
            .size_full()
            .on_mouse_down(MouseButton::Left, cx.listener(Self::forward_pointer_down))
            .on_mouse_up(MouseButton::Left, cx.listener(Self::forward_pointer_up))
    }
}

impl Fjord {
    fn forward_pointer_down(
        &mut self,
        event: &MouseDownEvent,
        _window: &mut Window,
        _cx: &mut Context<Self>,
    ) {
        self.forward_pointer_button(true, event.position.x.to_f64(), event.position.y.to_f64());
    }

    fn forward_pointer_up(
        &mut self,
        event: &MouseUpEvent,
        _window: &mut Window,
        _cx: &mut Context<Self>,
    ) {
        self.forward_pointer_button(false, event.position.x.to_f64(), event.position.y.to_f64());
    }

    fn forward_pointer_button(&mut self, pressed: bool, x: f64, y: f64) {
        if let Some(bridge) = &mut self.bridge
            && let Err(error) = bridge.pointer_button(pressed, x, y)
        {
            eprintln!("GPUI Wayland subsurface bridge pointer forwarding failed: {error}");
        }
    }

    fn pump_bridge(&mut self, window: &mut Window, cx: &mut Context<Self>) {
        if let Some(bridge) = &mut self.bridge {
            if let Err(error) = bridge.pump() {
                eprintln!("GPUI Wayland subsurface bridge failed: {error}");
                self.bridge = None;
                return;
            }
            cx.on_next_frame(window, Fjord::pump_bridge);
        }
    }
}

fn main() {
    application().run(|cx: &mut App| {
        cx.open_window(WindowOptions::default(), |_, cx| {
            cx.new(|_| Fjord {
                handles_logged: false,
                bridge: None,
            })
        })
        .expect("open Fjord window");
        cx.activate(true);
    });
}
