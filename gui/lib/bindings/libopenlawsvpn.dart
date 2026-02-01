import 'dart:convert';
import 'dart:ffi' as ffi;
import 'dart:io';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

// FFI structure for Phase1Result
final class Phase1ResultC extends ffi.Struct {
  external ffi.Pointer<Utf8> samlUrl;
  external ffi.Pointer<Utf8> stateId;
  external ffi.Pointer<Utf8> remoteIp;
}

// Log callback type
typedef LogCallbackNative = ffi.Void Function(
    ffi.Pointer<Utf8> message, ffi.Pointer<ffi.Void> userData);
typedef LogCallbackDart = void Function(
    ffi.Pointer<Utf8> message, ffi.Pointer<ffi.Void> userData);

class LibOpenVPN {
  late ffi.DynamicLibrary _lib;

  // Function signatures
  late ffi.Pointer<ffi.Void> Function(ffi.Pointer<Utf8>) _newClient;
  late void Function(ffi.Pointer<ffi.Void>) _freeClient;
  late void Function(ffi.Pointer<ffi.Void>, int) _setConnectMode;
  late void Function(ffi.Pointer<ffi.Void>, int) _setLogLevel;
  late void Function(ffi.Pointer<ffi.Void>,
      ffi.Pointer<ffi.NativeFunction<LogCallbackNative>>, ffi.Pointer<ffi.Void>) _setLogCallback;
  late Phase1ResultC Function(ffi.Pointer<ffi.Void>) _connectPhase1;
  late void Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>,
      ffi.Pointer<Utf8>) _connectPhase2;
  late void Function(ffi.Pointer<ffi.Void>) _disconnect;
  late void Function(ffi.Pointer<Utf8>) _freeString;

  ffi.NativeCallable<LogCallbackNative>? _logCallable;

  LibOpenVPN() {
    _lib = _loadLibrary();
    _initFunctions();
  }

  ffi.DynamicLibrary _loadLibrary() {
    if (Platform.isLinux) {
      // Try bundling location first, then build directory, then system
      final bundlePath = '${File(Platform.resolvedExecutable).parent.path}/lib/libopenlawsvpn.so';
      if (File(bundlePath).existsSync()) {
        return ffi.DynamicLibrary.open(bundlePath);
      }
      final libPaths = [
        '${Directory.current.path}/../build/lib/libopenlawsvpn.so',
        '${Directory.current.path}/../build/lib64/libopenlawsvpn.so',
      ];
      for (final localPath in libPaths) {
        if (File(localPath).existsSync()) {
          return ffi.DynamicLibrary.open(localPath);
        }
      }
      try {
        return ffi.DynamicLibrary.open('libopenlawsvpn.so');
      } catch (e) {
        rethrow;
      }
    }
    throw UnsupportedError('Only Linux is supported');
  }

  void _initFunctions() {
    _newClient = _lib
        .lookup<ffi.NativeFunction<ffi.Pointer<ffi.Void> Function(ffi.Pointer<Utf8>)>>(
            'openvpn_client_new')
        .asFunction();
    _freeClient = _lib
        .lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>)>>(
            'openvpn_client_free')
        .asFunction();
    _setConnectMode = _lib
        .lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Int32)>>(
            'openvpn_client_set_connect_mode')
        .asFunction();
    _setLogLevel = _lib
        .lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Int32)>>(
            'openvpn_client_set_log_level')
        .asFunction();
    _setLogCallback = _lib
        .lookup<
            ffi.NativeFunction<
                ffi.Void Function(
                    ffi.Pointer<ffi.Void>,
                    ffi.Pointer<ffi.NativeFunction<LogCallbackNative>>,
                    ffi.Pointer<ffi.Void>)>>('openvpn_client_set_log_callback')
        .asFunction();
    _connectPhase1 = _lib
        .lookup<ffi.NativeFunction<Phase1ResultC Function(ffi.Pointer<ffi.Void>)>>(
            'openvpn_client_connect_phase1')
        .asFunction();
    _connectPhase2 = _lib
        .lookup<
            ffi.NativeFunction<
                ffi.Void Function(ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>,
                    ffi.Pointer<Utf8>, ffi.Pointer<Utf8>)>>('openvpn_client_connect_phase2')
        .asFunction();
    _disconnect = _lib
        .lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>)>>(
            'openvpn_client_disconnect')
        .asFunction();
    _freeString = _lib
        .lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<Utf8>)>>(
            'openvpn_free_string')
        .asFunction();
  }

  ffi.Pointer<ffi.Void> newClient(String configPath) {
    final pathPtr = configPath.toNativeUtf8();
    try {
      return _newClient(pathPtr);
    } finally {
      malloc.free(pathPtr);
    }
  }

  void freeClient(ffi.Pointer<ffi.Void> client) => _freeClient(client);

  void setConnectMode(ffi.Pointer<ffi.Void> client, int mode) =>
      _setConnectMode(client, mode);

  void setLogLevel(ffi.Pointer<ffi.Void> client, int level) =>
      _setLogLevel(client, level);

  void setLogCallback(ffi.Pointer<ffi.Void> client,
          void Function(String message) callback) {
    _logCallable?.close();
    _logCallable = ffi.NativeCallable<LogCallbackNative>.listener(
        (ffi.Pointer<Utf8> message, ffi.Pointer<ffi.Void> userData) {
      if (message != ffi.nullptr) {
        try {
          final ptr = message.cast<ffi.Uint8>();
          int length = 0;
          while (ptr[length] != 0 && length < 16384) { // Increased safety cap
            length++;
          }
          final list = ptr.asTypedList(length);
          
          // Heuristic to detect binary "garbage"
          int nonPrintable = 0;
          for (int i = 0; i < list.length; i++) {
            int c = list[i];
            // Standard printable ASCII (32-126) + common whitespace
            if (!((c >= 32 && c <= 126) || c == 9 || c == 10 || c == 13)) {
              nonPrintable++;
            }
          }
          
          // Tighten heuristic even more to avoid "VPN LOG: 0j0..."
          // 1. If message contains a null byte, drop it.
          // 2. If it contains "http" but looks like binary (DER/ASN.1), drop it.
          // 3. If more than 5% (or 3 characters) are non-printable, drop it.
          bool hasNull = list.contains(0);
          bool isLikelyBinarySsl = list.length > 10 && list[0] == 0x30 && list[1] == 0x82; // ASN.1 Sequence
          if (hasNull || isLikelyBinarySsl || nonPrintable > 3 || (list.length > 0 && nonPrintable > (list.length * 0.05))) {
            return;
          }

          final decoded = utf8.decode(list, allowMalformed: true);
          // Further sanitization: replace remaining control characters (except newlines/tabs) with dots
          final sanitized = decoded.replaceAll(RegExp(r'[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]'), '.');
          if (sanitized.trim().isNotEmpty) {
            callback(sanitized);
          }
        } catch (e) {
          print('Error in log decoding/filtering: $e');
        }
      }
    });
    _setLogCallback(client, _logCallable!.nativeFunction, ffi.nullptr);
  }

  Phase1Result connectPhase1(ffi.Pointer<ffi.Void> client) {
    final res = _connectPhase1(client);
    final result = Phase1Result(
      samlUrl: res.samlUrl != ffi.nullptr ? res.samlUrl.toDartString() : '',
      stateId: res.stateId != ffi.nullptr ? res.stateId.toDartString() : '',
      remoteIp: res.remoteIp != ffi.nullptr ? res.remoteIp.toDartString() : '',
    );
    if (res.samlUrl != ffi.nullptr) _freeString(res.samlUrl);
    if (res.stateId != ffi.nullptr) _freeString(res.stateId);
    if (res.remoteIp != ffi.nullptr) _freeString(res.remoteIp);
    return result;
  }

  void connectPhase2(ffi.Pointer<ffi.Void> client, String stateId, String token, String remoteIp) {
    final stateIdPtr = stateId.toNativeUtf8();
    final tokenPtr = token.toNativeUtf8();
    final remoteIpPtr = remoteIp.toNativeUtf8();
    try {
      _connectPhase2(client, stateIdPtr, tokenPtr, remoteIpPtr);
    } finally {
      malloc.free(stateIdPtr);
      malloc.free(tokenPtr);
      malloc.free(remoteIpPtr);
    }
  }

  void disconnect(ffi.Pointer<ffi.Void> client) => _disconnect(client);
}

class Phase1Result {
  final String samlUrl;
  final String stateId;
  final String remoteIp;
  Phase1Result({required this.samlUrl, required this.stateId, required this.remoteIp});
}
