import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/core_state.dart';

// 계약 예제를 그대로 쓴다. flutter test는 flutter_app/에서 돈다.
final _examples = Directory('../contracts/examples');

Object? _load(String name) =>
    jsonDecode(File('${_examples.path}/$name').readAsStringSync());

Map<String, Object?> _idle() =>
    _load('state.idle.json')! as Map<String, Object?>;

/// 옛 Core(state v1·v2)가 보내던 이름으로 되돌린다: mid → caution, TEMP_MID → TEMP_CAUTION, t_mid → t_caution 등.
void _legacyNames(Map<String, Object?> json) {
  final p = json['p']! as Map<String, Object?>;
  p['hazard'] = const {
        'low': 'normal',
        'mid': 'caution',
        'high': 'warning'
      }[p['hazard']] ??
      p['hazard'];
  p['reasons'] = [
    for (final r in p['reasons']! as List)
      const {'TEMP_MID': 'TEMP_CAUTION', 'TEMP_HIGH': 'TEMP_WARN'}[r] ?? r
  ];
  final t = p['thresholds']! as Map<String, Object?>;
  t['t_caution'] = t.remove('t_mid');
  t['t_warn'] = t.remove('t_high');
}

void main() {
  test('계약 state 예제 전부 파싱되고 toJson으로 원문이 복원된다', () {
    final files = _examples
        .listSync()
        .whereType<File>()
        .where((f) => RegExp(r'/state(\..+)?\.json$').hasMatch(f.path))
        .toList();
    expect(files, hasLength(6));
    for (final file in files) {
      final json = jsonDecode(file.readAsStringSync());
      expect(CoreState.fromJson(json).toJson(), json, reason: file.path);
    }
  });

  test('state v4 IND_SET: 목표 단수 보존, v3·값 누락은 거부', () {
    final json = _load('state.ind-set.json')! as Map<String, Object?>;
    final s = CoreState.fromJson(json);
    expect((s.voice?.lastIntent, s.voice?.value), ('IND_SET', 2));
    json['v'] = 3;
    expect(() => CoreState.fromJson(json), throwsFormatException);
    expect(
        () =>
            CoreState.fromJson(_load('invalid/state.ind-set-null-value.json')),
        throwsFormatException);
    final old = _idle()..['v'] = 3;
    expect(CoreState.fromJson(old).hazard, Hazard.low);
  });

  test('state.json 값이 필드에 제대로 들어간다', () {
    final s = CoreState.fromJson(_load('state.json'));
    expect(s.seq, 40211);
    expect(s.hazard, Hazard.high); // 강불 + 자리 비움 (D-059)
    expect(s.presence, Presence.absent);
    expect(s.lock, Lock.lockedCat);
    expect((s.fan.level, s.fan.min, s.fan.requested), (3, 3, 1));
    expect(s.induction.allowedOn, isFalse);
    expect(s.thermal.metric, 24.0);
    expect((s.voice?.value, s.voice?.reason), (null, 'LOCKED'));
    expect(s.reasons, ['TEMP_HIGH', 'ABSENT', 'CAT_NEAR']);
    expect(s.alert, Alert.catLock);
    expect(s.thresholds.tHigh, 22);
  });

  test('state.lock-release: 위험 잠금 남은 초와 전체 길이 (v2, #116)', () {
    final s = CoreState.fromJson(_load('state.lock-release.json'));
    expect((s.lock, s.hazard), (Lock.lockedDanger, Hazard.mid));
    expect(s.lockReleaseLeftS, 7);
    expect(s.thresholds.lockReleaseS, 10);
  });

  test('state.user-lock: 사용자 잠금은 locked_user, 켜기 막힘, 남은 초 null (v3, D-058)',
      () {
    final s = CoreState.fromJson(_load('state.user-lock.json'));
    expect(s.lock, Lock.lockedUser);
    expect((s.induction.level, s.induction.allowedOn), (0, false));
    expect(s.lockReleaseLeftS, isNull);
  });

  test('state v2도 받는다 (Pi 4 Core를 나중에 올려도 화면이 끊기지 않게)', () {
    final v2 = _load('state.lock-release.json')! as Map<String, Object?>;
    v2['v'] = 2;
    _legacyNames(v2);
    final s = CoreState.fromJson(v2);
    expect((s.lock, s.lockReleaseLeftS), (Lock.lockedDanger, 7));
    // 옛 이름은 새 이름으로 읽는다 (D-059)
    expect((s.hazard, s.reasons.first, s.thresholds.tMid),
        (Hazard.mid, 'TEMP_MID', 16));
  });

  test('옛 Core의 warning은 강불(TEMP_WARN만)이면 high, 과한 연기가 붙으면 warning (D-059)',
      () {
    final strong = _load('state.json')! as Map<String, Object?>;
    strong['v'] = 2;
    _legacyNames(strong); // hazard warning, TEMP_WARN
    expect(CoreState.fromJson(strong).hazard, Hazard.high);
    final smoky = _load('state.json')! as Map<String, Object?>;
    smoky['v'] = 2;
    _legacyNames(smoky);
    (smoky['p']! as Map)['reasons'] = ['SMOKE', 'TEMP_WARN'];
    expect(CoreState.fromJson(smoky).hazard, Hazard.warning);
  });

  test('state v1도 받는다: 남은 초는 null, toJson은 v1 모양 그대로', () {
    final v1 = _load('state.lock-release.json')! as Map<String, Object?>;
    final p = v1['p']! as Map<String, Object?>;
    v1['v'] = 1;
    _legacyNames(v1);
    p.remove('lock_release_left_s');
    (p['thresholds']! as Map).remove('lock_release_s');
    final s = CoreState.fromJson(v1);
    expect(s.lock, Lock.lockedDanger);
    expect((s.lockReleaseLeftS, s.thresholds.lockReleaseS), (null, null));
    expect(s.toJson(), v1);
  });

  test('state v1에 v2 필드가 와도 읽지 않는다', () {
    final json = _load('state.lock-release.json')! as Map<String, Object?>;
    json['v'] = 1;
    _legacyNames(json);
    final s = CoreState.fromJson(json);
    expect((s.lockReleaseLeftS, s.thresholds.lockReleaseS), (null, null));
  });

  test('state.fault: 센서 무효면 값 null, voice 수락', () {
    final s = CoreState.fromJson(_load('state.fault.json'));
    expect(s.hazard, Hazard.fault);
    expect(s.thermal.valid, isFalse);
    expect(s.thermal.metric, isNull);
    expect(s.vision.fresh, isFalse);
    expect((s.voice?.value, s.voice?.accepted), (3, true));
  });

  group('형식 오류는 FormatException (메시지 폐기)', () {
    for (final name in [
      'state.fan-level-4',
      'state.hazard-offline',
      'state.lock-release-negative',
    ]) {
      test('invalid/$name', () {
        expect(() => CoreState.fromJson(_load('invalid/$name.json')),
            throwsFormatException);
      });
    }

    final cases = <String, void Function(Map<String, Object?>)>{
      'v=5': (j) => j['v'] = 5,
      'v 문자열': (j) => j['v'] = '2',
      'v2인데 lock_release_left_s 없음': (j) =>
          (j['p']! as Map).remove('lock_release_left_s'),
      'v2인데 thresholds.lock_release_s 없음': (j) =>
          ((j['p']! as Map)['thresholds'] as Map).remove('lock_release_s'),
      'lock_release_left_s 실수': (j) =>
          (j['p']! as Map)['lock_release_left_s'] = 6.5,
      'type=hello': (j) => j['type'] = 'hello',
      'seq 음수': (j) => j['seq'] = -1,
      'seq 32-bit 초과': (j) => j['seq'] = 4294967296,
      'p 없음': (j) => j.remove('p'),
      'hazard 없음': (j) => (j['p']! as Map).remove('hazard'),
      'alert 모르는 값': (j) => (j['p']! as Map)['alert'] = '../etc',
      'fan.level 실수': (j) => ((j['p']! as Map)['fan'] as Map)['level'] = 1.5,
      'valid=true인데 metric null': (j) =>
          (((j['p']! as Map)['sensors'] as Map)['thermal'] as Map)['metric'] =
              null,
      'reasons에 숫자': (j) => (j['p']! as Map)['reasons'] = [1],
      'voice.value 4': (j) => (j['p']! as Map)['voice'] = {
            'last_intent': 'FAN_SET',
            'value': 4,
            'accepted': true,
            'reason': null,
          },
    };
    cases.forEach((name, mutate) {
      test(name, () {
        final json = _idle();
        mutate(json);
        expect(() => CoreState.fromJson(json), throwsFormatException);
      });
    });

    test('JSON 객체가 아님', () {
      expect(() => CoreState.fromJson([]), throwsFormatException);
    });
  });

  // 스키마는 막지만 화면이 깨지지 않으므로 받아들이는 것. 의미 검사는 SafetyCore 몫.
  group('받아들이는 것', () {
    test('invalid/state.extra-field — 계약에 없는 필드는 무시', () {
      expect(
          CoreState.fromJson(_load('invalid/state.extra-field.json')).seq, 120);
    });

    test('invalid/state.unknown-reason — 모르는 코드는 원문 유지', () {
      final s = CoreState.fromJson(_load('invalid/state.unknown-reason.json'));
      expect(s.reasons, ['TEMP_HIGH', 'OVERHEAT']);
    });

    test('invalid/state.{thermal,pm}-invalid-with-value — 무효 센서 값은 버림', () {
      final t = CoreState.fromJson(
          _load('invalid/state.thermal-invalid-with-value.json'));
      expect((t.thermal.valid, t.thermal.metric, t.thermal.maxC),
          (false, null, null));
      final p =
          CoreState.fromJson(_load('invalid/state.pm-invalid-with-value.json'));
      expect((p.pm.valid, p.pm.pm25, p.pm.baseline), (false, null, null));
    });

    for (final name in [
      'state.accepted-with-reason',
      'state.stale-vision-cat',
      'state.voice-value-without-fan-set',
      'state.lock-release-while-unlocked',
      'state.lock-release-while-danger',
    ]) {
      test('invalid/$name — 필드 간 의미 검사는 하지 않음', () {
        expect(() => CoreState.fromJson(_load('invalid/$name.json')),
            returnsNormally);
      });
    }
  });
}
