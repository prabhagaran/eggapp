import 'package:flutter/material.dart';

import '../../data/api_client.dart';
import 'app_components.dart';

/// Loads [load] and renders it, with the loading / error / retry / pull-to-
/// refresh behaviour every screen in this app needs, written once.
///
/// Screens that poll (incubators, at the web client's 15s cadence) pass a
/// [refreshInterval]; the timer is cancelled with the widget, so leaving the
/// screen stops the traffic.
class AsyncView<T> extends StatefulWidget {
  final Future<T> Function() load;
  final Widget Function(BuildContext context, T data) builder;
  final Duration? refreshInterval;

  const AsyncView({
    required this.load,
    required this.builder,
    this.refreshInterval,
    super.key,
  });

  @override
  State<AsyncView<T>> createState() => AsyncViewState<T>();
}

class AsyncViewState<T> extends State<AsyncView<T>> {
  T? _data;
  String? _error;
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _fetch();
    final interval = widget.refreshInterval;
    if (interval != null) {
      _schedule(interval);
    }
  }

  void _schedule(Duration interval) {
    Future.delayed(interval, () {
      if (!mounted) return;
      // Refresh silently — a poll must never replace the visible data with a
      // spinner, or the screen would flash every interval.
      _fetch(silent: true);
      _schedule(interval);
    });
  }

  Future<void> _fetch({bool silent = false}) async {
    if (!silent) setState(() => _loading = true);
    try {
      final data = await widget.load();
      if (!mounted) return;
      setState(() {
        _data = data;
        _error = null;
        _loading = false;
      });
    } on ApiException catch (e) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        // A failed background poll must not wipe data already on screen — the
        // stale reading is more useful to a field worker than an error page.
        if (!silent || _data == null) _error = e.message;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        if (!silent || _data == null) _error = '$e';
      });
    }
  }

  /// Lets a parent trigger a reload — e.g. after returning from a detail screen.
  Future<void> reload() => _fetch(silent: true);

  @override
  Widget build(BuildContext context) {
    if (_loading && _data == null) {
      return const Center(child: CircularProgressIndicator());
    }
    if (_error != null && _data == null) {
      return ErrorState(message: _error!, onRetry: _fetch);
    }
    return RefreshIndicator(
      onRefresh: () => _fetch(silent: true),
      child: widget.builder(context, _data as T),
    );
  }
}
