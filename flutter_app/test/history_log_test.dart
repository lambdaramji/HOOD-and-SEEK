import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/core_state.dart';
import 'package:hood_seek/data/kitchen_feed.dart';
import 'package:hood_seek/screens/history_log.dart';

/// 정상 조리 시나리오의 JSON을 고쳐 계약 파서를 그대로 통과시킨 state.
CoreState _s({
  int ind = 0,
  int fan = 1,
  int requested = 1,
  bool fanAck = true,
  bool indAck = true,
  String presence = 'present',
  String hazard = 'low',
  String lock = 'unlocked',
  List<String> reasons = const [],
  bool thermalValid = true,
  int uptime = 100,
}) {
  final json = demoScenarios.firstWhere((s) => s.id == 'low').state!.toJson();
  final p = json['p']! as Map<String, Object?>;
  p['hazard'] = hazard;
  p['presence'] = presence;
  p['lock'] = lock;
  p['fan'] = {
    'level': fan,
    'min': (p['fan']! as Map)['min'],
    'requested': requested,
    'ack': fanAck,
  };
  p['induction'] = {'level': ind, 'allowed_on': true, 'ack': indAck};
  p['reasons'] = reasons;
  p['uptime_s'] = uptime;
  final thermal = (p['sensors']! as Map<String, Object?>)['thermal']!
      as Map<String, Object?>;
  if (!thermalValid) {
    thermal
      ..['valid'] = false
      ..['max_c'] = null
      ..['median_c'] = null
      ..['metric'] = null;
  }
  return CoreState.fromJson(json);
}

const _step = Duration(milliseconds: 500);

void main() {
  final t0 = DateTime(2026, 10, 26, 13);

  /// 0.5 s마다 [states]를 차례로 넣는다. 각 state가 [repeat]번씩 들어간다.
  DateTime feed(HistoryLog log, DateTime from, List<CoreState?> states,
      {int repeat = 1}) {
    var now = from;
    CoreState? prev;
    for (final s in states) {
      for (var i = 0; i < repeat; i++) {
        if (s != null && i == 0) log.state(prev, s, now);
        log.sample(s, now, _step);
        now = now.add(_step);
      }
      if (s != null) prev = s;
    }
    return now;
  }

  test('조리 1회: 0 → 1 이상에서 시작, 다시 0에서 끝나고 단계 비율을 센다', () {
    final log = HistoryLog(t0);
    feed(
        log,
        t0,
        [
          _s(ind: 0),
          _s(ind: 1),
          _s(ind: 2, reasons: ['TEMP_MID'], hazard: 'mid'),
          _s(ind: 3, reasons: ['TEMP_HIGH'], hazard: 'high'),
          _s(ind: 0),
        ],
        repeat: 4);
    expect(log.current, isNull);
    expect(log.recent, hasLength(1));
    final session = log.recent.first;
    expect(session.start, t0.add(const Duration(seconds: 2)));
    expect(session.length(t0), const Duration(seconds: 6));
    // 끝나는 0단 표본은 넣지 않아 「대기」가 섞이지 않는다.
    final shares = {for (final (g, _, v) in session.shares()) g: v};
    expect(shares.keys, ['약불', '중불', '강불']);
    expect(shares['약불'], closeTo(4 / 12, 1e-9));
    expect(session.chips(t0), ['조리 6초', '약불 33 %', '중불 33 %', '강불 33 %']);
  });

  test('후드 자동 상향은 level > requested 구간에 들어간 횟수', () {
    final log = HistoryLog(t0);
    feed(log, t0, [
      _s(ind: 2, fan: 1, requested: 1),
      _s(ind: 2, fan: 2, requested: 1),
      _s(ind: 2, fan: 2, requested: 1),
      _s(ind: 2, fan: 1, requested: 1),
      _s(ind: 2, fan: 3, requested: 0),
      _s(ind: 0),
    ]);
    expect(log.recent.first.autoRaises, 2);
    expect(log.recent.first.chips(t0), contains('후드 자동 상향 2번'));
  });

  test('자리 비움 핀은 비운 동안 하나로 늘어나고, 돌아왔다 다시 비우면 새로 찍는다', () {
    final log = HistoryLog(t0);
    feed(
        log,
        t0,
        [
          _s(ind: 1),
          _s(ind: 1, presence: 'absent'),
          _s(ind: 1),
          _s(ind: 1, presence: 'absent'),
        ],
        repeat: 4);
    final pins = log.current!.pins.where((p) => p.kind == PinKind.absent);
    expect(pins, hasLength(2));
    expect(pins.first.length, const Duration(milliseconds: 1500));
    expect(log.current!.absences, 2);
  });

  test('위험 차단으로 0이 되면 위험 잠금·차단 핀을 찍고 끝낸다', () {
    final log = HistoryLog(t0);
    feed(
        log,
        t0,
        [
          _s(ind: 3, reasons: ['TEMP_HIGH', 'SMOKE'], hazard: 'warning'),
          _s(
              ind: 0,
              fan: 3,
              requested: 1,
              hazard: 'danger',
              lock: 'locked_danger',
              reasons: ['TEMP_DANGER', 'SMOKE']),
        ],
        repeat: 2);
    final session = log.recent.first;
    // 같은 순간의 위험 잠금 핀은 「위험 차단」 하나로 합친다 (핀 겹침).
    expect(session.pins.map((p) => p.kind), [PinKind.cutoff]);
    expect(session.chips(t0).last, '위험 차단으로 끝남');
    expect(session.samples.last!.smoke, 2);
  });

  test('센서 정상 비율은 끊기지 않은 표본 중 열화상·미세먼지가 둘 다 valid인 비율', () {
    final log = HistoryLog(t0);
    feed(
        log,
        t0,
        [
          _s(),
          _s(thermalValid: false),
          null,
          _s(),
        ],
        repeat: 2);
    expect(log.samples, 6);
    expect(log.sensorOk, 4);
  });

  test('위험 잠금 횟수와 잠금부터 해제까지 걸린 시간', () {
    final log = HistoryLog(t0);
    feed(
        log,
        t0,
        [
          _s(),
          _s(hazard: 'danger', lock: 'locked_danger', reasons: ['SMOKE']),
          _s(hazard: 'low', lock: 'locked_danger', reasons: ['ACK_WAIT']),
          _s(),
          _s(lock: 'locked_cat', presence: 'absent', reasons: ['CAT_NEAR']),
        ],
        repeat: 6);
    expect(log.dangerLocks, 1);
    expect(log.dangerReleases, [const Duration(seconds: 6)]);
    expect(log.catLocks, 1);
  });

  test('연결 끊김 횟수와 복구 시간, uptime이 줄면 Core 재시작', () {
    final log = HistoryLog(t0);
    log.lost(t0);
    log.recovered(t0.add(const Duration(seconds: 4)));
    log.lost(t0.add(const Duration(seconds: 30)));
    log.recovered(t0.add(const Duration(seconds: 39)));
    expect(log.disconnects, 2);
    expect(log.recoveries,
        [const Duration(seconds: 4), const Duration(seconds: 9)]);
    log.state(_s(uptime: 500), _s(uptime: 3), t0);
    expect(log.coreRestarts, 1);
  });

  test('자리 비움 보호는 사람이 없거나 모를 때 hazard가 mid 이상인 구간', () {
    final log = HistoryLog(t0);
    feed(
        log,
        t0,
        [
          _s(presence: 'absent'),
          _s(presence: 'absent', hazard: 'warning', reasons: ['ABSENT']),
          _s(),
          _s(presence: 'unknown', hazard: 'mid'),
        ],
        repeat: 4);
    expect(log.absentGuards, 2);
    expect(log.absentGuardTime, const Duration(seconds: 4));
  });

  test('기기 응답 없음은 fan.ack·induction.ack가 false인 구간 횟수', () {
    final log = HistoryLog(t0);
    feed(log, t0, [
      _s(),
      _s(fanAck: false),
      _s(fanAck: false, indAck: false),
      _s(),
      _s(indAck: false),
    ]);
    expect(log.noAcks, 2);
  });
}
