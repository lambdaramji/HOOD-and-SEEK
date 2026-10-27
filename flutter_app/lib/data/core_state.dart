/// SafetyCore → Flutter `state` 메시지 (TCP 계약 state v4, `contracts/state.schema.json`).
///
/// state `v` 1도 받는다(#116). Pi 4 Core와 Pi 5 앱을 어느 쪽부터 올려도 화면이 끊기지 않게
/// 하려는 것이고, `v` 1이면 v2에서 더한 값(잠금 해제 남은 초)은 null이다. v3(D-058·D-059)은
/// `lock`에 `locked_user`(사용자 잠금)를 더하고, hazard를 정상 조리 low·mid·high와
/// warning·danger·fault로 나눴다. v1·v2(옛 Core)의 normal·caution·TEMP_WARN 등 옛 이름은
/// 읽을 때 새 이름으로 바꾼다 — 새 앱을 먼저 올리거나 Core를 되돌려도 화면이 끊기지 않게.
///
/// 형식(필수 필드·타입·enum·범위)이 틀리면 [FormatException]을 던지고, 받는 쪽은
/// 그 메시지를 버린다(상태를 바꾸지 않음). 필드 사이의 의미 검사(`fresh=false`인데
/// `cat=true` 등)는 SafetyCore 몫이라 여기서 하지 않는다. 계약에 없는 필드는 무시하고,
/// 모르는 `reasons` 코드는 원문 그대로 둔다(화면에서 원문 표시).
library;

/// 계약 문자열 값을 가진 enum.
abstract interface class WireEnum {
  String get wire;
}

/// low·mid·high는 정상 조리의 약불(대기 포함)·중불·강불, warning부터 주의가 필요한 상태 (D-059).
enum Hazard implements WireEnum {
  low('low'),
  mid('mid'),
  high('high'),
  warning('warning'),
  danger('danger'),
  fault('fault');

  const Hazard(this.wire);
  @override
  final String wire;
}

enum Presence implements WireEnum {
  present('present'),
  absent('absent'),
  unknown('unknown');

  const Presence(this.wire);
  @override
  final String wire;
}

enum Lock implements WireEnum {
  unlocked('unlocked'),
  lockedCat('locked_cat'),
  lockedDanger('locked_danger'),

  /// 사용자가 「인덕션 잠그기」로 건 잠금 (state v3, D-058). 위험·반려동물 잠금이 있으면 그쪽이 보인다.
  lockedUser('locked_user');

  const Lock(this.wire);
  @override
  final String wire;
}

/// 클립 ID. 파일 경로(`assets/audio/<wire>.wav`, #66)에 쓰이므로 allowlist로만 받는다.
enum Alert implements WireEnum {
  none('NONE'),
  caution('CAUTION'),
  warning('WARNING'),
  danger('DANGER'),
  catLock('CAT_LOCK'),
  offlineRecover('OFFLINE_RECOVER'),
  cmdRejected('CMD_REJECTED');

  const Alert(this.wire);
  @override
  final String wire;
}

class CoreState {
  const CoreState({
    required this.seq,
    required this.tMs,
    required this.hazard,
    required this.presence,
    required this.lock,
    required this.lockReleaseLeftS,
    required this.fan,
    required this.induction,
    required this.thermal,
    required this.pm,
    required this.vision,
    required this.voice,
    required this.reasons,
    required this.alert,
    required this.thresholds,
    required this.uptimeS,
  });

  factory CoreState.fromJson(Object? json) {
    final env = _Reader.root(json);
    final version = env.oneOf('v', const [1, 2, 3, 4]) as int;
    final v2 = version != 1;
    final legacy = version < 3;
    env.constant('type', 'state');
    env.constant('src', 'core');
    final p = env.object('p');
    final sensors = p.object('sensors');
    final reasons = legacy
        ? [for (final r in p.strings('reasons')) _legacyReasons[r] ?? r]
        : p.strings('reasons');
    return CoreState(
      seq: env.integer('seq', max: 0xFFFFFFFF),
      tMs: env.integer('t_ms'),
      hazard: legacy
          ? _legacyHazard(p.oneOf('hazard', _legacyHazards) as String, reasons)
          : p.pick('hazard', Hazard.values),
      presence: p.pick('presence', Presence.values),
      lock: p.pick('lock', Lock.values),
      lockReleaseLeftS: v2 ? p.integerOrNull('lock_release_left_s') : null,
      fan: FanState._read(p.object('fan')),
      induction: InductionState._read(p.object('induction')),
      thermal: ThermalReading._read(sensors.object('thermal')),
      pm: PmReading._read(sensors.object('pm')),
      vision: VisionState._read(p.object('vision')),
      voice: switch (p.objectOrNull('voice')) {
        final r? => VoiceResult._read(r, version: version),
        null => null,
      },
      reasons: reasons,
      alert: p.pick('alert', Alert.values),
      thresholds:
          Thresholds._read(p.object('thresholds'), v2: v2, legacy: legacy),
      uptimeS: p.integer('uptime_s'),
    );
  }

  final int seq;
  final int tMs;
  final Hazard hazard;
  final Presence presence;
  final Lock lock;

  /// 위험 잠금 해제 조건 ①(안전한 상태 유지)의 남은 초. `0`이면 ① 충족.
  /// `locked_danger` ∧ hazard ≤ mid일 때만 오고, 그 밖과 state `v` 1에서는 null.
  /// Core가 센 값을 옮길 뿐 앱은 시간을 세지 않는다 (설계 v1 §4.5.1).
  final int? lockReleaseLeftS;
  final FanState fan;
  final InductionState induction;
  final ThermalReading thermal;
  final PmReading pm;
  final VisionState vision;

  /// 부팅 후 음성 명령이 없었으면 null.
  final VoiceResult? voice;

  /// 현재 상태를 만든 조건 코드, 우선순위 순 (코드표: `contracts/README.md`).
  final List<String> reasons;
  final Alert alert;

  /// 그래프 임계선용 표시 사본. 앱은 이 값으로 비교·판단하지 않는다.
  final Thresholds thresholds;
  final int uptimeS;

  /// v2 이후 값이 있으면 지금 Core와 같은 state `v` 4, 없으면 옛 이름의 `v` 1 모양으로 쓴다.
  Map<String, Object?> toJson() {
    final v1 = thresholds.lockReleaseS == null;
    return {
      'v': v1 ? 1 : 4,
      'type': 'state',
      'src': 'core',
      'seq': seq,
      't_ms': tMs,
      'p': {
        'hazard': v1 ? _wireV1(hazard) : hazard.wire,
        'presence': presence.wire,
        'lock': lock.wire,
        if (thresholds.lockReleaseS != null)
          'lock_release_left_s': lockReleaseLeftS,
        'fan': fan.toJson(),
        'induction': induction.toJson(),
        'sensors': {'thermal': thermal.toJson(), 'pm': pm.toJson()},
        'vision': vision.toJson(),
        'voice': voice?.toJson(),
        'reasons': v1
            ? [
                for (final r in reasons)
                  _legacyReasons.entries
                          .where((e) => e.value == r)
                          .firstOrNull
                          ?.key ??
                      r
              ]
            : reasons,
        'alert': alert.wire,
        'thresholds': thresholds.toJson(legacy: v1),
        'uptime_s': uptimeS,
      },
    };
  }
}

/// v1·v2 Core의 옛 hazard 이름. warning은 강불(TEMP_WARN만)이면 high, 과열·과한 연기면 warning이다.
const _legacyHazards = ['normal', 'caution', 'warning', 'danger', 'fault'];
const _legacyReasons = {'TEMP_WARN': 'TEMP_HIGH', 'TEMP_CAUTION': 'TEMP_MID'};

Hazard _legacyHazard(String wire, List<String> reasons) => switch (wire) {
      'normal' => Hazard.low,
      'caution' => Hazard.mid,
      'warning'
          when reasons.contains('TEMP_HIGH') && !reasons.contains('SMOKE') =>
        Hazard.high,
      'warning' => Hazard.warning,
      'danger' => Hazard.danger,
      _ => Hazard.fault,
    };

String _wireV1(Hazard h) => switch (h) {
      Hazard.low => 'normal',
      Hazard.mid => 'caution',
      Hazard.high || Hazard.warning => 'warning',
      _ => h.wire,
    };

class FanState {
  const FanState({
    required this.level,
    required this.min,
    required this.requested,
    required this.ack,
  });

  factory FanState._read(_Reader r) => FanState(
        level: r.step('level'),
        min: r.step('min'),
        requested: r.step('requested'),
        ack: r.boolean('ack'),
      );

  final int level;
  final int min;
  final int requested;
  final bool ack;

  Map<String, Object?> toJson() =>
      {'level': level, 'min': min, 'requested': requested, 'ack': ack};
}

class InductionState {
  const InductionState({
    required this.level,
    required this.allowedOn,
    required this.ack,
  });

  factory InductionState._read(_Reader r) => InductionState(
        level: r.step('level'),
        allowedOn: r.boolean('allowed_on'),
        ack: r.boolean('ack'),
      );

  final int level;
  final bool allowedOn;
  final bool ack;

  Map<String, Object?> toJson() =>
      {'level': level, 'allowed_on': allowedOn, 'ack': ack};
}

class ThermalReading {
  const ThermalReading({
    required this.valid,
    required this.maxC,
    required this.medianC,
    required this.metric,
  });

  /// `valid=false`면 값은 전부 null(계약). 어긋나게 값이 와도 화면에 내지 않는다.
  factory ThermalReading._read(_Reader r) => r.boolean('valid')
      ? ThermalReading(
          valid: true,
          maxC: r.number('max_c', min: -40, max: 300),
          medianC: r.number('median_c', min: -40, max: 300),
          metric: r.number('metric'),
        )
      : const ThermalReading(
          valid: false, maxC: null, medianC: null, metric: null);

  final bool valid;
  final double? maxC;
  final double? medianC;

  /// T_metric = max − median (°C).
  final double? metric;

  Map<String, Object?> toJson() =>
      {'valid': valid, 'max_c': maxC, 'median_c': medianC, 'metric': metric};
}

class PmReading {
  const PmReading({
    required this.valid,
    required this.pm25,
    required this.tsize,
    required this.baseline,
  });

  /// `valid=false`면 `baseline`까지 전부 null(계약). 온도와 같은 방식.
  factory PmReading._read(_Reader r) => r.boolean('valid')
      ? PmReading(
          valid: true,
          pm25: r.number('pm25'),
          tsize: r.number('tsize'),
          baseline: r.number('baseline', nullable: true),
        )
      : const PmReading(valid: false, pm25: null, tsize: null, baseline: null);

  final bool valid;
  final double? pm25;
  final double? tsize;

  /// 부팅 후 60 s 동안과 `valid=false`일 때 null.
  final double? baseline;

  Map<String, Object?> toJson() =>
      {'valid': valid, 'pm25': pm25, 'tsize': tsize, 'baseline': baseline};
}

class VisionState {
  const VisionState({
    required this.fresh,
    required this.person,
    required this.cat,
  });

  factory VisionState._read(_Reader r) => VisionState(
        fresh: r.boolean('fresh'),
        person: r.boolean('person'),
        cat: r.boolean('cat'),
      );

  final bool fresh;
  final bool person;
  final bool cat;

  Map<String, Object?> toJson() =>
      {'fresh': fresh, 'person': person, 'cat': cat};
}

class VoiceResult {
  const VoiceResult({
    required this.lastIntent,
    required this.value,
    required this.accepted,
    required this.reason,
  });

  factory VoiceResult._read(_Reader r, {required int version}) {
    final intent = r.string('last_intent');
    final value = r.integerOrNull('value', min: 1, max: 3);
    if (intent == 'IND_SET' && (version < 4 || value == null)) {
      r._bad('IND_SET', value, 'state v4, value 1..3');
    }
    return VoiceResult(
      lastIntent: intent,
      value: value,
      accepted: r.boolean('accepted'),
      reason: r.stringOrNull('reason'),
    );
  }

  final String lastIntent;

  /// `FAN_SET`·`IND_SET`의 목표 단수 1..3, 그 외 null.
  final int? value;
  final bool accepted;

  /// 거부 사유 코드. `accepted=true`면 null.
  final String? reason;

  Map<String, Object?> toJson() => {
        'last_intent': lastIntent,
        'value': value,
        'accepted': accepted,
        'reason': reason,
      };
}

class Thresholds {
  const Thresholds({
    required this.tMid,
    required this.tHigh,
    required this.tDanger,
    required this.pmWarn,
    required this.pmSmoke,
    required this.lockReleaseS,
  });

  factory Thresholds._read(_Reader r,
          {required bool v2, required bool legacy}) =>
      Thresholds(
        tMid: r.number(legacy ? 't_caution' : 't_mid')!,
        tHigh: r.number(legacy ? 't_warn' : 't_high')!,
        tDanger: r.number('t_danger')!,
        pmWarn: r.number('pm_warn')!,
        pmSmoke: r.number('pm_smoke')!,
        lockReleaseS: v2 ? r.number('lock_release_s') : null,
      );

  /// 중불(mid)·강불(high) 경계 사본 (`P_T_mid`·`P_T_high`, D-059).
  final double tMid;
  final double tHigh;
  final double tDanger;
  final double pmWarn;
  final double pmSmoke;

  /// `P_lock_release_s` 사본. 남은 초 진행 원의 전체 길이. state `v` 1이면 null.
  final double? lockReleaseS;

  Map<String, Object?> toJson({bool legacy = false}) => {
        legacy ? 't_caution' : 't_mid': tMid,
        legacy ? 't_warn' : 't_high': tHigh,
        't_danger': tDanger,
        'pm_warn': pmWarn,
        'pm_smoke': pmSmoke,
        if (lockReleaseS != null) 'lock_release_s': lockReleaseS,
      };
}

/// JSON 객체 한 단계를 읽으면서 틀린 곳을 `p.fan.level` 같은 경로로 알려 준다.
class _Reader {
  _Reader(this._map, this._path);

  factory _Reader.root(Object? json) {
    if (json is! Map<String, Object?>) {
      throw const FormatException('state: JSON 객체가 아님');
    }
    return _Reader(json, '');
  }

  final Map<String, Object?> _map;
  final String _path;

  String _at(String key) => _path.isEmpty ? key : '$_path.$key';

  Object? _get(String key) {
    if (!_map.containsKey(key)) {
      throw FormatException('${_at(key)}: 필수 필드 없음');
    }
    return _map[key];
  }

  Never _bad(String key, Object? value, String expected) =>
      throw FormatException('${_at(key)}: $expected 아님 ($value)');

  void constant(String key, Object expected) {
    final v = _get(key);
    if (v != expected) _bad(key, v, '$expected');
  }

  Object oneOf(String key, List<Object> allowed) {
    final v = _get(key);
    if (v == null || !allowed.contains(v)) _bad(key, v, allowed.join('·'));
    return v;
  }

  _Reader object(String key) {
    final v = _get(key);
    if (v is! Map<String, Object?>) _bad(key, v, '객체');
    return _Reader(v, _at(key));
  }

  _Reader? objectOrNull(String key) => _get(key) == null ? null : object(key);

  bool boolean(String key) {
    final v = _get(key);
    if (v is! bool) _bad(key, v, 'bool');
    return v;
  }

  int integer(String key, {int min = 0, int? max}) {
    final v = _get(key);
    if (v is! int || v < min || (max != null && v > max)) {
      _bad(key, v, max == null ? '$min 이상 정수' : '$min..$max 정수');
    }
    return v;
  }

  int? integerOrNull(String key, {int min = 0, int? max}) =>
      _get(key) == null ? null : integer(key, min: min, max: max);

  int step(String key) => integer(key, max: 3);

  double? number(
    String key, {
    double min = 0,
    double max = double.infinity,
    bool nullable = false,
  }) {
    final v = _get(key);
    if (v == null && nullable) return null;
    if (v is! num || v < min || v > max) _bad(key, v, '$min..$max 숫자');
    return v.toDouble();
  }

  String string(String key) {
    final v = _get(key);
    if (v is! String) _bad(key, v, '문자열');
    return v;
  }

  String? stringOrNull(String key) => _get(key) == null ? null : string(key);

  List<String> strings(String key) {
    final v = _get(key);
    if (v is! List || v.any((e) => e is! String)) _bad(key, v, '문자열 배열');
    return List.unmodifiable(v.cast<String>());
  }

  T pick<T extends WireEnum>(String key, List<T> values) {
    final v = _get(key);
    for (final value in values) {
      if (value.wire == v) return value;
    }
    _bad(key, v, values.map((e) => e.wire).join('|'));
  }
}
