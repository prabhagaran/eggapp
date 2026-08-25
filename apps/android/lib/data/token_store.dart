import 'package:flutter_secure_storage/flutter_secure_storage.dart';

/// Token + active-farm storage, backed by the Android Keystore and the iOS
/// Keychain. This replaces the Kotlin app's `EncryptedSharedPreferences`
/// (android-architect.md security review item: "Android Keystore usage for
/// tokens/credentials") and gives iOS the equivalent guarantee.
///
/// Values are cached in memory after the first read so the request
/// interceptor stays synchronous-ish on the hot path — every authenticated
/// request needs the access token, and hitting the Keystore each time is both
/// slow and needless.
class TokenStore {
  static const _keyAccess = 'access_token';
  static const _keyRefresh = 'refresh_token';
  static const _keyFarm = 'farm_id';
  static const _keyDevice = 'device_id';

  final FlutterSecureStorage _storage;

  String? _access;
  String? _refresh;
  String? _farmId;
  bool _loaded = false;

  TokenStore({FlutterSecureStorage? storage})
      : _storage = storage ??
            const FlutterSecureStorage(
              aOptions: AndroidOptions(encryptedSharedPreferences: true),
              iOptions: IOSOptions(accessibility: KeychainAccessibility.first_unlock),
            );

  /// Must be awaited once at startup, before the first authenticated request.
  Future<void> load() async {
    if (_loaded) return;
    _access = await _storage.read(key: _keyAccess);
    _refresh = await _storage.read(key: _keyRefresh);
    _farmId = await _storage.read(key: _keyFarm);
    _loaded = true;
  }

  String? get accessToken => _access;
  String? get refreshToken => _refresh;
  String? get farmId => _farmId;
  bool get isLoggedIn => _access != null;

  Future<void> saveTokens(String access, String refresh) async {
    _access = access;
    _refresh = refresh;
    await _storage.write(key: _keyAccess, value: access);
    await _storage.write(key: _keyRefresh, value: refresh);
  }

  Future<void> saveFarmId(String id) async {
    _farmId = id;
    await _storage.write(key: _keyFarm, value: id);
  }

  /// Stable per-install client identifier (FRS §10, §19.7).
  ///
  /// Read directly rather than from the startup cache: it is fetched once at
  /// launch, not on every request, so the cache would buy nothing.
  Future<String?> deviceId() => _storage.read(key: _keyDevice);

  Future<void> saveDeviceId(String id) =>
      _storage.write(key: _keyDevice, value: id);

  /// Clears the session. The device id deliberately survives sign-out — it
  /// identifies the phone, not the person, and a queued record captured
  /// before sign-out must keep its origin.
  Future<void> clear() async {
    _access = null;
    _refresh = null;
    _farmId = null;
    final device = await _storage.read(key: _keyDevice);
    await _storage.deleteAll();
    if (device != null) await _storage.write(key: _keyDevice, value: device);
  }
}
