import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/core_state.dart';
import 'package:hood_seek/data/kitchen_feed.dart';
import 'package:hood_seek/screens/labels.dart';
import 'package:hood_seek/screens/sensor_view.dart';
import 'package:hood_seek/widgets/hs.dart';

// D-054(#125): 약불·중불·강불은 정상 조리, 차단은 65 °C급 고온 ∧ 과한 연기뿐이다.
// 화면은 Core가 보낸 hazard·presence·reasons를 옮기기만 한다 (#126).

CoreState _scenario(String id) =>
    demoScenarios.firstWhere((s) => s.id == id).state!;

CoreState _edit(String id, void Function(Map<String, Object?> p) edit) {
  final json = _scenario(id).toJson();
  edit(json['p']! as Map<String, Object?>);
  return CoreState.fromJson(json);
}

void main() {
  test('중불(mid, 재실): 정상 조리로 말하고 불을 줄이라고 하지 않는다', () {
    final h = headline(_scenario('mid'));
    expect(h.tone, Tone.caution);
    expect(h.pill, '중불 조리 중');
    expect(h.title, '중불로 조리 중이에요');
    expect(h.body, '후드를 2단으로 켜 두었어요.');
  });

  test('강불(high + TEMP_HIGH, 재실): 정상 조리 — 인덕션 그대로, 주의 톤, 경고음 없음 (D-059)', () {
    final s = _scenario('high');
    final h = headline(s);
    expect(s.hazard, Hazard.high);
    expect(h.pill, '강불 조리 중');
    expect(h.body, '후드를 최대로 올렸어요. 인덕션은 그대로예요.');
    expect((h.tone, temperatureGrade(s), hazardTone(s.hazard)),
        (Tone.caution, (Tone.caution, '강불'), Tone.caution));
    expect(s.alert, Alert.none);
    expect(hazardTone(_scenario('overheat').hazard), Tone.warning);
    expect(headline(_scenario('absent')).tone, Tone.warning); // 자리 비움 보호는 경고 톤
  });

  test('65 °C급 고온 단독(warning + TEMP_DANGER): 과열 주의, 게이지는 경고 톤', () {
    final s = _scenario('overheat');
    final h = headline(s);
    expect(h.tone, Tone.warning);
    expect(h.pill, '경고 · 과열 주의');
    expect(h.body, '후드를 최대로 올렸어요. 인덕션은 그대로예요.');
    expect(temperatureGrade(s), (Tone.warning, '과열'));
  });

  test('과한 연기 단독(warning + SMOKE): 공기 게이지·헤드라인 모두 경고 톤', () {
    final s = _scenario('heavy-smoke');
    final h = headline(s);
    expect(h.tone, Tone.warning);
    expect(h.pill, '경고 · 연기 많음');
    expect(pmGrade(s), (Tone.warning, '연기 많음'));
  });

  test('화재 위험(danger + TEMP_DANGER + SMOKE): P-011 §5 안내, 게이지는 위험 톤', () {
    final s = _scenario('fire');
    final h = headline(s);
    expect(h.tone, Tone.danger);
    expect(h.pill, '위험 · 화재 위험');
    expect(h.title, '화재 위험을 미리 감지해 인덕션을 껐어요');
    expect(h.body, '후드가 최대로 환기하고 있어요. 뜨거운 조리 기구는 그대로 두세요.');
    expect(pmGrade(s), (Tone.danger, '연기 많음'));
    expect(temperatureGrade(s), (Tone.danger, '위험'));
  });

  test('부재 + 65 °C급 고온 단독: 온도 때문이라고 말한다 (TEMP_HIGH 없음)', () {
    final s = _edit('absent', (p) => p['reasons'] = ['TEMP_DANGER', 'ABSENT']);
    final h = headline(s);
    expect(h.pill, '경고 · 자리 비움');
    expect(h.body, '온도가 매우 높아 후드를 최대로 올리고 인덕션은 1단까지만 쓰게 했어요.');
  });

  test('부재 + 중불(mid + ABSENT): 자리 비움과 인덕션 제한을 말한다', () {
    final s = _edit('mid', (p) {
      p['presence'] = 'absent';
      p['vision'] = {'fresh': true, 'person': false, 'cat': false};
      p['fan'] = {'level': 3, 'min': 3, 'requested': 0, 'ack': true};
      p['induction'] = {'level': 1, 'allowed_on': false, 'ack': true};
      p['reasons'] = ['TEMP_MID', 'ABSENT'];
    });
    final h = headline(s);
    expect(h.tone, Tone.caution);
    expect(h.pill, '주의 · 자리 비움');
    expect(h.body, '불이 켜져 있어 후드를 최대로 올리고 인덕션은 1단까지만 쓰게 했어요.');
  });

  test('카메라가 막 돌아온 presence=unknown(ABSENT·VISION_STALE 없음)도 재실로 보지 않는다', () {
    final s = _edit('mid', (p) {
      p['presence'] = 'unknown';
      p['induction'] = {'level': 1, 'allowed_on': false, 'ack': true};
    });
    expect(headline(s).pill, '주의 · 카메라 정보 없음');
  });

  test('PM_WARN만 있으면 hazard low 그대로, 「연기 있음」은 정상 톤', () {
    final s = _scenario('pm-warn');
    final h = headline(s);
    expect(h.tone, Tone.normal);
    expect(h.body, '온도는 정상이고 연기가 조금 있어요.');
    expect(pmGrade(s), (Tone.caution, '연기 있음'));
    expect(pmFooter(s), '센서 최대치가 이어지면 연기 많음');
  });

  test('잠금 풀린 뒤 인덕션 0인데 남은 열로 mid·TEMP_HIGH이면 「중불·강불 조리 중」이 아니다', () {
    for (final id in ['mid', 'high']) {
      final s = _edit(id, (p) => (p['induction']! as Map)['level'] = 0);
      final h = headline(s);
      expect(h.title, '인덕션은 꺼졌어요', reason: id);
      expect(h.pill, '남은 열', reason: id);
      expect(temperatureGrade(s).$2, '남은 열', reason: id);
    }
  });

  test('데모 기준값은 profiles/default.json(D-054)과 같다', () {
    final profile =
        jsonDecode(File('../profiles/default.json').readAsStringSync()) as Map;
    final t = _scenario('low').thresholds;
    expect((t.tMid, t.tHigh, t.tDanger),
        (profile['P_T_mid'], profile['P_T_high'], profile['P_T_danger']));
  });

  test('센서 고장(SENSOR_FAULT)은 어느 센서인지, 둘 다면 「센서」', () {
    expect(headline(_scenario('fault')).pill, '점검 필요 · 온도 센서');
    final both = _edit('fault', (p) {
      final sensors = p['sensors']! as Map<String, Object?>;
      sensors['pm'] = {
        'valid': false,
        'pm25': null,
        'tsize': null,
        'baseline': null
      };
    });
    expect(headline(both).pill, '점검 필요 · 센서');
  });

  test('profile을 못 읽은 fail-safe(reasons 빈 배열)는 센서 고장으로 말하지 않는다', () {
    final s = _edit('fault', (p) {
      p['reasons'] = <String>[];
      final sensors = p['sensors']! as Map<String, Object?>;
      sensors['pm'] = {
        'valid': false,
        'pm25': null,
        'tsize': null,
        'baseline': null
      };
    });
    final h = headline(s);
    expect(h.pill, '점검 필요 · 안전 장치');
    expect(h.title, '안전 장치가 아직 준비되지 않았어요');
  });

  test('온도 게이지 아래 기준: 강불·과열이면 「과열 기준」(위험이라고 안 함)', () {
    for (final id in ['high', 'overheat']) {
      final s = _scenario(id);
      expect(
          temperatureFooter(s), '과열 기준 ${threshold(s.thresholds.tDanger)} °C');
    }
  });

  test('온도 게이지는 단계대로 끊어 채운다: 대기 0 · 약불 1/3 · 중불 2/3 · 강불·과열·위험 꽉 (정인 10/26)',
      () {
    expect(temperatureFill(_scenario('idle')), 0);
    expect(temperatureGrade(_scenario('low')).$2, '정상 (약불)');
    expect(temperatureFill(_scenario('low')), 1 / 3);
    expect(temperatureFill(_scenario('mid')), 2 / 3);
    expect(temperatureFill(_scenario('high')), 1);
    expect(temperatureFill(_scenario('overheat')), 1);
    expect(temperatureFill(_scenario('fire')), 1);
    expect(temperatureFill(_scenario('fault')), isNull);
    // 꺼진 채 온도 사유 없이 warning(과한 연기)이면 문구가 「정상」이라 게이지도 비운다.
    final smokeOff = _edit('heavy-smoke', (p) {
      (p['induction']! as Map<String, Object?>)['level'] = 0;
    });
    expect(temperatureGrade(smokeOff).$2, '정상');
    expect(temperatureFill(smokeOff), 0);
  });

  test('임계선 라벨: 20 px보다 가까우면 위 선 하나에 합치고, 멀면 따로', () {
    const lines = [('경고', 39.4, Tone.warning), ('연기', 59.4, Tone.danger)];
    // 눈금이 2000대: 20 µg/m³ 차이가 몇 px밖에 안 된다.
    final near = thresholdLabels(lines, (v) => 400 - v * 0.2);
    expect(near.map((l) => l.$1), ['경고 39.4 · 연기 59.4']);
    expect(near.single.$3, Tone.danger);
    final far = thresholdLabels(lines, (v) => 400 - v * 4);
    expect(far.map((l) => l.$1), ['경고 39.4', '연기 59.4']);
  });

  test('PM2.5가 센서 상한(2000)에 닿으면 「2000+」, 아래는 정수 그대로', () {
    expect(pmNumber(2000), '2000+');
    expect(pmNumber(1999.6), '2000');
    expect(pmNumber(52), '52');
    expect(pmNumber(null), '—');
  });

  group('음성 결과 안내 클립 (#173)', () {
    VoiceResult v(String intent, [int? value]) => VoiceResult(
        lastIntent: intent, value: value, accepted: true, reason: null);
    CoreState state({
      required (int, int, int) fan,
      bool fanAck = true,
      int ind = 0,
      bool indAck = true,
    }) =>
        _edit('low', (p) {
          p['fan'] = {
            'level': fan.$1,
            'min': fan.$2,
            'requested': fan.$3,
            'ack': fanAck
          };
          p['induction'] = {'level': ind, 'allowed_on': true, 'ack': indAck};
        });

    test('후드: 요청·실제 단수가 같고 ACK가 있으면 그 단수, 끄기는 「꺼졌어요」', () {
      expect(voiceResultClip(v('FAN_SET', 3), 0, state(fan: (3, 0, 3))),
          'results/RESULT_FAN_3');
      expect(voiceResultClip(v('FAN_OFF'), 0, state(fan: (0, 0, 0))),
          'results/RESULT_FAN_OFF');
    });

    test('후드: 안전 하한으로 더 높으면 「안전을 위해 … 유지」 — 「꺼졌어요」라고 하지 않는다', () {
      expect(voiceResultClip(v('FAN_OFF'), 0, state(fan: (2, 2, 0))),
          'results/RESULT_FAN_MIN_2');
      expect(voiceResultClip(v('FAN_SET', 1), 0, state(fan: (3, 3, 1))),
          'results/RESULT_FAN_MIN_3');
    });

    test('후드: ACK 없음·요청이 아직 안 바뀜이면 기다린다(null)', () {
      expect(
          voiceResultClip(
              v('FAN_SET', 3), 0, state(fan: (3, 0, 3), fanAck: false)),
          isNull);
      expect(
          voiceResultClip(v('FAN_SET', 3), 0, state(fan: (1, 0, 1))), isNull);
    });

    test('인덕션 목표 단수: 목표·ACK가 맞을 때만 기존 결과 WAV 선택', () {
      expect(voiceResultClip(v('IND_SET', 2), 0, state(fan: (1, 0, 1), ind: 2)),
          'results/RESULT_IND_2');
      expect(voiceResultClip(v('IND_SET', 2), 0, state(fan: (1, 0, 1), ind: 1)),
          isNull);
      expect(
          voiceResultClip(
              v('IND_SET', 2), 0, state(fan: (1, 0, 1), ind: 2, indAck: false)),
          isNull);
    });

    test('인덕션: 끄기는 0이면, 켜기·올리기·내리기는 시작 단수에서 바뀐 최종 단수', () {
      expect(voiceResultClip(v('IND_OFF'), 2, state(fan: (1, 0, 1), ind: 0)),
          'results/RESULT_IND_OFF');
      expect(voiceResultClip(v('IND_UP'), 1, state(fan: (1, 0, 1), ind: 2)),
          'results/RESULT_IND_2');
      expect(voiceResultClip(v('IND_ON'), 0, state(fan: (1, 0, 1), ind: 1)),
          'results/RESULT_IND_1');
      expect(voiceResultClip(v('IND_UP'), 2, state(fan: (1, 0, 1), ind: 2)),
          isNull);
      expect(
          voiceResultClip(
              v('IND_DOWN'), 3, state(fan: (1, 0, 1), ind: 2, indAck: false)),
          isNull);
    });
  });
}
