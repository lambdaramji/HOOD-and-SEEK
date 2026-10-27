import '../data/core_state.dart';
import '../widgets/hs.dart';
import 'labels.dart';

/// 조리 리듬 한 표본(0.5 s). 연결이 끊긴 동안은 null로 둔다.
class RhythmSample {
  const RhythmSample(this.tone, this.grade, this.fan, this.auto, this.smoke);

  /// 홈 게이지와 같은 [temperatureGrade] 색·문구. 조리 중 「정상 (약불)」은 「약불」로 읽는다 (PM 설계 10/26 A).
  final Tone tone;
  final String grade;
  final int fan;

  /// `fan.level > fan.requested` — Core가 안전 하한으로 올린 구간.
  final bool auto;

  /// 0 없음 · 1 연기 있음(`PM_WARN`) · 2 연기 많음(`SMOKE`).
  final int smoke;
}

enum PinKind { absent, voiceAccepted, voiceRejected, dangerLock, cutoff }

class RhythmPin {
  RhythmPin(this.kind, this.at);

  final PinKind kind;
  final DateTime at;

  /// 자리 비움 핀의 길이. 비운 동안 계속 늘어난다.
  Duration length = Duration.zero;
}

/// 조리 1회: `induction.level`이 0에서 1 이상이 된 순간부터 다시 0이 될 때까지 (PM 설계 10/26 A).
class CookSession {
  CookSession(this.start);

  final DateTime start;
  DateTime? end;
  final samples = <RhythmSample?>[];
  final pins = <RhythmPin>[];

  /// 「자동」 구간(`fan.level > fan.requested`)으로 들어간 횟수.
  int autoRaises = 0;

  /// 1시간(0.5 s × 7200)이 넘으면 앞쪽 표본을 버린다. 비율도 남은 표본으로 센다.
  static const maxSamples = 7200;

  bool get active => end == null;
  Duration length(DateTime now) => (end ?? now).difference(start);
  int get absences => pins.where((p) => p.kind == PinKind.absent).length;

  /// 단계별 비율(끊긴 표본 제외). 단계 순서대로.
  List<(String, Tone, double)> shares() {
    final counts = <String, (Tone, int)>{};
    var total = 0;
    for (final s in samples) {
      if (s == null) continue;
      total++;
      final (tone, n) = counts[s.grade] ?? (s.tone, 0);
      counts[s.grade] = (tone, n + 1);
    }
    if (total == 0) return const [];
    final order = [for (final g in gradeOrder) g, ...counts.keys];
    return [
      for (final g in order.toSet())
        if (counts[g] case (final tone, final n)) (g, tone, n / total),
    ];
  }

  static const gradeOrder = ['약불', '중불', '강불', '과열', '위험', '남은 열', '점검', '대기'];

  /// 요약 칩: 「조리 29분」「약불 40 %」「중불 35 %」「강불 25 %」「후드 자동 상향 2번」「자리 비움 1번」.
  /// 0번인 항목은 칩을 만들지 않는다(#156 재범 「0회로 채우지 않음」).
  List<String> chips(DateTime now) => [
        '조리 ${duration(length(now))}',
        for (final (grade, _, share) in shares())
          if ((share * 100).round() > 0) '$grade ${(share * 100).round()} %',
        if (autoRaises > 0) '후드 자동 상향 $autoRaises번',
        if (absences > 0) '자리 비움 $absences번',
        if (pins.any((p) => p.kind == PinKind.cutoff)) '위험 차단으로 끝남',
      ];

  /// 칩을 한 줄로(끝난 조리 목록).
  String summary(DateTime now) => chips(now).join(' · ');
}

/// 기록 탭(UI 설계 v1 §3.3)의 A·⑥. 앱을 켠 뒤 받은 state만 앱 메모리에 모은다 — SafetyCore 로그가
/// 아니고 앱을 다시 켜면 비워진다. 임계값을 비교하지 않고 Core가 보낸 단계·잠금·사유 코드·ack만 센다.
class HistoryLog {
  HistoryLog(this.since);

  /// 이 시각부터 모은 기록이다(화면에 「앱을 켠 뒤」로 밝힌다).
  final DateTime since;

  CookSession? current;

  /// 조리 1회가 끝날 때(기록 DB에 요약을 남기려고, 대시보드가 건다).
  void Function(CookSession session)? onFinished;

  /// 최근 끝난 조리. 앞이 최신이고 [keepSessions]개까지.
  final recent = <CookSession>[];
  static const keepSessions = 3;

  // ⑥ 지켜 준 기록
  int samples = 0;
  int sensorOk = 0;
  int disconnects = 0;
  final recoveries = <Duration>[];
  int coreRestarts = 0;
  int dangerLocks = 0;
  final dangerReleases = <Duration>[];
  int catLocks = 0;
  int absentGuards = 0;
  Duration absentGuardTime = Duration.zero;
  int noAcks = 0;
  int voiceAccepted = 0;
  int voiceRejected = 0;

  DateTime? _lostAt;
  DateTime? _dangerAt;
  bool _guarding = false;
  bool _noAck = false;
  bool _auto = false;
  RhythmPin? _absentPin;

  /// 그래프와 같은 0.5 s 표본(`DashboardScreen._sampleTrend`). [live]는 끊긴 동안 null.
  void sample(CoreState? live, DateTime now, Duration step) {
    final session = current;
    if (live == null) {
      session?.samples.add(null);
      _absentPin = null;
      return;
    }
    samples++;
    if (live.thermal.valid && live.pm.valid) sensorOk++;

    // 자리 비움 보호: 사람이 없거나 모를 때 hazard가 mid 이상인 구간 (Core가 후드·인덕션을 조절한 장면).
    final guarding = live.presence != Presence.present &&
        live.hazard != Hazard.low &&
        live.hazard != Hazard.fault;
    if (guarding && !_guarding) absentGuards++;
    if (guarding) absentGuardTime += step;
    _guarding = guarding;

    final noAck = !live.fan.ack || !live.induction.ack;
    if (noAck && !_noAck) noAcks++;
    _noAck = noAck;

    final cooking = live.induction.level > 0;
    if (session == null) {
      if (cooking) _start(now, live);
      return;
    }
    // 끝나는 순간의 0단 표본은 넣지 않는다 — 리듬·비율에 「대기」가 섞이지 않게.
    if (cooking) {
      _record(session, live, now, step);
    } else {
      _finish(session, live, now);
    }
  }

  void _start(DateTime now, CoreState live) {
    final session = current = CookSession(now);
    _auto = false;
    _absentPin = null;
    _record(session, live, now, Duration.zero);
  }

  void _record(
      CookSession session, CoreState live, DateTime now, Duration step) {
    final (tone, grade) = temperatureGrade(live);
    final auto = live.fan.level > live.fan.requested;
    if (auto && !_auto) session.autoRaises++;
    _auto = auto;
    session.samples.add(RhythmSample(
      tone,
      grade == '정상 (약불)' ? '약불' : grade,
      live.fan.level,
      auto,
      live.reasons.contains('SMOKE')
          ? 2
          : live.reasons.contains('PM_WARN')
              ? 1
              : 0,
    ));
    if (session.samples.length > CookSession.maxSamples) {
      session.samples.removeAt(0);
    }
    if (live.presence == Presence.absent) {
      var pin = _absentPin;
      if (pin == null) {
        pin = _absentPin = RhythmPin(PinKind.absent, now);
        session.pins.add(pin);
      }
      pin.length = now.difference(pin.at);
    } else {
      _absentPin = null;
    }
  }

  void _finish(CookSession session, CoreState live, DateTime now) {
    if (live.hazard == Hazard.danger || live.lock == Lock.lockedDanger) {
      // 위험 차단은 잠금과 인덕션 0단이 같은 순간에 온다. 2 s 안의 위험 잠금 핀은 「위험 차단」 하나로 합친다.
      session.pins.removeWhere((p) =>
          p.kind == PinKind.dangerLock &&
          now.difference(p.at) <= const Duration(seconds: 2));
      session.pins.add(RhythmPin(PinKind.cutoff, now));
    }
    session.end = now;
    current = null;
    recent.insert(0, session);
    if (recent.length > keepSessions) recent.removeLast();
    onFinished?.call(session);
  }

  /// state가 올 때마다(`DashboardScreen._onState`). 잠금 변화와 Core 재시작을 센다.
  void state(CoreState? prev, CoreState next, DateTime now) {
    if (prev != null && next.uptimeS < prev.uptimeS) coreRestarts++;
    final was = prev?.lock ?? Lock.unlocked;
    if (was != next.lock) {
      if (next.lock == Lock.lockedDanger) {
        dangerLocks++;
        _dangerAt = now;
        current?.pins.add(RhythmPin(PinKind.dangerLock, now));
      }
      if (next.lock == Lock.lockedCat) catLocks++;
      if (was == Lock.lockedDanger && _dangerAt != null) {
        dangerReleases.add(now.difference(_dangerAt!));
        _dangerAt = null;
      }
    }
  }

  /// 앱의 「연결 끊김」(state 3 s 무수신).
  void lost(DateTime now) {
    disconnects++;
    _lostAt = now;
  }

  /// 끊긴 뒤 다시 state를 받았을 때.
  void recovered(DateTime now) {
    if (_lostAt case final at?) recoveries.add(now.difference(at));
    _lostAt = null;
  }

  /// 새 음성 결과. 계약에 순번이 없어 같은 결과가 연달아 오면 하나로 센다.
  void voice(VoiceResult v, DateTime now) {
    if (v.accepted) {
      voiceAccepted++;
    } else {
      voiceRejected++;
    }
    current?.pins.add(RhythmPin(
        v.accepted ? PinKind.voiceAccepted : PinKind.voiceRejected, now));
  }

  /// 보여 줄 조리: 진행 중이면 그것, 아니면 최근 끝난 것.
  CookSession? get shown => current ?? recent.firstOrNull;
}
