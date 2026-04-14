# Play Store Data Safety — OpenLaws VPN (Android)

Answers for the Google Play Console **Data Safety** section.

---

## Does your app collect or share any user data?

**No** — the app does not collect, store, or share any user data with the developer or any third party.

Detailed breakdown by data type:

| Data Type | Collected | Shared | Notes |
|---|---|---|---|
| Location (precise or approximate) | No | No | No location permission requested |
| Personal info (name, email, address, etc.) | No | No | |
| Financial info | No | No | |
| Health & fitness | No | No | |
| Messages (SMS, email, chat) | No | No | |
| Photos & videos | No | No | |
| Audio files | No | No | |
| Files & docs | No | No | |
| Calendar events | No | No | |
| Contacts | No | No | |
| App activity (search history, installed apps, etc.) | No | No | No analytics or telemetry |
| Web browsing history | No | No | |
| App info & performance (crash logs, diagnostics) | No | No | No crash reporting; connection logs held in memory only, never written to disk or transmitted |
| Device or other IDs | No | No | |

**VPN profile files (.ovpn):** stored on-device only, excluded from Android Auto Backup. Never transmitted to the developer.

**SAML authentication:** performed in a Chrome Custom Tab (separate process). The app receives only an opaque SAML token to pass to AWS Client VPN; it never sees or stores user credentials.

**Network traffic:** the app establishes an encrypted VPN tunnel to the user's own AWS Client VPN endpoint. No traffic is routed to or inspected by the developer.

---

## Is all data encrypted in transit?

**Yes** — the only outbound connections are:

1. The encrypted VPN tunnel to the user's own AWS Client VPN endpoint (TLS).
2. SAML authentication via a Chrome Custom Tab (HTTPS).

No unencrypted network connections are made by the app.

---

## Can users request that their data be deleted?

**Not applicable** — the app holds no user data on developer servers, so there is nothing for the developer to delete. All data (VPN profiles, in-memory logs) exists solely on the user's device and can be removed by uninstalling the app or clearing app data.

For any questions, contact: security@openlawsvpn.com

---

## Has the app been independently security audited or verified?

**No** — the app has not undergone a formal third-party security audit at this time.

The full source code is publicly available and open source (LGPL-2.1-or-later with usage exception), allowing independent review by anyone:
https://github.com/openlawsvpn/openlawsvpn

---

## Permissions used

| Permission | Purpose |
|---|---|
| `INTERNET` | Establish VPN tunnel and SAML auth |
| `BIND_VPN_SERVICE` / `VpnService` | Create and manage the VPN interface |
| `FOREGROUND_SERVICE` / `FOREGROUND_SERVICE_SPECIAL_USE` | Keep VPN connection alive in background |
| `POST_NOTIFICATIONS` | Show persistent VPN connection status notification |
| `ACCESS_NETWORK_STATE` | Detect network changes to handle reconnection |

No location, contacts, camera, microphone, or storage permissions are requested.

---

*Last updated: 2026-04-14*
