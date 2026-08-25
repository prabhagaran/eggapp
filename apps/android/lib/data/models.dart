/// Wire-format DTOs. Field names match docs/api/openapi.yaml exactly
/// (camelCase JSON) — not adapted, so a reader can compare these against the
/// API spec line for line.
///
/// Hand-written `fromJson` rather than generated: no build_runner step to keep
/// in sync, and the parsing rules below are explicit where a reader can see
/// them.
library;

/// A null numeric means "no reading" / "not fitted" and must survive as null —
/// rendering it as 0 would show a fault as a real measurement.
double? _d(dynamic v) => v == null ? null : (v as num).toDouble();
int? _i(dynamic v) => v == null ? null : (v as num).toInt();
DateTime? _t(dynamic v) => v == null ? null : DateTime.parse(v as String).toLocal();

class TokenPair {
  final String accessToken;
  final String refreshToken;
  const TokenPair({required this.accessToken, required this.refreshToken});

  factory TokenPair.fromJson(Map<String, dynamic> j) => TokenPair(
        accessToken: j['accessToken'] as String,
        refreshToken: j['refreshToken'] as String,
      );
}

class Me {
  final String id;
  final String email;
  final String? name;
  const Me({required this.id, required this.email, this.name});

  factory Me.fromJson(Map<String, dynamic> j) =>
      Me(id: j['id'] as String, email: j['email'] as String, name: j['name'] as String?);
}

class Farm {
  final String id;
  final String name;
  final String timezone;
  final String? location;
  final String role;
  const Farm({
    required this.id,
    required this.name,
    required this.timezone,
    this.location,
    required this.role,
  });

  factory Farm.fromJson(Map<String, dynamic> j) => Farm(
        id: j['id'] as String,
        name: j['name'] as String,
        timezone: j['timezone'] as String? ?? 'UTC',
        location: j['location'] as String?,
        role: j['role'] as String? ?? 'member',
      );
}

class DeviceSummary {
  final String id;
  final String hardwareId;
  final String? name;
  final String status;
  final DateTime? lastSeenAt;
  final double? currentTempSetpoint;
  final double? currentTempHysteresis;
  final double? currentHumSetpoint;
  final double? currentHumHysteresis;
  final bool? currentFanOn;
  final bool? currentTurnerOn;
  final bool? currentHumidifierOn;
  final bool? currentPumpOn;

  const DeviceSummary({
    required this.id,
    required this.hardwareId,
    this.name,
    required this.status,
    this.lastSeenAt,
    this.currentTempSetpoint,
    this.currentTempHysteresis,
    this.currentHumSetpoint,
    this.currentHumHysteresis,
    this.currentFanOn,
    this.currentTurnerOn,
    this.currentHumidifierOn,
    this.currentPumpOn,
  });

  factory DeviceSummary.fromJson(Map<String, dynamic> j) => DeviceSummary(
        id: j['id'] as String,
        hardwareId: j['hardwareId'] as String? ?? '',
        name: j['name'] as String?,
        status: j['status'] as String? ?? 'unknown',
        lastSeenAt: _t(j['lastSeenAt']),
        currentTempSetpoint: _d(j['currentTempSetpoint']),
        currentTempHysteresis: _d(j['currentTempHysteresis']),
        currentHumSetpoint: _d(j['currentHumSetpoint']),
        currentHumHysteresis: _d(j['currentHumHysteresis']),
        currentFanOn: j['currentFanOn'] as bool?,
        currentTurnerOn: j['currentTurnerOn'] as bool?,
        currentHumidifierOn: j['currentHumidifierOn'] as bool?,
        currentPumpOn: j['currentPumpOn'] as bool?,
      );
}

class LatestTelemetry {
  final DateTime ts;
  final double? tempC;
  final double? humidityPct;
  final bool? turnerOn;

  /// Relay states at the moment of this reading. null = firmware predating the
  /// actuator fields, not "off" — rendered as an em dash, never as OFF.
  final bool? heaterOn;
  final bool? coolerOn;
  final bool? humidifierOn;
  final bool? fanOn;
  final bool? pumpOn;
  final String source;

  const LatestTelemetry({
    required this.ts,
    this.tempC,
    this.humidityPct,
    this.turnerOn,
    this.heaterOn,
    this.coolerOn,
    this.humidifierOn,
    this.fanOn,
    this.pumpOn,
    required this.source,
  });

  factory LatestTelemetry.fromJson(Map<String, dynamic> j) => LatestTelemetry(
        ts: DateTime.parse(j['ts'] as String).toLocal(),
        tempC: _d(j['tempC']),
        humidityPct: _d(j['humidityPct']),
        turnerOn: j['turnerOn'] as bool?,
        heaterOn: j['heaterOn'] as bool?,
        coolerOn: j['coolerOn'] as bool?,
        humidifierOn: j['humidifierOn'] as bool?,
        fanOn: j['fanOn'] as bool?,
        pumpOn: j['pumpOn'] as bool?,
        source: j['source'] as String? ?? 'device',
      );
}

class Incubator {
  final String id;
  final String name;
  final int capacity;
  final String? deviceId;
  final DeviceSummary? device;
  final LatestTelemetry? latestTelemetry;

  const Incubator({
    required this.id,
    required this.name,
    required this.capacity,
    this.deviceId,
    this.device,
    this.latestTelemetry,
  });

  factory Incubator.fromJson(Map<String, dynamic> j) => Incubator(
        id: j['id'] as String,
        name: j['name'] as String,
        capacity: _i(j['capacity']) ?? 0,
        deviceId: j['deviceId'] as String?,
        device: j['device'] == null
            ? null
            : DeviceSummary.fromJson(j['device'] as Map<String, dynamic>),
        latestTelemetry: j['latestTelemetry'] == null
            ? null
            : LatestTelemetry.fromJson(j['latestTelemetry'] as Map<String, dynamic>),
      );
}

class SpeciesRef {
  final String id;
  final String name;
  final int incubationDays;
  const SpeciesRef({required this.id, required this.name, required this.incubationDays});

  factory SpeciesRef.fromJson(Map<String, dynamic> j) => SpeciesRef(
        id: j['id'] as String,
        name: j['name'] as String,
        incubationDays: _i(j['incubationDays']) ?? 0,
      );
}

class IncubatorRef {
  final String id;
  final String name;
  const IncubatorRef({required this.id, required this.name});

  factory IncubatorRef.fromJson(Map<String, dynamic> j) =>
      IncubatorRef(id: j['id'] as String, name: j['name'] as String);
}

class Batch {
  final String id;
  final String incubatorId;
  final String speciesId;
  final String status;
  final DateTime? setAt;
  final List<int> candlingDays;
  final DateTime? lockdownAt;
  final DateTime? expectedHatchAt;
  final int viableCount;
  final double? fertilityPct;
  final double? hatchOfSetPct;
  final double? hatchOfFertilePct;
  final SpeciesRef? species;
  final IncubatorRef? incubator;

  /// The device's own day counter and hatch estimate, mirrored onto the batch
  /// by the MQTT ingest. Cross-check only — it never overwrites the manual
  /// schedule. See docs/iot/telemetry-contract.md.
  final int? deviceDay;
  final DateTime? deviceExpectedHatchAt;

  const Batch({
    required this.id,
    required this.incubatorId,
    required this.speciesId,
    required this.status,
    this.setAt,
    this.candlingDays = const [],
    this.lockdownAt,
    this.expectedHatchAt,
    required this.viableCount,
    this.fertilityPct,
    this.hatchOfSetPct,
    this.hatchOfFertilePct,
    this.species,
    this.incubator,
    this.deviceDay,
    this.deviceExpectedHatchAt,
  });

  factory Batch.fromJson(Map<String, dynamic> j) => Batch(
        id: j['id'] as String,
        incubatorId: j['incubatorId'] as String,
        speciesId: j['speciesId'] as String,
        status: j['status'] as String,
        setAt: _t(j['setAt']),
        candlingDays:
            (j['candlingDays'] as List?)?.map((e) => (e as num).toInt()).toList() ?? const [],
        lockdownAt: _t(j['lockdownAt']),
        expectedHatchAt: _t(j['expectedHatchAt']),
        viableCount: _i(j['viableCount']) ?? 0,
        fertilityPct: _d(j['fertilityPct']),
        hatchOfSetPct: _d(j['hatchOfSetPct']),
        hatchOfFertilePct: _d(j['hatchOfFertilePct']),
        species:
            j['species'] == null ? null : SpeciesRef.fromJson(j['species'] as Map<String, dynamic>),
        incubator: j['incubator'] == null
            ? null
            : IncubatorRef.fromJson(j['incubator'] as Map<String, dynamic>),
        deviceDay: _i(j['deviceDay']),
        deviceExpectedHatchAt: _t(j['deviceExpectedHatchAt']),
      );
}

class BatchSource {
  final String collectionId;
  final int count;
  final EggCollection? collection;
  const BatchSource({required this.collectionId, required this.count, this.collection});

  factory BatchSource.fromJson(Map<String, dynamic> j) => BatchSource(
        collectionId: j['collectionId'] as String,
        count: _i(j['count']) ?? 0,
        collection: j['collection'] == null
            ? null
            : EggCollection.fromJson(j['collection'] as Map<String, dynamic>),
      );
}

/// A candling session already accepted by the server. Used to work out which
/// scheduled days are still outstanding when pre-filling the capture form.
class CandlingSession {
  final String id;
  final int dayNo;
  final DateTime candledAt;
  final int fertile;
  final int clear;
  final int bloodRing;
  final int unsure;

  const CandlingSession({
    required this.id,
    required this.dayNo,
    required this.candledAt,
    required this.fertile,
    required this.clear,
    required this.bloodRing,
    required this.unsure,
  });

  factory CandlingSession.fromJson(Map<String, dynamic> j) => CandlingSession(
        id: j['id'] as String,
        dayNo: _i(j['dayNo']) ?? 0,
        candledAt: DateTime.parse(j['candledAt'] as String).toLocal(),
        fertile: _i(j['fertile']) ?? 0,
        clear: _i(j['clear']) ?? 0,
        bloodRing: _i(j['bloodRing']) ?? 0,
        unsure: _i(j['unsure']) ?? 0,
      );
}

/// Detail view — separate from [Batch] because only the single-batch GET
/// includes egg sources and candling history, matching the API's own
/// list/detail split.
class BatchDetail {
  final Batch batch;
  final List<BatchSource> sources;
  final List<CandlingSession> candlings;

  const BatchDetail({
    required this.batch,
    this.sources = const [],
    this.candlings = const [],
  });

  factory BatchDetail.fromJson(Map<String, dynamic> j) => BatchDetail(
        batch: Batch.fromJson(j),
        sources: (j['sources'] as List? ?? [])
            .map((e) => BatchSource.fromJson(e as Map<String, dynamic>))
            .toList(),
        candlings: (j['candlings'] as List? ?? [])
            .map((e) => CandlingSession.fromJson(e as Map<String, dynamic>))
            .toList(),
      );
}

class EggCollection {
  final String id;
  final DateTime collectedOn;
  final int count;
  final int discardedCount;
  final double? avgWeightGrams;
  final String? sourceNote;
  final int assignedCount;
  final int availableCount;

  const EggCollection({
    required this.id,
    required this.collectedOn,
    required this.count,
    required this.discardedCount,
    this.avgWeightGrams,
    this.sourceNote,
    required this.assignedCount,
    required this.availableCount,
  });

  factory EggCollection.fromJson(Map<String, dynamic> j) => EggCollection(
        id: j['id'] as String,
        collectedOn: DateTime.parse(j['collectedOn'] as String).toLocal(),
        count: _i(j['count']) ?? 0,
        discardedCount: _i(j['discardedCount']) ?? 0,
        avgWeightGrams: _d(j['avgWeightGrams']),
        sourceNote: j['sourceNote'] as String?,
        assignedCount: _i(j['assignedCount']) ?? 0,
        availableCount: _i(j['availableCount']) ?? 0,
      );

  /// Whole days in storage. BR-011 bands: >14 days blocks assignment without an
  /// override note, >7 warns.
  int get ageDays => DateTime.now().difference(collectedOn).inDays;
}

class Flock {
  final String id;
  final String name;
  final String speciesId;
  final String purpose;
  final int placedCount;
  final int? ageDays;
  final String? stage;
  final int currentCount;
  final String? speciesName;

  const Flock({
    required this.id,
    required this.name,
    required this.speciesId,
    required this.purpose,
    required this.placedCount,
    this.ageDays,
    this.stage,
    required this.currentCount,
    this.speciesName,
  });

  factory Flock.fromJson(Map<String, dynamic> j) => Flock(
        id: j['id'] as String,
        name: j['name'] as String,
        speciesId: j['speciesId'] as String,
        purpose: j['purpose'] as String? ?? '',
        placedCount: _i(j['placedCount']) ?? 0,
        ageDays: _i(j['ageDays']),
        stage: j['stage'] as String?,
        currentCount: _i(j['currentCount']) ?? 0,
        speciesName: (j['species'] as Map<String, dynamic>?)?['name'] as String?,
      );
}

class MortalityRecord {
  final String id;
  final DateTime date;
  final int count;
  final String cause;
  final String? notes;
  const MortalityRecord({
    required this.id,
    required this.date,
    required this.count,
    required this.cause,
    this.notes,
  });

  factory MortalityRecord.fromJson(Map<String, dynamic> j) => MortalityRecord(
        id: j['id'] as String,
        date: DateTime.parse(j['date'] as String).toLocal(),
        count: _i(j['count']) ?? 0,
        cause: j['cause'] as String? ?? '',
        notes: j['notes'] as String?,
      );
}

class VaccinationRecord {
  final String id;
  final DateTime date;
  final String vaccine;
  final String disease;
  final String route;
  final int count;
  final String administeredBy;
  const VaccinationRecord({
    required this.id,
    required this.date,
    required this.vaccine,
    required this.disease,
    required this.route,
    required this.count,
    required this.administeredBy,
  });

  factory VaccinationRecord.fromJson(Map<String, dynamic> j) => VaccinationRecord(
        id: j['id'] as String,
        date: DateTime.parse(j['date'] as String).toLocal(),
        vaccine: j['vaccine'] as String? ?? '',
        disease: j['disease'] as String? ?? '',
        route: j['route'] as String? ?? '',
        count: _i(j['count']) ?? 0,
        administeredBy: j['administeredBy'] as String? ?? '',
      );
}

class ComplianceItem {
  final String id;
  final int ageDaysFrom;
  final int ageDaysTo;
  final String vaccine;
  final String disease;
  final String route;
  final DateTime dueDate;

  /// "administered" | "overdue" | "due" | "upcoming"
  final String status;

  const ComplianceItem({
    required this.id,
    required this.ageDaysFrom,
    required this.ageDaysTo,
    required this.vaccine,
    required this.disease,
    required this.route,
    required this.dueDate,
    required this.status,
  });

  factory ComplianceItem.fromJson(Map<String, dynamic> j) => ComplianceItem(
        id: j['id'] as String,
        ageDaysFrom: _i(j['ageDaysFrom']) ?? 0,
        ageDaysTo: _i(j['ageDaysTo']) ?? 0,
        vaccine: j['vaccine'] as String? ?? '',
        disease: j['disease'] as String? ?? '',
        route: j['route'] as String? ?? '',
        dueDate: DateTime.parse(j['dueDate'] as String).toLocal(),
        status: j['status'] as String? ?? 'upcoming',
      );
}

class FeedLog {
  final String id;
  final DateTime loggedAt;
  final String feedType;
  final double quantityKg;
  final bool stageMismatch;
  const FeedLog({
    required this.id,
    required this.loggedAt,
    required this.feedType,
    required this.quantityKg,
    required this.stageMismatch,
  });

  factory FeedLog.fromJson(Map<String, dynamic> j) => FeedLog(
        id: j['id'] as String,
        loggedAt: DateTime.parse(j['loggedAt'] as String).toLocal(),
        feedType: j['feedType'] as String? ?? '',
        quantityKg: _d(j['quantityKg']) ?? 0,
        stageMismatch: j['stageMismatch'] as bool? ?? false,
      );
}

class WaterLog {
  final String id;
  final DateTime loggedAt;
  final double quantityLiters;
  const WaterLog({required this.id, required this.loggedAt, required this.quantityLiters});

  factory WaterLog.fromJson(Map<String, dynamic> j) => WaterLog(
        id: j['id'] as String,
        loggedAt: DateTime.parse(j['loggedAt'] as String).toLocal(),
        quantityLiters: _d(j['quantityLiters']) ?? 0,
      );
}

class FlockDetail {
  final String id;
  final String name;
  final String purpose;
  final int placedCount;
  final int? ageDays;
  final String? stage;
  final int currentCount;
  final List<MortalityRecord> mortalityRecords;
  final List<VaccinationRecord> vaccinationRecords;
  final List<FeedLog> recentFeed;
  final List<WaterLog> recentWater;

  const FlockDetail({
    required this.id,
    required this.name,
    required this.purpose,
    required this.placedCount,
    this.ageDays,
    this.stage,
    required this.currentCount,
    this.mortalityRecords = const [],
    this.vaccinationRecords = const [],
    this.recentFeed = const [],
    this.recentWater = const [],
  });

  factory FlockDetail.fromJson(Map<String, dynamic> j) => FlockDetail(
        id: j['id'] as String,
        name: j['name'] as String,
        purpose: j['purpose'] as String? ?? '',
        placedCount: _i(j['placedCount']) ?? 0,
        ageDays: _i(j['ageDays']),
        stage: j['stage'] as String?,
        currentCount: _i(j['currentCount']) ?? 0,
        mortalityRecords: (j['mortalityRecords'] as List? ?? [])
            .map((e) => MortalityRecord.fromJson(e as Map<String, dynamic>))
            .toList(),
        vaccinationRecords: (j['vaccinationRecords'] as List? ?? [])
            .map((e) => VaccinationRecord.fromJson(e as Map<String, dynamic>))
            .toList(),
        recentFeed: (j['recentFeed'] as List? ?? [])
            .map((e) => FeedLog.fromJson(e as Map<String, dynamic>))
            .toList(),
        recentWater: (j['recentWater'] as List? ?? [])
            .map((e) => WaterLog.fromJson(e as Map<String, dynamic>))
            .toList(),
      );
}

class Alert {
  final String id;
  final String? incubatorId;

  /// "warning" | "critical"
  final String severity;

  /// "open" | "acked" | "resolved"
  final String state;
  final String message;
  final DateTime triggeredAt;
  final DateTime? resolvedAt;

  const Alert({
    required this.id,
    this.incubatorId,
    required this.severity,
    required this.state,
    required this.message,
    required this.triggeredAt,
    this.resolvedAt,
  });

  factory Alert.fromJson(Map<String, dynamic> j) => Alert(
        id: j['id'] as String,
        incubatorId: j['incubatorId'] as String?,
        severity: j['severity'] as String? ?? 'warning',
        state: j['state'] as String? ?? 'open',
        message: j['message'] as String? ?? '',
        triggeredAt: DateTime.parse(j['triggeredAt'] as String).toLocal(),
        resolvedAt: _t(j['resolvedAt']),
      );
}
