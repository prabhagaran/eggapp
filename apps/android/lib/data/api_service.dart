import 'api_client.dart';
import 'models.dart';

/// Typed wrapper over [ApiClient]. One method per endpoint, named for what it
/// fetches — the paths live here and nowhere else, so a route change in the
/// API is a one-file edit.
///
/// Phase 1 covers the read surface plus auth. The write endpoints
/// (candling, hatch, collections, mortality, vaccination, feed/water,
/// setpoints) arrive in Phase 2 together with the offline queue that backs
/// them, so that no write ever reaches the network without a local record
/// first — the property the Kotlin app's field testing showed matters most.
class ApiService {
  final ApiClient _client;
  const ApiService(this._client);

  List<T> _list<T>(dynamic data, T Function(Map<String, dynamic>) fromJson) {
    if (data is! List) return const [];
    return data.map((e) => fromJson(e as Map<String, dynamic>)).toList();
  }

  // ── Auth ────────────────────────────────────────────────────────────────

  Future<TokenPair> login(String email, String password) async {
    final data = await _client.postAuthFree(
      'v1/auth/login',
      body: {'email': email, 'password': password},
    );
    return TokenPair.fromJson(data as Map<String, dynamic>);
  }

  Future<Me> me() async => Me.fromJson(await _client.get('v1/me') as Map<String, dynamic>);

  Future<List<Farm>> farms() async => _list(await _client.get('v1/farms'), Farm.fromJson);

  // ── Incubators ──────────────────────────────────────────────────────────

  Future<List<Incubator>> incubators(String farmId) async =>
      _list(await _client.get('v1/farms/$farmId/incubators'), Incubator.fromJson);

  Future<Incubator> incubator(String farmId, String id) async =>
      Incubator.fromJson(await _client.get('v1/farms/$farmId/incubators/$id') as Map<String, dynamic>);

  // ── Batches ─────────────────────────────────────────────────────────────

  Future<List<Batch>> batches(String farmId, {String? status}) async => _list(
        await _client.get(
          'v1/farms/$farmId/batches',
          query: status == null ? null : {'status': status},
        ),
        Batch.fromJson,
      );

  Future<BatchDetail> batchDetail(String farmId, String id) async =>
      BatchDetail.fromJson(await _client.get('v1/farms/$farmId/batches/$id') as Map<String, dynamic>);

  // ── Collections ─────────────────────────────────────────────────────────

  Future<List<EggCollection>> collections(String farmId) async =>
      _list(await _client.get('v1/farms/$farmId/collections'), EggCollection.fromJson);

  // ── Flocks ──────────────────────────────────────────────────────────────

  Future<List<Flock>> flocks(String farmId) async =>
      _list(await _client.get('v1/farms/$farmId/flocks'), Flock.fromJson);

  Future<FlockDetail> flock(String farmId, String id) async =>
      FlockDetail.fromJson(await _client.get('v1/farms/$farmId/flocks/$id') as Map<String, dynamic>);

  Future<List<ComplianceItem>> vaccinationCompliance(String farmId, String flockId) async => _list(
        await _client.get('v1/farms/$farmId/flocks/$flockId/vaccination/compliance'),
        ComplianceItem.fromJson,
      );

  // ── Alerts ──────────────────────────────────────────────────────────────

  Future<List<Alert>> alerts(String farmId, {String? state}) async => _list(
        await _client.get(
          'v1/farms/$farmId/alerts',
          query: state == null ? null : {'state': state},
        ),
        Alert.fromJson,
      );
}
