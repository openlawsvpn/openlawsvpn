package com.openlawsvpn.poc

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.browser.customtabs.CustomTabsIntent
import androidx.lifecycle.lifecycleScope
import com.openlawsvpn.poc.databinding.ActivityMainBinding
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * PoC entry point.
 *
 * Goal: verify that Chrome Custom Tabs allows the AWS SSO SPA (HTTPS) to form-POST
 * the SAMLResponse to http://127.0.0.1:35001/ (HTTP, loopback).
 *
 * Two test paths:
 *
 * 1. Local test (no AWS needed):
 *    - Start server → "Test Local Callback"
 *    - Opens http://127.0.0.1:35001/test in a Custom Tab
 *    - Page auto-submits a fake SAMLResponse back to :35001
 *    - Confirms HTTP→HTTP localhost round-trip works
 *
 * 2. Real SAML test (requires AWS VPN config):
 *    - Run `openlawsvpn-cli` on Linux, copy the printed SAML URL from Phase 1
 *    - Paste URL into the field → "Open in Chrome Custom Tabs"
 *    - Complete IdP login → SAMLResponse captured
 *    - Confirms HTTPS→HTTP mixed-content form POST is not blocked by Chrome
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val server = SamlCallbackServer()
    private var serverRunning = false
    private val timeFmt = SimpleDateFormat("HH:mm:ss", Locale.getDefault())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        binding.btnToggleServer.setOnClickListener { toggleServer() }
        binding.btnTestLocal.setOnClickListener { openTestForm() }
        binding.btnOpenSaml.setOnClickListener { openSamlUrl() }
        setTestButtonsEnabled(false)
        binding.btnClear.setOnClickListener {
            binding.tvLog.text = ""
            binding.tvToken.text = ""
        }

        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    /** Called when Chrome Custom Tab fires openlawsvpn://saml-callback after ACS redirect. */
    private fun handleIntent(intent: Intent) {
        if (intent.data?.scheme == "openlawsvpn" && intent.data?.host == "saml-callback") {
            log("App resumed via openlawsvpn://saml-callback — browser redirect worked.")
        }
    }

    private fun toggleServer() {
        if (serverRunning) {
            server.stop()
            serverRunning = false
            setTestButtonsEnabled(false)
            updateServerUi(running = false)
            log("ACS server stopped.")
        } else {
            // Disable test buttons until we get Event.Started — the socket isn't
            // bound yet until the coroutine runs, so opening Chrome before that
            // gives "connection refused".
            setTestButtonsEnabled(false)
            binding.btnToggleServer.isEnabled = false
            log("Starting ACS server on 127.0.0.1:${SamlCallbackServer.PORT}…")
            server.start(lifecycleScope) { event ->
                runOnUiThread { handleServerEvent(event) }
            }
        }
    }

    private fun handleServerEvent(event: SamlCallbackServer.Event) {
        when (event) {
            is SamlCallbackServer.Event.Started -> {
                serverRunning = true
                binding.btnToggleServer.isEnabled = true
                updateServerUi(running = true)
                setTestButtonsEnabled(true)
                log("Server ready on 127.0.0.1:${SamlCallbackServer.PORT}")
            }

            is SamlCallbackServer.Event.RequestReceived ->
                log("[REQUEST] ${event.method} ${event.path}\n  ${event.bodySnippet}")

            is SamlCallbackServer.Event.TokenReceived -> {
                val len = event.samlResponse.length
                log("[SUCCESS] SAMLResponse received ($len chars)")
                binding.tvToken.text =
                    "SAMLResponse ($len chars):\n\n${event.samlResponse.take(400)}…"
            }

            is SamlCallbackServer.Event.Error -> {
                log("[ERROR] ${event.message}")
                serverRunning = false
                binding.btnToggleServer.isEnabled = true
                updateServerUi(running = false)
                setTestButtonsEnabled(false)
            }

            SamlCallbackServer.Event.Stopped -> log("Server stopped.")
        }
    }

    /** Opens http://127.0.0.1:35001/test — the server's self-hosted test form. */
    private fun openTestForm() {
        if (!serverRunning) { log("Start the server first."); return }
        log("Opening test form: http://127.0.0.1:${SamlCallbackServer.PORT}/test")
        launchCustomTab("http://127.0.0.1:${SamlCallbackServer.PORT}/test")
    }

    /** Opens a real SAML URL from Phase 1 (obtained from the Linux CLI). */
    private fun openSamlUrl() {
        val url = binding.etSamlUrl.text.toString().trim()
        if (url.isEmpty()) { log("Paste a SAML URL first."); return }
        if (!serverRunning) { log("Start the server first."); return }
        log("Opening SAML URL in Custom Tabs:\n  ${url.take(80)}…")
        launchCustomTab(url)
    }

    private fun launchCustomTab(url: String) {
        CustomTabsIntent.Builder()
            .setShowTitle(true)
            .build()
            .launchUrl(this, Uri.parse(url))
    }

    private fun setTestButtonsEnabled(enabled: Boolean) {
        binding.btnTestLocal.isEnabled = enabled
        binding.btnOpenSaml.isEnabled = enabled
    }

    private fun updateServerUi(running: Boolean) {
        binding.btnToggleServer.text =
            if (running) "Stop ACS Server (:${SamlCallbackServer.PORT})"
            else "Start ACS Server (:${SamlCallbackServer.PORT})"
    }

    private fun log(msg: String) {
        val ts = timeFmt.format(Date())
        val prev = binding.tvLog.text.toString()
        binding.tvLog.text = "[$ts] $msg\n\n$prev"
    }

    override fun onDestroy() {
        super.onDestroy()
        server.stop()
    }
}
