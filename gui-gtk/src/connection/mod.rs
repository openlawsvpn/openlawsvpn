// SPDX-License-Identifier: LGPL-2.1-or-later
pub mod profile;
pub mod state;

pub use profile::Profile;
pub use state::ConnectionState;

use gtk4::prelude::*;
use gtk4::{
    Box as GtkBox, Button, FileDialog, Label, ListBox, Orientation,
    ScrolledWindow, SelectionMode, Spinner,
};
use libadwaita::prelude::*;
use libadwaita::{ActionRow, Toast, ToastOverlay};

use std::cell::RefCell;
use std::rc::Rc;

use crate::profile_store::ProfileStore;

pub struct ConnectionScreen {
    pub widget: GtkBox,
    pub toast_overlay: ToastOverlay,
    store: Rc<RefCell<ProfileStore>>,
    list_box: ListBox,
    empty_label: Label,
    active_id: Rc<RefCell<Option<String>>>,
    state: Rc<RefCell<ConnectionState>>,
}

impl ConnectionScreen {
    pub fn new(store: Rc<RefCell<ProfileStore>>) -> Rc<RefCell<Self>> {
        // Root container
        let root = GtkBox::new(Orientation::Vertical, 0);

        // Header
        let header = Label::new(Some("openlawsvpn"));
        header.set_css_classes(&["title-1"]);
        header.set_xalign(0.0);
        header.set_margin_start(16);
        header.set_margin_end(16);
        header.set_margin_top(24);
        header.set_margin_bottom(8);
        root.append(&header);

        // Toast overlay wraps everything below header
        let toast_overlay = ToastOverlay::new();
        let content = GtkBox::new(Orientation::Vertical, 0);
        toast_overlay.set_child(Some(&content));
        toast_overlay.set_vexpand(true);
        root.append(&toast_overlay);

        // Empty state label
        let empty_label = Label::new(Some(
            "No VPN profiles yet\n\nImport an .ovpn file to get started.",
        ));
        empty_label.set_css_classes(&["dim-label"]);
        empty_label.set_justify(gtk4::Justification::Center);
        empty_label.set_vexpand(true);
        empty_label.set_valign(gtk4::Align::Center);
        content.append(&empty_label);

        // Profile list inside scrolled window
        let scrolled = ScrolledWindow::new();
        scrolled.set_vexpand(true);
        scrolled.set_policy(gtk4::PolicyType::Never, gtk4::PolicyType::Automatic);

        let list_box = ListBox::new();
        list_box.set_selection_mode(SelectionMode::None);
        list_box.set_css_classes(&["boxed-list"]);
        list_box.set_margin_start(16);
        list_box.set_margin_end(16);
        list_box.set_margin_top(8);
        list_box.set_margin_bottom(8);
        scrolled.set_child(Some(&list_box));
        content.append(&scrolled);

        // Import button
        let import_btn = Button::with_label("+ Add Profile");
        import_btn.set_css_classes(&["pill"]);
        import_btn.set_margin_start(16);
        import_btn.set_margin_end(16);
        import_btn.set_margin_top(8);
        import_btn.set_margin_bottom(16);
        content.append(&import_btn);

        let screen = Rc::new(RefCell::new(Self {
            widget: root,
            toast_overlay,
            store,
            list_box,
            empty_label,
            active_id: Rc::new(RefCell::new(None)),
            state: Rc::new(RefCell::new(ConnectionState::Idle)),
        }));

        // Wire import button
        {
            let screen_clone = screen.clone();
            import_btn.connect_clicked(move |btn| {
                let win = btn.root().and_downcast::<gtk4::Window>();
                Self::open_file_picker(win, screen_clone.clone());
            });
        }

        screen.borrow().refresh_list();
        screen
    }

    fn refresh_list(&self) {
        // Remove all existing rows
        while let Some(child) = self.list_box.first_child() {
            self.list_box.remove(&child);
        }

        let profiles = self.store.borrow().list();

        if profiles.is_empty() {
            self.empty_label.set_visible(true);
            self.list_box.parent().and_downcast::<ScrolledWindow>()
                .map(|s| s.set_visible(false));
        } else {
            self.empty_label.set_visible(false);
            self.list_box.parent().and_downcast::<ScrolledWindow>()
                .map(|s| s.set_visible(true));

            let active_id = self.active_id.clone();
            let state = self.state.clone();

            for profile in &profiles {
                let row = self.build_profile_row(profile, &active_id, &state);
                self.list_box.append(&row);
            }
        }
    }

    fn build_profile_row(
        &self,
        profile: &Profile,
        active_id: &Rc<RefCell<Option<String>>>,
        state: &Rc<RefCell<ConnectionState>>,
    ) -> ActionRow {
        let row = ActionRow::new();
        row.set_title(&profile.name);

        // Spinner (shown during transitions)
        let spinner = Spinner::new();
        spinner.set_visible(false);
        row.add_prefix(&spinner);

        // Action button
        let btn = Button::with_label("Connect");
        btn.set_css_classes(&["suggested-action", "pill"]);
        btn.set_valign(gtk4::Align::Center);
        row.add_suffix(&btn);

        // Right-click context menu for delete
        let gesture = gtk4::GestureClick::new();
        gesture.set_button(3); // right click
        let profile_id = profile.id.clone();
        let profile_name = profile.name.clone();
        let store = self.store.clone();
        let toast_overlay = self.toast_overlay.clone();

        {
            let profile_id = profile_id.clone();
            let profile_name = profile_name.clone();
            let store = store.clone();
            gesture.connect_pressed(move |_, _, x, y| {
                let menu = gtk4::PopoverMenu::from_model(None::<&gtk4::gio::MenuModel>);
                let menu_box = GtkBox::new(Orientation::Vertical, 4);
                let delete_btn = Button::with_label("Delete");
                delete_btn.set_css_classes(&["destructive-action"]);
                menu_box.append(&delete_btn);
                menu.set_child(Some(&menu_box));
                menu.set_pointing_to(Some(&gtk4::gdk::Rectangle::new(x as i32, y as i32, 1, 1)));

                let store = store.clone();
                let profile_id = profile_id.clone();
                let profile_name = profile_name.clone();
                let toast_overlay = toast_overlay.clone();
                let menu_clone = menu.clone();
                delete_btn.connect_clicked(move |_| {
                    store.borrow_mut().delete(&profile_id);
                    let toast = Toast::new(&format!("Profile '{}' deleted.", profile_name));
                    toast_overlay.add_toast(toast);
                    menu_clone.popdown();
                });

                menu.popup();
            });
        }
        row.add_controller(gesture);

        // Connect/Disconnect button logic
        {
            let active_id = active_id.clone();
            let state = state.clone();
            let profile_id_btn = profile_id.clone();
            btn.connect_clicked(move |_| {
                let current_active = active_id.borrow().clone();
                let current_state = state.borrow().clone();

                if current_active.as_deref() == Some(&profile_id_btn)
                    && matches!(current_state, ConnectionState::Connected { .. })
                {
                    // Disconnect
                    *state.borrow_mut() = ConnectionState::Disconnecting;
                    *active_id.borrow_mut() = None;
                } else {
                    // Connect
                    *active_id.borrow_mut() = Some(profile_id_btn.clone());
                    *state.borrow_mut() = ConnectionState::Connecting;
                }
            });
        }

        row
    }

    fn open_file_picker(
        parent: Option<gtk4::Window>,
        screen: Rc<RefCell<ConnectionScreen>>,
    ) {
        let dialog = FileDialog::new();
        dialog.set_title("Import .ovpn Profile");
        dialog.set_modal(true);

        // Filter: show only .ovpn files in Nautilus / GTK file picker
        let filter = gtk4::FileFilter::new();
        filter.set_name(Some("OpenVPN profiles (*.ovpn)"));
        filter.add_suffix("ovpn");
        let filters = gtk4::gio::ListStore::new::<gtk4::FileFilter>();
        filters.append(&filter);
        dialog.set_filters(Some(&filters));
        dialog.set_default_filter(Some(&filter));

        dialog.open(parent.as_ref(), gtk4::gio::Cancellable::NONE, move |result| {
            if let Ok(file) = result {
                let path = file.path().unwrap_or_default();
                let name = path
                    .file_name()
                    .and_then(|n| n.to_str())
                    .unwrap_or("profile")
                    .trim_end_matches(".ovpn")
                    .to_string();

                match std::fs::read_to_string(&path) {
                    Ok(content) => {
                        let s = screen.borrow();
                        s.store.borrow_mut().import(&name, &content);
                        let toast = Toast::new(&format!("Profile '{}' imported.", name));
                        s.toast_overlay.add_toast(toast);
                        drop(s);
                        screen.borrow().refresh_list();
                    }
                    Err(e) => {
                        let s = screen.borrow();
                        let toast = Toast::new(&format!("Import failed: {}", e));
                        s.toast_overlay.add_toast(toast);
                    }
                }
            }
        });
    }

    pub fn get_widget(&self) -> &GtkBox {
        &self.widget
    }
}
