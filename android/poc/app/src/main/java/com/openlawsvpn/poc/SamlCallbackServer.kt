package com.openlawsvpn.poc

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.Inet4Address
import java.net.InetSocketAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.URLDecoder

/**
 * Minimal HTTP server on 127.0.0.1:35001 that acts as the SAML ACS endpoint.
 *
 * AWS Client VPN hardcodes AssertionConsumerServiceURL=http://127.0.0.1:35001 in the
 * SAMLRequest. After the user completes IdP login, the AWS SSO SPA form-POSTs the
 * SAMLResponse to this address.
 *
 * Endpoints:
 *   POST /          — ACS: captures SAMLResponse, redirects to openlawsvpn://saml-callback
 *   GET  /test      — serves an auto-submit test form (HTTP→HTTP, no AWS needed)
 *   OPTIONS /       — CORS preflight, returns Private Network Access allow headers
 *   GET  /          — status page
 */
class SamlCallbackServer {

    private var serverSocket: ServerSocket? = null
    private var serverJob: Job? = null

    sealed class Event {
        object Started : Event()
        object Stopped : Event()
        data class RequestReceived(val method: String, val path: String, val bodySnippet: String) : Event()
        data class TokenReceived(val samlResponse: String) : Event()
        data class Error(val message: String) : Event()
    }

    fun start(scope: CoroutineScope, onEvent: (Event) -> Unit) {
        serverJob = scope.launch(Dispatchers.IO) {
            try {
                // Bind explicitly to IPv4 127.0.0.1.
                // InetAddress.getLoopbackAddress() can return ::1 (IPv6) on some Android
                // devices, which would cause Chrome to get "connection refused" when it
                // connects to 127.0.0.1 (IPv4).
                val loopback = Inet4Address.getByName("127.0.0.1")
                serverSocket = ServerSocket().apply {
                    reuseAddress = true
                    bind(InetSocketAddress(loopback, PORT))
                }
                Log.i(TAG, "SAML ACS server started on 127.0.0.1:$PORT")
                onEvent(Event.Started)
                while (isActive) {
                    val client = serverSocket!!.accept()
                    launch { handleClient(client, onEvent) }
                }
            } catch (e: Exception) {
                if (isActive) {
                    Log.e(TAG, "Server error: ${e.message}")
                    onEvent(Event.Error(e.message ?: "Unknown error"))
                }
            }
        }
    }

    private fun handleClient(socket: Socket, onEvent: (Event) -> Unit) {
        socket.use {
            try {
                val reader = BufferedReader(InputStreamReader(it.inputStream))
                val output = it.outputStream

                val requestLine = reader.readLine() ?: return
                val parts = requestLine.split(" ")
                val method = parts.getOrElse(0) { "?" }
                val path = parts.getOrElse(1) { "/" }

                val headers = mutableMapOf<String, String>()
                var contentLength = 0
                while (true) {
                    val line = reader.readLine() ?: break
                    if (line.isEmpty()) break
                    val colonIdx = line.indexOf(':')
                    if (colonIdx > 0) {
                        val key = line.substring(0, colonIdx).trim().lowercase()
                        val value = line.substring(colonIdx + 1).trim()
                        headers[key] = value
                        if (key == "content-length") contentLength = value.toIntOrNull() ?: 0
                    }
                }

                val body = if (contentLength > 0) {
                    val chars = CharArray(minOf(contentLength, 65536))
                    val read = reader.read(chars, 0, chars.size)
                    if (read > 0) String(chars, 0, read) else ""
                } else ""

                val bodySnippet = if (body.length > 120) "${body.take(120)}…" else body
                Log.d(TAG, "$method $path | body[$contentLength]: $bodySnippet")
                onEvent(Event.RequestReceived(method, path, bodySnippet))

                when {
                    method == "OPTIONS" -> handlePreflight(output)
                    method == "POST" && path == "/" -> handleAcs(output, body, onEvent)
                    method == "GET" && path == "/test" -> handleTestForm(output)
                    else -> handleStatus(output)
                }

            } catch (e: Exception) {
                Log.e(TAG, "Client handling error: ${e.message}")
            }
        }
    }

    /** ACS endpoint — extract SAMLResponse and redirect back to the app. */
    private fun handleAcs(output: java.io.OutputStream, body: String, onEvent: (Event) -> Unit) {
        val samlResponse = body.split("&")
            .find { it.startsWith("SAMLResponse=") }
            ?.removePrefix("SAMLResponse=")
            ?.let { URLDecoder.decode(it, "UTF-8") }

        if (samlResponse != null) {
            Log.i(TAG, "SAMLResponse captured (${samlResponse.length} chars)")
            // Redirect to custom scheme — Android fires an intent, closing the Custom Tab
            // and bringing the app to the foreground.
            write(output, buildResponse(
                302,
                mapOf(
                    "Location" to "openlawsvpn://saml-callback",
                    "Access-Control-Allow-Origin" to "*",
                    "Access-Control-Allow-Private-Network" to "true",
                ),
                ""
            ))
            onEvent(Event.TokenReceived(samlResponse))
        } else {
            write(output, buildResponse(400, emptyMap(), "Missing SAMLResponse"))
        }
    }

    /**
     * Serves a self-submitting HTML form that POSTs a fake SAMLResponse to /.
     * Used to verify the HTTP→HTTP local callback works without needing real AWS credentials.
     * Open http://127.0.0.1:35001/test in Chrome Custom Tabs to run this test.
     */
    private fun handleTestForm(output: java.io.OutputStream) {
        val html = """
            <!DOCTYPE html>
            <html>
            <head>
              <meta charset="utf-8">
              <title>openlawsvpn — Local Callback Test</title>
            </head>
            <body onload="document.getElementById('f').submit()">
              <form id="f" method="post" action="http://127.0.0.1:$PORT/">
                <input type="hidden" name="SAMLResponse"
                  value="TEST_SAML_RESPONSE_HELLO_OPENLAWSVPN" />
              </form>
              <p>Submitting test SAMLResponse to ACS endpoint…</p>
            </body>
            </html>
        """.trimIndent()
        write(output, buildResponse(200, mapOf("Content-Type" to "text/html; charset=utf-8"), html))
    }

    /** CORS preflight — return Private Network Access allow headers. */
    private fun handlePreflight(output: java.io.OutputStream) {
        write(output, buildResponse(
            204,
            mapOf(
                "Access-Control-Allow-Origin" to "*",
                "Access-Control-Allow-Methods" to "POST, GET, OPTIONS",
                "Access-Control-Allow-Headers" to "Content-Type",
                "Access-Control-Allow-Private-Network" to "true",
            ),
            ""
        ))
    }

    /** Simple status page for GET /. */
    private fun handleStatus(output: java.io.OutputStream) {
        val html = """
            <!DOCTYPE html>
            <html><head><title>openlawsvpn ACS</title></head>
            <body><p>openlawsvpn SAML ACS endpoint — waiting for callback on port $PORT.</p></body>
            </html>
        """.trimIndent()
        write(output, buildResponse(
            200,
            mapOf(
                "Content-Type" to "text/html; charset=utf-8",
                "Access-Control-Allow-Private-Network" to "true",
            ),
            html
        ))
    }

    private fun buildResponse(status: Int, headers: Map<String, String>, body: String): ByteArray {
        val statusText = mapOf(200 to "OK", 204 to "No Content", 302 to "Found", 400 to "Bad Request")
            .getOrDefault(status, "Unknown")
        val bodyBytes = body.toByteArray(Charsets.UTF_8)
        val sb = StringBuilder("HTTP/1.1 $status $statusText\r\n")
        headers.forEach { (k, v) -> sb.append("$k: $v\r\n") }
        if (bodyBytes.isNotEmpty()) sb.append("Content-Length: ${bodyBytes.size}\r\n")
        sb.append("Connection: close\r\n\r\n")
        return sb.toString().toByteArray() + if (bodyBytes.isNotEmpty()) bodyBytes else byteArrayOf()
    }

    private fun write(output: java.io.OutputStream, bytes: ByteArray) {
        output.write(bytes)
        output.flush()
    }

    fun stop() {
        serverJob?.cancel()
        serverSocket?.close()
        serverSocket = null
        Log.i(TAG, "SAML ACS server stopped")
    }

    companion object {
        private const val TAG = "SamlServer"
        const val PORT = 35001
    }
}
