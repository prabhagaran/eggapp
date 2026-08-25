import 'dart:async';

import 'package:dio/dio.dart';

import '../core/config.dart';
import 'token_store.dart';

/// An API call that failed in a way worth showing the user.
///
/// [message] is the server's own error message when it sent one — the API
/// returns `{ error: { code, message } }`, and those messages (e.g. a missing
/// discrepancy note) are written to be read by the person entering the data.
class ApiException implements Exception {
  final int? statusCode;
  final String message;
  final String? code;

  const ApiException(this.message, {this.statusCode, this.code});

  bool get isUnauthorized => statusCode == 401;

  /// No response at all — airplane mode, out of Tailscale range, server down.
  /// Phase 2 turns these into "queued locally" rather than a failure.
  bool get isNetworkFailure => statusCode == null;

  @override
  String toString() => message;
}

/// HTTP client for the eggAPP API.
///
/// Two Dio instances, mirroring the Kotlin app's split: [_auth] attaches the
/// bearer token and refreshes once on a 401, and [_authFree] is used for
/// login/refresh, where sending a token would be circular.
class ApiClient {
  final TokenStore tokenStore;
  late final Dio _auth;
  late final Dio _authFree;

  /// Called when refreshing fails — the session is genuinely over and the UI
  /// should return to the login screen.
  void Function()? onSessionExpired;

  /// Guards against a burst of parallel 401s each firing their own refresh.
  /// The first one refreshes; the rest await the same future.
  Future<bool>? _refreshInFlight;

  ApiClient({required this.tokenStore}) {
    final options = BaseOptions(
      baseUrl: AppConfig.apiBaseUrl,
      connectTimeout: const Duration(seconds: 15),
      receiveTimeout: const Duration(seconds: 20),
      contentType: 'application/json',
      // Handle every status ourselves so a 4xx surfaces as an ApiException
      // carrying the server's message rather than a bare DioException.
      validateStatus: (_) => true,
    );

    _authFree = Dio(options);
    _auth = Dio(options)
      ..interceptors.add(
        InterceptorsWrapper(
          onRequest: (request, handler) {
            final token = tokenStore.accessToken;
            if (token != null) {
              request.headers['Authorization'] = 'Bearer $token';
            }
            handler.next(request);
          },
          onResponse: (response, handler) async {
            // Not a 401, or this request is already the post-refresh retry —
            // either way, pass it through rather than looping.
            if (response.statusCode != 401 || response.requestOptions.extra['retried'] == true) {
              return handler.next(response);
            }

            final refreshed = await _refreshOnce();
            if (!refreshed) {
              onSessionExpired?.call();
              return handler.next(response);
            }

            try {
              final retried = await _auth.fetch<dynamic>(
                response.requestOptions
                  ..extra['retried'] = true
                  ..headers['Authorization'] = 'Bearer ${tokenStore.accessToken}',
              );
              return handler.resolve(retried);
            } on DioException catch (e) {
              return handler.next(e.response ?? response);
            }
          },
        ),
      );
  }

  /// Refreshes the access token, collapsing concurrent callers onto one call.
  Future<bool> _refreshOnce() {
    return _refreshInFlight ??= _doRefresh().whenComplete(() => _refreshInFlight = null);
  }

  Future<bool> _doRefresh() async {
    final refresh = tokenStore.refreshToken;
    if (refresh == null) return false;
    try {
      final res = await _authFree.post<dynamic>(
        'v1/auth/refresh',
        data: {'refreshToken': refresh},
      );
      if (res.statusCode != 200 || res.data is! Map) return false;
      final data = res.data as Map<String, dynamic>;
      await tokenStore.saveTokens(
        data['accessToken'] as String,
        data['refreshToken'] as String,
      );
      return true;
    } catch (_) {
      return false;
    }
  }

  Future<dynamic> _send(
    Future<Response<dynamic>> Function() call, {
    bool allowEmpty = false,
  }) async {
    Response<dynamic> res;
    try {
      res = await call();
    } on DioException catch (e) {
      throw ApiException(
        _networkMessage(e),
        statusCode: e.response?.statusCode,
      );
    }

    final status = res.statusCode ?? 0;
    if (status >= 200 && status < 300) {
      if (res.data == null && !allowEmpty) return null;
      return res.data;
    }
    throw _errorFrom(res);
  }

  String _networkMessage(DioException e) {
    switch (e.type) {
      case DioExceptionType.connectionTimeout:
      case DioExceptionType.sendTimeout:
      case DioExceptionType.receiveTimeout:
        return 'The server took too long to respond.';
      case DioExceptionType.connectionError:
        return "Can't reach the server. Check your connection, and that "
            'Tailscale is connected.';
      default:
        return e.message ?? 'Network request failed.';
    }
  }

  ApiException _errorFrom(Response<dynamic> res) {
    final data = res.data;
    if (data is Map && data['error'] is Map) {
      final err = data['error'] as Map;
      return ApiException(
        err['message'] as String? ?? 'Request failed',
        statusCode: res.statusCode,
        code: err['code'] as String?,
      );
    }
    if (res.statusCode == 401) {
      return ApiException('Your session has expired. Please sign in again.', statusCode: 401);
    }
    return ApiException('Request failed (${res.statusCode})', statusCode: res.statusCode);
  }

  Future<dynamic> get(String path, {Map<String, dynamic>? query}) =>
      _send(() => _auth.get<dynamic>(path, queryParameters: query));

  Future<dynamic> post(String path, {Object? body}) =>
      _send(() => _auth.post<dynamic>(path, data: body), allowEmpty: true);

  Future<dynamic> patch(String path, {Object? body}) =>
      _send(() => _auth.patch<dynamic>(path, data: body), allowEmpty: true);

  Future<dynamic> delete(String path) =>
      _send(() => _auth.delete<dynamic>(path), allowEmpty: true);

  /// Login goes through the auth-free client — there is no token yet.
  Future<dynamic> postAuthFree(String path, {Object? body}) =>
      _send(() => _authFree.post<dynamic>(path, data: body));
}
