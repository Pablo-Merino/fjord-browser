use gpui::{
    App, Context, FocusHandle, KeyDownEvent, KeyUpEvent, Keystroke, MouseButton, MouseDownEvent,
    MouseUpEvent, Render, ScrollDelta, ScrollWheelEvent, Window, WindowOptions, div, prelude::*,
};
use gpui_platform::application;
use raw_window_handle::{HasDisplayHandle, HasWindowHandle, RawDisplayHandle, RawWindowHandle};
use std::collections::HashMap;
use xkbcommon::xkb::keysyms::{
    KEY_BackSpace, KEY_Down, KEY_Escape, KEY_Left, KEY_Return, KEY_Right, KEY_Tab, KEY_Up,
};

struct Fjord {
    handles_logged: bool,
    bridge: Option<fjord_webkit::WaylandSubsurfaceBridge>,
    focus: FocusHandle,
    forwarded_keys: HashMap<String, u32>,
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
            .track_focus(&self.focus)
            .on_mouse_down(MouseButton::Left, cx.listener(Self::forward_pointer_down))
            .on_mouse_up(MouseButton::Left, cx.listener(Self::forward_pointer_up))
            .on_scroll_wheel(cx.listener(Self::forward_scroll))
            .on_key_down(cx.listener(Self::forward_key_down))
            .on_key_up(cx.listener(Self::forward_key_up))
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

    fn forward_scroll(
        &mut self,
        event: &ScrollWheelEvent,
        _window: &mut Window,
        _cx: &mut Context<Self>,
    ) {
        let (delta_x, delta_y, precise) = match event.delta {
            ScrollDelta::Pixels(delta) => (delta.x.to_f64(), delta.y.to_f64(), true),
            ScrollDelta::Lines(delta) => (f64::from(delta.x), f64::from(delta.y), false),
        };

        if let Some(bridge) = &mut self.bridge
            && let Err(error) = bridge.scroll(
                event.position.x.to_f64(),
                event.position.y.to_f64(),
                delta_x,
                delta_y,
                precise,
            )
        {
            eprintln!("GPUI Wayland subsurface bridge scroll forwarding failed: {error}");
        }
    }

    fn forward_key_down(
        &mut self,
        event: &KeyDownEvent,
        _window: &mut Window,
        _cx: &mut Context<Self>,
    ) {
        if forwards_to_page(&event.keystroke)
            && let Some(keyval) = keyval(&event.keystroke)
            && self.forward_key(true, keyval)
        {
            self.forwarded_keys
                .insert(event.keystroke.key.clone(), keyval);
        }
    }

    fn forward_key_up(
        &mut self,
        event: &KeyUpEvent,
        _window: &mut Window,
        _cx: &mut Context<Self>,
    ) {
        if let Some(keyval) = self.forwarded_keys.remove(&event.keystroke.key) {
            self.forward_key(false, keyval);
        }
    }

    fn forward_key(&mut self, pressed: bool, keyval: u32) -> bool {
        let Some(bridge) = &mut self.bridge else {
            return false;
        };
        if let Err(error) = bridge.keyboard(pressed, keyval) {
            eprintln!("GPUI Wayland subsurface bridge keyboard forwarding failed: {error}");
            return false;
        }
        true
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
        let focus = cx.focus_handle();
        cx.open_window(WindowOptions::default(), |_, cx| {
            cx.new(move |_| Fjord {
                handles_logged: false,
                bridge: None,
                focus,
                forwarded_keys: HashMap::new(),
            })
        })
        .expect("open Fjord window");
        cx.activate(true);
    });
}

fn forwards_to_page(keystroke: &Keystroke) -> bool {
    !keystroke.modifiers.control && !keystroke.modifiers.alt && !keystroke.modifiers.platform
}

fn keyval(keystroke: &Keystroke) -> Option<u32> {
    if let Some(character) = keystroke.key_char.as_deref()
        && character.len() == 1
        && character.is_ascii()
        && character.as_bytes()[0].is_ascii_graphic()
    {
        return Some(u32::from(character.as_bytes()[0]));
    }

    match keystroke.key.as_str() {
        "space" => Some(u32::from(b' ')),
        "enter" => Some(KEY_Return),
        "backspace" => Some(KEY_BackSpace),
        "tab" => Some(KEY_Tab),
        "escape" => Some(KEY_Escape),
        "left" => Some(KEY_Left),
        "right" => Some(KEY_Right),
        "up" => Some(KEY_Up),
        "down" => Some(KEY_Down),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::{Keystroke, keyval};

    #[test]
    fn maps_printable_and_editing_keys() {
        let mut printable = Keystroke::parse("a").unwrap();
        printable.key_char = Some("a".into());
        assert_eq!(keyval(&printable), Some(u32::from(b'a')));
        assert_eq!(keyval(&Keystroke::parse("enter").unwrap()), Some(0xff0d));
        assert_eq!(keyval(&Keystroke::parse("left").unwrap()), Some(0xff51));
    }
}
