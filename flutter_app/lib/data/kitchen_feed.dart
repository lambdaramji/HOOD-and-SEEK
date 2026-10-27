import 'dart:async';

import 'core_state.dart';
import 'ui_command.dart';

abstract interface class KitchenFeed {
  /// SafetyCore가 확정한 state. 검증에 실패한 메시지는 흘려보내지 않는다.
  Stream<CoreState> get states;

  Future<void> connect();

  /// ui_cmd 1건 전송. 집행 여부는 SafetyCore가 정하고 결과는 다음 state로 돌아온다.
  Future<void> send(UiCommand command);

  Future<void> close();
}

/// SafetyCore 없이 화면을 돌려 보는 피드 (web 데모·위젯 테스트).
///
/// SafetyCore처럼 마지막 state를 [period]마다 다시 보낸다. [play]에 다음 state들(`then`)을
/// 주면 [period]마다 하나씩 넘긴다 — 잠금 해제 남은 초처럼 Core가 2 Hz로 바꿔 보내는 값을
/// 손으로 적어 둔 것이다. [silence]를 부르면 송신을 멈춰 3 s 뒤 "연결 끊김" 화면을 재현한다.
// ponytail: 명령은 [sent]에 쌓기만 하고 state에 반영하지 않는다. 반영은 SafetyCore의
// 판정이라 데모 피드가 흉내 내지 않는다. 데모에서 반응이 필요해지면 시나리오를 추가한다.
class DemoKitchenFeed implements KitchenFeed {
  DemoKitchenFeed({CoreState? initial})
      : _state = initial ?? demoScenarios.first.state;

  static const period = Duration(milliseconds: 500);

  final StreamController<CoreState> _controller = StreamController.broadcast();
  final List<UiCommand> sent = [];
  CoreState? _state;
  final List<CoreState> _next = [];
  Timer? _ticker;

  @override
  Stream<CoreState> get states => _controller.stream;

  @override
  Future<void> connect() async {
    _emit();
    _ticker ??= Timer.periodic(period, (_) {
      if (_next.isNotEmpty) _state = _next.removeAt(0);
      _emit();
    });
  }

  void play(CoreState state, {Iterable<CoreState> then = const []}) {
    _state = state;
    _next
      ..clear()
      ..addAll(then);
    _emit();
  }

  void silence() {
    _state = null;
    _next.clear();
  }

  void _emit() {
    final state = _state;
    if (state != null && !_controller.isClosed) _controller.add(state);
  }

  @override
  Future<void> send(UiCommand command) async => sent.add(command);

  @override
  Future<void> close() async {
    _ticker?.cancel();
    await _controller.close();
  }
}

class DemoScenario {
  const DemoScenario(this.id, this.title, this.state, {this.then = const []});

  final String id;
  final String title;

  /// null이면 state 송신을 멈춘다 (연결 끊김 재현).
  final CoreState? state;

  /// [state] 다음에 0.5 s마다 하나씩 보낼 state (Core 송신 주기 2 Hz).
  final List<CoreState> then;
}

/// 리허설 칩용 state. SafetyCore가 보낼 법한 값을 손으로 적은 것이며 앱이 판정한
/// 결과가 아니다. 계약 파서를 그대로 통과시켜 형식을 보장한다.
final List<DemoScenario> demoScenarios = [
  DemoScenario('idle', '대기', _demo(metric: 0.4)),
  // D-054·D-059 판정 사다리: 약불 low · 중불 mid · 강불 high는 정상 조리라 재실이면
  // 인덕션을 그대로 두고 경고음도 없다. 차단(danger)은 65 °C급 고온 ∧ 과한 연기(화재 위험)뿐이다.
  // 재실이면 warning(과열·과한 연기)까지 켜기·올리기가 된다(`allowed_on`, policy.cpp 8단계, D-061).
  DemoScenario(
      'low', '약불 조리', _demo(fan: (2, 1, 2), induction: 2, metric: 14.6)),
  DemoScenario(
    'mid',
    '중불 조리',
    _demo(
      hazard: 'mid',
      fan: (2, 2, 0),
      induction: 2,
      metric: 20.6,
      reasons: ['TEMP_MID'],
    ),
  ),
  DemoScenario(
    'high',
    '강불 조리',
    _demo(
      hazard: 'high',
      fan: (3, 3, 0),
      induction: 3,
      metric: 23.6,
      reasons: ['TEMP_HIGH'],
      alert: 'NONE',
    ),
  ),
  DemoScenario(
    'pm-warn',
    '연기 조금',
    _demo(
      fan: (1, 1, 1),
      induction: 1,
      metric: 15.0,
      pm25: 52,
      tsize: 0.62,
      reasons: ['PM_WARN'],
    ),
  ),
  DemoScenario(
    'overheat',
    '과열 주의',
    _demo(
      hazard: 'warning',
      fan: (3, 3, 0),
      induction: 3,
      metric: 28.9,
      reasons: ['TEMP_DANGER'],
      alert: 'WARNING',
    ),
  ),
  DemoScenario(
    'heavy-smoke',
    '과한 연기',
    _demo(
      hazard: 'warning',
      fan: (3, 3, 0),
      induction: 1,
      metric: 15.0,
      pm25: 2000, // 진한 연기는 SPS30 상한에서 잘려 온다(E2E 10/26) → 「2000+」
      tsize: 0.42,
      reasons: ['SMOKE'],
      alert: 'WARNING',
    ),
  ),
  DemoScenario(
    'absent',
    '자리 비움',
    _demo(
      hazard: 'high',
      presence: 'absent',
      person: false,
      fan: (3, 3, 0),
      induction: 1,
      allowedOn: false,
      metric: 23.6,
      reasons: ['TEMP_HIGH', 'ABSENT'],
      alert: 'NONE',
    ),
  ),
  DemoScenario(
    'cat-lock',
    '고양이 잠금',
    _demo(
      presence: 'absent',
      lock: 'locked_cat',
      fan: (1, 0, 1),
      allowedOn: false,
      person: false,
      cat: true,
      reasons: ['ABSENT', 'CAT_NEAR'],
      alert: 'CAT_LOCK',
    ),
  ),
  DemoScenario(
    'user-lock',
    '사용자 잠금',
    _demo(lock: 'locked_user', fan: (1, 0, 1), allowedOn: false),
  ),
  DemoScenario(
    'fire',
    '화재 위험 차단',
    _demo(
      hazard: 'danger',
      lock: 'locked_danger',
      fan: (3, 3, 0),
      allowedOn: false,
      metric: 28.9,
      pm25: 2000, // 진한 연기는 SPS30 상한에서 잘려 온다(E2E 10/26) → 「2000+」
      tsize: 0.42,
      reasons: ['TEMP_DANGER', 'SMOKE'],
      alert: 'DANGER',
    ),
  ),
  DemoScenario(
    'locked-voice',
    '잠금 중 음성 거부',
    _demo(
      hazard: 'mid',
      lock: 'locked_danger',
      fan: (1, 1, 0),
      allowedOn: false,
      metric: 20.1,
      releaseLeft: 6,
      reasons: ['TEMP_MID', 'ACK_WAIT'],
      voice: {
        'last_intent': 'IND_ON',
        'value': null,
        'accepted': false,
        'reason': 'LOCKED'
      },
    ),
  ),
  DemoScenario(
    'ack-wait',
    '위험 해소 · 확인 대기',
    _demo(
      hazard: 'mid',
      lock: 'locked_danger',
      fan: (1, 1, 0),
      allowedOn: false,
      metric: 19.6,
      releaseLeft: 7,
      reasons: ['TEMP_MID', 'ACK_WAIT'],
    ),
  ),
  DemoScenario(
    'unlock-wait',
    '확인 뒤 자리 비움',
    _demo(
      hazard: 'mid',
      presence: 'absent',
      person: false,
      lock: 'locked_danger',
      fan: (1, 1, 0),
      allowedOn: false,
      metric: 19.4,
      releaseLeft: 0,
      reasons: ['TEMP_MID', 'ABSENT', 'UNLOCK_WAIT'],
    ),
  ),
  // M10: 위험 확인 뒤 안전 시간 10 s가 줄어 0이 되는 tick에 Core가 잠금을 푼다(불변식 ⑨).
  // 남은 초는 Core가 올림한 값이라 2 Hz에서 같은 숫자가 두 번씩 온다 (설계 v1 §4.5.1).
  DemoScenario(
    'release',
    '해제 카운트다운',
    _releasing(10),
    then: [
      for (var half = 19; half > 0; half--) _releasing((half + 1) ~/ 2),
      _demo(hazard: 'mid', fan: (1, 1, 0), metric: 19.4, reasons: ['TEMP_MID']),
    ],
  ),
  DemoScenario(
    'voice',
    '음성 실행',
    _demo(
      fan: (2, 1, 2),
      induction: 2,
      voice: {
        'last_intent': 'FAN_SET',
        'value': 2,
        'accepted': true,
        'reason': null
      },
    ),
  ),
  DemoScenario(
    'fault',
    '센서 고장',
    _demo(
      hazard: 'fault',
      lock: 'locked_danger',
      fan: (3, 3, 0),
      allowedOn: false,
      metric: null,
      reasons: ['SENSOR_FAULT'],
    ),
  ),
  const DemoScenario('offline', '연결 끊김', null),
];

/// M10 해제 대기: 위험 확인을 마쳤고(`UNLOCK_WAIT`) 사람도 있어 안전 시간만 남은 상태.
CoreState _releasing(int left) => _demo(
      hazard: 'mid',
      lock: 'locked_danger',
      fan: (1, 1, 0),
      allowedOn: false,
      metric: 19.4,
      releaseLeft: left,
      reasons: ['TEMP_MID', 'UNLOCK_WAIT'],
    );

CoreState _demo({
  String hazard = 'low',
  String presence = 'present',
  String lock = 'unlocked',
  (int, int, int) fan = (0, 0, 0),
  int induction = 0,
  bool allowedOn = true,
  double? metric = 8.2,
  double pm25 = 9.0,
  double tsize = 0.71,
  bool person = true,
  bool cat = false,
  Map<String, Object?>? voice,
  List<String> reasons = const [],
  String alert = 'NONE',
  int? releaseLeft,
}) {
  return CoreState.fromJson({
    'v': 3,
    'type': 'state',
    'src': 'core',
    'seq': 1,
    't_ms': 0,
    'p': {
      'hazard': hazard,
      'presence': presence,
      'lock': lock,
      'lock_release_left_s': releaseLeft,
      'fan': {'level': fan.$1, 'min': fan.$2, 'requested': fan.$3, 'ack': true},
      'induction': {'level': induction, 'allowed_on': allowedOn, 'ack': true},
      'sensors': {
        'thermal': {
          'valid': metric != null,
          'max_c': metric == null ? null : 28.0 + metric,
          'median_c': metric == null ? null : 28.0,
          'metric': metric,
        },
        'pm': {'valid': true, 'pm25': pm25, 'tsize': tsize, 'baseline': 9.4},
      },
      'vision': {'fresh': true, 'person': person, 'cat': cat},
      'voice': voice,
      'reasons': reasons,
      'alert': alert,
      'thresholds': {
        't_mid': 16.99,
        't_high': 21.86,
        't_danger': 27.07,
        'pm_warn': 30,
        'pm_smoke': 50,
        'lock_release_s': 10,
      },
      'uptime_s': 61,
    },
  });
}
