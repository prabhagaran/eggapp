/// Build-time configuration.
///
/// [apiBaseUrl] points at nila's **Tailscale** address, not its LAN IP — a
/// Tailscale address is reachable whether the phone is on home WiFi or
/// anywhere else with internet, as long as Tailscale is connected on both
/// ends. This carries over the same reasoning (and the same host) as the
/// Kotlin app's `API_BASE_URL` buildConfigField.
///
/// Override per build without editing this file:
///   flutter run --dart-define=API_BASE_URL=http://10.0.2.2:3001/
/// which is what an emulator hitting an API on the dev machine needs.
class AppConfig {
  const AppConfig._();

  static const String apiBaseUrl = String.fromEnvironment(
    'API_BASE_URL',
    defaultValue: 'http://100.76.190.23:3001/',
  );
}
