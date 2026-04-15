// SPDX-License-Identifier: LGPL-2.1-or-later
use gtk4::prelude::*;
use gtk4::{ScrolledWindow, TextView, TextBuffer};

const NOTICE: &str = include_str!("../notice.txt");

pub struct AboutView {
    pub widget: ScrolledWindow,
}

impl AboutView {
    pub fn new() -> Self {
        let buffer = TextBuffer::new(None);
        buffer.set_text(NOTICE);

        let text_view = TextView::with_buffer(&buffer);
        text_view.set_editable(false);
        text_view.set_monospace(true);
        text_view.set_css_classes(&["log-view"]);
        text_view.set_left_margin(16);
        text_view.set_right_margin(16);
        text_view.set_top_margin(16);
        text_view.set_bottom_margin(16);
        text_view.set_wrap_mode(gtk4::WrapMode::Word);

        let scrolled = ScrolledWindow::new();
        scrolled.set_vexpand(true);
        scrolled.set_child(Some(&text_view));

        Self { widget: scrolled }
    }
}
