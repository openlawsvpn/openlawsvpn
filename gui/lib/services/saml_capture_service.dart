import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:shelf/shelf.dart';
import 'package:shelf/shelf_io.dart' as io;
import 'package:shelf_router/shelf_router.dart';

class SamlCaptureService {
  HttpServer? _server;
  final _tokenController = StreamController<String>.broadcast();

  Stream<String> get onTokenCaptured => _tokenController.stream;

  Future<int> start() async {
    final router = Router();

    router.all('/<ignored|.*>', (Request request) async {
      print('SAML Capture Request: ${request.method} ${request.url}');
      String? token = request.url.queryParameters['token'];
      String? body;

      try {
        if (token == null && request.method == 'POST') {
          // Explicitly drain the request body to avoid "ERR_EMPTY_RESPONSE"
          // some IDPs might send large bodies that need to be fully read.
          final bytes = await request.read().toList();
          if (bytes.isNotEmpty) {
            final flatBytes = bytes.expand((i) => i).toList();
            final decodedBody = utf8.decode(flatBytes, allowMalformed: true);
            body = decodedBody;
            final params = Uri.splitQueryString(decodedBody);
            token = params['token'] ?? params['SAMLResponse'] ?? params['wresult'];
            print('SAML Capture: Parsed POST body. Token found: ${token != null}');
          }
        }
      } catch (e, stack) {
        print('Error parsing POST body: $e\n$stack');
        body = 'Error: $e';
      }

      final responseHeaders = {
        'content-type': 'text/html; charset=utf-8',
        'connection': 'close',
        'cache-control': 'no-cache, no-store, must-revalidate',
      };

      if (token != null) {
        _tokenController.add(token);
        return Response.ok('''
          <html>
            <head><title>Authentication Successful</title></head>
            <body style="font-family: sans-serif; text-align: center; padding-top: 50px;">
              <h1 style="color: #2e7d32;">✔ Authentication Successful</h1>
              <p>The VPN client has received your credentials.</p>
              <p>You can close this window now.</p>
              <script>setTimeout(() => window.close(), 3000);</script>
            </body>
          </html>
        ''', headers: responseHeaders);
      }

      // Detailed feedback for debugging
      final debugInfo = "Method: ${request.method}\nPath: ${request.url.path}\nParams: ${request.url.queryParameters}\nBody Length: ${body?.length ?? 0}";
      return Response.ok('''
          <html>
            <head><title>Authentication Pending</title></head>
            <body style="font-family: sans-serif; text-align: center; padding-top: 50px;">
              <h1 style="color: #d32f2f;">⌛ Authentication Pending</h1>
              <p>SAML Capture Service is running, but no token was found in this request.</p>
              <p>If you have already authenticated, you can close this window.</p>
              <hr/>
              <div style="text-align: left; display: inline-block; font-family: monospace; background: #eee; padding: 10px; border-radius: 4px; max-width: 90%; overflow: auto;">
                <pre>${debugInfo}</pre>
                ${body != null ? '<details><summary>Raw Body</summary><pre style="white-space: pre-wrap; word-break: break-all;">' + body! + '</pre></details>' : ''}
              </div>
              <p style="color: #666; font-size: 0.8em;">This window will attempt to close automatically in 60 seconds.</p>
              <script>setTimeout(() => window.close(), 60000);</script>
            </body>
          </html>
        ''', headers: responseHeaders);
    });

    final handler = const Pipeline()
        .addMiddleware(logRequests())
        .addHandler(router.call);

    // Use standard port 35001 (AWS fixed redirect)
    try {
      _server = await io.serve(handler, '127.0.0.1', 35001);
      print('SAML capture server listening on port 35001');
      return 35001;
    } catch (e) {
      print('Error starting SAML capture server: $e');
      rethrow;
    }
  }

  Future<void> stop() async {
    await _server?.close(force: true);
    _server = null;
  }
}
