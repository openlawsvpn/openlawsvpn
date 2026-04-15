// SPDX-License-Identifier: LGPL-2.1-or-later
//
// SAML capture server — listens on 127.0.0.1:35001 (AWS-hardcoded ACS endpoint).
// Accepts the POST /  with SAMLResponse form field that the IdP redirects to.
// Returns immediately once a token is captured or timeout_secs elapses.

use axum::{
    extract::Form,
    response::Html,
    routing::{get, post},
    Router,
};
use serde::Deserialize;
use std::sync::{Arc, Mutex};
use tokio::sync::oneshot;

const BIND_ADDR: &str = "127.0.0.1:35001";

const SUCCESS_HTML: &str = r#"<!DOCTYPE html>
<html><head><title>Authentication successful</title>
<style>body{font-family:sans-serif;text-align:center;padding:60px;}</style></head>
<body><h2>Authentication successful</h2>
<p>You may close this tab and return to openlawsvpn.</p></body></html>"#;

#[derive(Deserialize)]
struct SamlForm {
    #[serde(rename = "SAMLResponse", default)]
    saml_response: String,
    // AWS sometimes uses these alternate field names
    #[serde(default)]
    wresult: String,
    #[serde(default)]
    token: String,
}

/// Starts the server, blocks until a SAML token arrives or timeout_secs elapses.
/// Returns the base64-encoded SAML assertion.
pub fn capture_saml_token(timeout_secs: u64) -> Result<String, String> {
    let (token_tx, token_rx) = oneshot::channel::<String>();
    let token_tx = Arc::new(Mutex::new(Some(token_tx)));

    let rt = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .map_err(|e| e.to_string())?;

    rt.block_on(async move {
        let token_tx_clone = token_tx.clone();

        let app = Router::new()
            .route(
                "/",
                post(move |Form(form): Form<SamlForm>| {
                    let token_tx = token_tx_clone.clone();
                    async move {
                        let raw = if !form.saml_response.is_empty() {
                            form.saml_response
                        } else if !form.wresult.is_empty() {
                            form.wresult
                        } else {
                            form.token
                        };
                        let normalized = normalize_base64(&raw);
                        if let Some(tx) = token_tx.lock().unwrap().take() {
                            let _ = tx.send(normalized);
                        }
                        Html(SUCCESS_HTML)
                    }
                }),
            )
            // GET handler for redirect-based IdPs
            .route(
                "/",
                get(move |axum::extract::Query(params): axum::extract::Query<std::collections::HashMap<String, String>>| {
                    let token_tx = token_tx.clone();
                    async move {
                        let raw = params.get("SAMLResponse")
                            .or_else(|| params.get("wresult"))
                            .or_else(|| params.get("token"))
                            .cloned()
                            .unwrap_or_default();
                        if !raw.is_empty() {
                            let normalized = normalize_base64(&raw);
                            if let Some(tx) = token_tx.lock().unwrap().take() {
                                let _ = tx.send(normalized);
                            }
                        }
                        Html(SUCCESS_HTML)
                    }
                }),
            );

        let listener = tokio::net::TcpListener::bind(BIND_ADDR)
            .await
            .map_err(|e| format!("Cannot bind {}: {}", BIND_ADDR, e))?;

        let server = axum::serve(listener, app);

        tokio::select! {
            _ = server => {},
            result = tokio::time::timeout(
                std::time::Duration::from_secs(timeout_secs),
                token_rx,
            ) => {
                return result
                    .map_err(|_| "SAML login timed out".to_string())?
                    .map_err(|_| "SAML channel closed".to_string());
            }
        }

        Err("Server stopped unexpectedly".to_string())
    })
}

/// Normalises a SAML response: URL-decode, replace spaces with +, pad to 4-byte boundary.
fn normalize_base64(raw: &str) -> String {
    // URL-decode (spaces may appear as + or %2B)
    let decoded = urlencoding::decode(raw).unwrap_or_else(|_| raw.into());
    // Spaces → +
    let replaced = decoded.replace(' ', "+");
    // Pad to multiple of 4
    let pad = (4 - replaced.len() % 4) % 4;
    format!("{}{}", replaced, "=".repeat(pad))
}
