// SPDX-License-Identifier: LGPL-2.1-or-later
mod about_view;
mod connection;
mod log_view;
mod profile_store;

use about_view::AboutView;
use connection::ConnectionScreen;
use log_view::LogView;
use profile_store::ProfileStore;

use gtk4::prelude::*;
use gtk4::CssProvider;
use libadwaita::{Application, ApplicationWindow, HeaderBar, ViewStack, ViewSwitcherBar};

use std::cell::RefCell;
use std::rc::Rc;

const APP_ID: &str = "com.openlawsvpn.gui";

const STYLE: &str = "
.log-view {
    background-color: #f5f5f5;
    font-family: monospace;
    font-size: 11px;
}
@media (prefers-color-scheme: dark) {
    .log-view {
        background-color: #1e1e1e;
    }
}
";

fn main() {
    env_logger::init();

    let app = Application::builder()
        .application_id(APP_ID)
        .build();

    app.connect_startup(|_| {
        libadwaita::init().expect("Failed to initialize libadwaita");
    });

    app.connect_activate(|app| {
        let provider = CssProvider::new();
        provider.load_from_string(STYLE);
        gtk4::style_context_add_provider_for_display(
            &gtk4::gdk::Display::default().expect("no display"),
            &provider,
            gtk4::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
        build_ui(app);
    });

    app.run();
}

fn build_ui(app: &Application) {
    let store = Rc::new(RefCell::new(ProfileStore::new()));

    let connection_screen = ConnectionScreen::new(store.clone());
    let log_view = LogView::new();
    let about_view = AboutView::new();

    let stack = ViewStack::new();

    let conn_page = stack.add_titled(
        connection_screen.borrow().get_widget(),
        Some("connect"),
        "Connect",
    );
    conn_page.set_icon_name(Some("network-vpn-symbolic"));

    let log_page = stack.add_titled(&log_view.widget, Some("log"), "Log");
    log_page.set_icon_name(Some("dialog-information-symbolic"));

    let about_page = stack.add_titled(&about_view.widget, Some("about"), "About");
    about_page.set_icon_name(Some("help-about-symbolic"));

    // Plain title in the header bar — tabs live only in the bottom bar (like Android).
    let header = HeaderBar::new();
    let title_label = gtk4::Label::new(Some("openlawsvpn"));
    title_label.set_css_classes(&["title"]);
    header.set_title_widget(Some(&title_label));

    let switcher_bar = ViewSwitcherBar::new();
    switcher_bar.set_stack(Some(&stack));
    switcher_bar.set_reveal(true);

    let content = gtk4::Box::new(gtk4::Orientation::Vertical, 0);
    content.append(&header);
    content.append(&stack);
    content.append(&switcher_bar);

    let window = ApplicationWindow::builder()
        .application(app)
        .title("openlawsvpn")
        .default_width(480)
        .default_height(640)
        .content(&content)
        .build();

    window.present();
}
