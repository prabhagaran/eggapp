import 'dart:async';

import 'package:flutter/foundation.dart';

import '../data/api_client.dart';
import '../data/api_service.dart';
import '../data/models.dart';
import '../data/token_store.dart';

enum SessionStatus { loading, loggedOut, loggedIn }

/// Who is signed in and which farm they are working on.
///
/// The farm id is resolved once after login (first membership, matching the
/// web client's behaviour) and persisted, so the app opens straight onto the
/// user's farm rather than asking again.
class Session extends ChangeNotifier {
  final TokenStore tokenStore;
  final ApiService api;

  SessionStatus _status = SessionStatus.loading;
  Me? _me;
  List<Farm> _farms = const [];
  String? _farmId;
  String? _error;
  bool _busy = false;

  Session({required this.tokenStore, required this.api});

  SessionStatus get status => _status;
  Me? get me => _me;
  List<Farm> get farms => _farms;
  String? get farmId => _farmId;
  String? get error => _error;
  bool get busy => _busy;

  Farm? get farm {
    for (final f in _farms) {
      if (f.id == _farmId) return f;
    }
    return _farms.isEmpty ? null : _farms.first;
  }

  /// Called once at startup. A stored token is trusted optimistically: the
  /// first authenticated request will refresh it or fail, and [onSessionExpired]
  /// drops us back to the login screen. Blocking startup on a verification
  /// round-trip would put a spinner in front of a field worker who may have no
  /// signal at all.
  Future<void> restore() async {
    await tokenStore.load();
    _farmId = tokenStore.farmId;
    if (!tokenStore.isLoggedIn) {
      _set(SessionStatus.loggedOut);
      return;
    }
    _set(SessionStatus.loggedIn);
    // Refresh identity in the background; failure here is not fatal.
    unawaited(_loadIdentity());
  }

  Future<bool> login(String email, String password) async {
    _busy = true;
    _error = null;
    notifyListeners();
    try {
      final tokens = await api.login(email.trim(), password);
      await tokenStore.saveTokens(tokens.accessToken, tokens.refreshToken);
      await _loadIdentity();
      _set(SessionStatus.loggedIn);
      return true;
    } on ApiException catch (e) {
      _error = e.message;
      return false;
    } catch (e) {
      _error = 'Sign-in failed: $e';
      return false;
    } finally {
      _busy = false;
      notifyListeners();
    }
  }

  Future<void> _loadIdentity() async {
    try {
      _me = await api.me();
      _farms = await api.farms();
      if (_farmId == null && _farms.isNotEmpty) {
        _farmId = _farms.first.id;
        await tokenStore.saveFarmId(_farmId!);
      }
      notifyListeners();
    } on ApiException {
      // Leave the session as-is; a genuine auth failure arrives via
      // onSessionExpired, and a network blip should not sign anyone out.
    }
  }

  Future<void> selectFarm(String id) async {
    if (id == _farmId) return;
    _farmId = id;
    await tokenStore.saveFarmId(id);
    notifyListeners();
  }

  Future<void> logout() async {
    await tokenStore.clear();
    _me = null;
    _farms = const [];
    _farmId = null;
    _set(SessionStatus.loggedOut);
  }

  /// Wired to [ApiClient.onSessionExpired]: the refresh token is gone or
  /// rejected, so there is nothing left to retry with.
  void expire() {
    if (_status == SessionStatus.loggedOut) return;
    tokenStore.clear();
    _me = null;
    _farms = const [];
    _set(SessionStatus.loggedOut);
  }

  void _set(SessionStatus s) {
    _status = s;
    notifyListeners();
  }
}

