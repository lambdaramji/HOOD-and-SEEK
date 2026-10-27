/// 「기록」 탭 C. 후드 케어: 후드가 실제로 돈 양을 기기 안에서 세어 청소할 때를 알려 준다.
///
/// 안전 판정이 아니라 제품 편의 기능이다. 앱은 임계값을 비교하지 않고 SafetyCore가 보낸
/// `fan.level`·`fan.ack`·`reasons` 코드를 그대로 더하기만 한다. 누적은 기기 안에만 남고
/// ui_cmd는 보내지 않는다. IO가 없는 순수 Dart이며 저장은 `hood_care_store.dart`가 맡는다.
library;

import 'core_state.dart';

/// 청소 권장 기준(가중 가동, 단·초). 60 단·시간 = 3단으로 20시간 · 1단으로 60시간.
/// (제안) 근거가 없는 제품 설정 값이라 화면에 「(제안)」으로 보인다. 안전 판정 값이
/// 아니어서 `profiles/`에 두지 않는다. 확인 방법: 제조사 권장 청소 주기.
const cleanAfterLevelSeconds = 216000.0;

/// 저장에 실패했을 때 화면에 한 줄로 보이는 알림. 웹 빌드(저장 없음)도 같은 문구다.
const hoodCareSaveFailed = '저장하지 못해요 · 앱을 켠 뒤만 누적';

/// 표본 하나가 셀 수 있는 가장 긴 간격. Flutter는 state가 3 s 넘게 안 오면 「연결 끊김」이라
/// (불변식 ③) 그보다 긴 간격은 연결을 확인하지 못한 시간이다. 시계가 건너뛰어(NTP 동기화 등)
/// 간격이 몇 시간으로 잡혀도 그만큼 더하지 않는다.
const _maxSampleGap = Duration(seconds: 3);

/// 하루 기록 시계 확인 기간. 마지막으로 기록한 날부터 이 기간 안의 날만 하루 기록·예측에 넣는다.
/// Pi 5는 시계 배터리가 없어 전원을 끄면 1970년으로 돌아가고 시간 동기화도 안 된다
/// [실측 10/26 정인]. 특정 연도를 박지 않고 기록끼리 비교한다(PM 10/26, 1년 기간).
/// 누적(가중·연기 중)은 표본 간격으로 세서 날짜와 무관하므로 그대로 센다.
const clockWindow = Duration(days: 365);

const _keys = {'level_seconds', 'smoke_seconds', 'cleaned_at', 'daily'};
final _dayPattern = RegExp(r'^\d{4}-\d{2}-\d{2}$');

class HoodCare {
  HoodCare({
    this.levelSeconds = 0,
    this.smokeSeconds = 0,
    this.cleanedAt,
    Map<String, double>? daily,
  }) : daily = daily ?? {};

  /// 파일에서 읽은 값. 모르는 키·빠진 키·잘못된 타입·음수는 [FormatException]이다.
  factory HoodCare.fromJson(Map<String, Object?> json) {
    if (json.length != _keys.length || !json.keys.every(_keys.contains)) {
      throw FormatException('hood care: 키가 ${_keys.join('·')}가 아님', json);
    }
    final cleaned = json['cleaned_at'];
    if (cleaned is! String?) {
      throw FormatException('hood care: cleaned_at 문자열 아님', cleaned);
    }
    final daily = json['daily'];
    if (daily is! Map<String, Object?>) {
      throw FormatException('hood care: daily 객체 아님', daily);
    }
    return HoodCare(
      levelSeconds: _seconds('level_seconds', json['level_seconds']),
      smokeSeconds: _seconds('smoke_seconds', json['smoke_seconds']),
      // DateTime.parse도 틀린 문자열이면 FormatException을 던진다.
      cleanedAt: cleaned == null ? null : DateTime.parse(cleaned).toLocal(),
      daily: {
        for (final e in daily.entries)
          _dayKey(e.key): _seconds('daily.${e.key}', e.value),
      },
    );
  }

  /// 가중 가동 Σ(경과 초 × fan.level), 단·초.
  double levelSeconds;

  /// 연기 중 가동 시간(초). `PM_WARN`·`SMOKE` 사유가 있던 동안의 가동이다(단수 가중 없음).
  double smokeSeconds;

  /// 마지막으로 「청소했어요」를 누른 시각(기기 로컬). 없으면 null.
  DateTime? cleanedAt;

  /// 기기 로컬 날짜(`YYYY-MM-DD`)별 가중 가동, 단·초. 예측의 하루 평균에 쓴다.
  final Map<String, double> daily;

  /// 마지막 표본의 날짜가 [clockWindow] 밖이라 하루 기록을 건너뛰었는지. 화면 알림용이고 저장하지 않는다.
  bool clockSuspect = false;

  /// 0.5 s 표본마다 부른다. [live]가 null(연결 끊김)이거나 팬이 꺼졌거나 ESP32 응답(`fan.ack`)이
  /// 없으면 더하지 않는다. [elapsed]는 앞 표본부터의 시간이고 [now]의 날짜에 더한다.
  void sample(CoreState? live, Duration elapsed, DateTime now) {
    if (live == null || live.fan.level <= 0 || !live.fan.ack) return;
    if (elapsed <= Duration.zero) return;
    final gap = elapsed > _maxSampleGap ? _maxSampleGap : elapsed;
    final s = gap.inMicroseconds / Duration.microsecondsPerSecond;
    final weighted = s * live.fan.level;
    levelSeconds += weighted;
    if (live.reasons.contains('PM_WARN') || live.reasons.contains('SMOKE')) {
      smokeSeconds += s;
    }
    final day = _day(now);
    clockSuspect = !_dayInWindow(day);
    if (clockSuspect) return;
    daily.update(day, (v) => v + weighted, ifAbsent: () => weighted);
  }

  /// 마지막 기록일보다 앞서거나(시계가 뒤로 감) [clockWindow]보다 뒤면 시계를 믿지 않는다.
  /// 기록이 없으면(첫 사용·청소 직후) 받는다 — 시계를 잘못 맞춰 막혔으면 「청소했어요」가 기준을 지운다.
  bool _dayInWindow(String day) {
    if (daily.isEmpty) return true;
    final last = daily.keys.reduce((a, b) => a.compareTo(b) >= 0 ? a : b);
    if (day.compareTo(last) < 0) return false;
    return DateTime.parse(day).difference(DateTime.parse(last)) <= clockWindow;
  }

  /// 「청소했어요」: 누적(가중·연기 중·날짜별)을 0으로 되돌리고 청소 시각을 남긴다.
  void clean(DateTime now) {
    levelSeconds = 0;
    smokeSeconds = 0;
    daily.clear();
    clockSuspect = false;
    cleanedAt = now;
  }

  /// 청소 권장 기준 대비 비율. 1 이상이면 기준에 닿았다.
  double get progress => levelSeconds / cleanAfterLevelSeconds;

  /// 이 속도로 쓰면 기준까지 남은 날(올림). 기록된 날이 2일 미만이면 null,
  /// 이미 기준을 넘었으면 0이다. 하루 평균은 기록된 날(후드를 쓴 날)의 평균이다.
  int? daysLeft() {
    if (daily.length < 2) return null;
    final left = cleanAfterLevelSeconds - levelSeconds;
    if (left <= 0) return 0;
    final perDay = daily.values.fold(0.0, (a, b) => a + b) / daily.length;
    if (perDay <= 0) return null;
    return (left / perDay).ceil();
  }

  Map<String, Object?> toJson() => {
        'level_seconds': levelSeconds,
        'smoke_seconds': smokeSeconds,
        'cleaned_at': cleanedAt?.toUtc().toIso8601String(),
        'daily': Map.of(daily),
      };
}

String _day(DateTime t) {
  final l = t.toLocal();
  return '${l.year.toString().padLeft(4, '0')}-${_two(l.month)}-${_two(l.day)}';
}

String _two(int n) => n.toString().padLeft(2, '0');

String _dayKey(String key) {
  if (!_dayPattern.hasMatch(key)) {
    throw FormatException('hood care: daily 날짜 키 YYYY-MM-DD 아님', key);
  }
  return key;
}

double _seconds(String key, Object? v) {
  if (v is! num || !v.isFinite || v < 0) {
    throw FormatException('hood care: $key 0 이상 숫자 아님', v);
  }
  return v.toDouble();
}
