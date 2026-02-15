import 'dart:async';
import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';
import 'package:url_launcher/url_launcher.dart';
import '../bindings/libopenlawsvpn.dart';
import 'saml_capture_service.dart';

enum VpnStatus { disconnected, connecting, authenticating, connected }

class VpnService {
  final LibOpenVPN _lib = LibOpenVPN();
  final SamlCaptureService _samlCapture = SamlCaptureService();
  ffi.Pointer<ffi.Void>? _clientHandle;

  final _statusController = StreamController<VpnStatus>.broadcast();
  final _logController = StreamController<String>.broadcast();

  Stream<VpnStatus> get status => _statusController.stream;
  Stream<String> get logs => _logController.stream;

  VpnStatus _currentStatus = VpnStatus.disconnected;
  VpnStatus get currentStatus => _currentStatus;

  VpnService() {
    _statusController.add(VpnStatus.disconnected);
  }

  void _updateStatus(VpnStatus status) {
    _currentStatus = status;
    _statusController.add(status);
  }

  void _onLog(String message) {
    _logController.add(message);
    print('VPN LOG: $message');
  }

  Future<void> connect(String configPath) async {
    if (_clientHandle != null) return;

    try {
      _updateStatus(VpnStatus.connecting);
      _logController.add("Initializing client with $configPath...");

      _clientHandle = _lib.newClient(configPath);
      _lib.setConnectMode(_clientHandle!, 1); // 1 = DBUS

      _lib.setLogCallback(_clientHandle!, _onLog);

      _logController.add("Starting Phase 1...");
      final phase1 = _lib.connectPhase1(_clientHandle!);

      if (phase1.samlUrl.isNotEmpty) {
        _updateStatus(VpnStatus.authenticating);
        _logController.add("SAML URL received. Starting capture server...");

        await _samlCapture.start();
        _logController.add("Opening browser for SAML authentication...");

        if (await canLaunchUrl(Uri.parse(phase1.samlUrl))) {
          await launchUrl(Uri.parse(phase1.samlUrl));
        } else {
          _logController.add("Error: Could not launch SAML URL");
          disconnect();
          return;
        }

        final token = await _samlCapture.onTokenCaptured.first;
        _logController.add("Token captured. Starting Phase 2...");
        await _samlCapture.stop();

        _lib.connectPhase2(_clientHandle!, phase1.stateId, token, phase1.remoteIp);
        _updateStatus(VpnStatus.connected);
        _logController.add("Connected successfully.");
      } else {
        _logController.add("Error: Phase 1 did not return a SAML URL");
        disconnect();
      }
    } catch (e) {
      _logController.add("Connection error: $e");
      disconnect();
    }
  }

  void disconnect() {
    if (_clientHandle != null) {
      _logController.add("Disconnecting...");
      _lib.disconnect(_clientHandle!);
      _lib.freeClient(_clientHandle!);
      _clientHandle = null;
    }
    _samlCapture.stop();
    _updateStatus(VpnStatus.disconnected);
    _logController.add("Disconnected.");
  }
}
