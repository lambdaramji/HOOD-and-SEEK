/// SafetyCore state → 화면 문구·색. 전부 state의 enum·`reasons` 코드·값을 옮기는
/// 매핑이고, 임계값 비교·잠금 해제 같은 판단은 하지 않는다 (설계서 §5.4).
///
/// 화면은 실제 LG 후드·인덕션이 연결된 앱처럼 「인덕션」·「가열 중」으로 쓴다
/// (D-047, 불변식 ⑧ 개정). 쿡탑이 시뮬레이터임은 보고서·문서에서 밝힌다.
library;

import 'package:flutter/material.dart';

import '../data/core_state.dart';
import '../widgets/hs.dart';

/// 상단 알약 + 안전 헤드라인 카드 한 벌.
class Headline {
  const Headline({
    required this.tone,
    required this.pill,
    required this.icon,
    required this.title,
    required this.body,
  });

  final Tone tone;
  final String pill;
  final IconData icon;
  final String title;
  final String body;
}

/// 끊긴 동안의 배지·헤드라인 (설계 v1 §4.1·§4.2 stale 행).
const offlineHeadline = Headline(
  tone: Tone.idle,
  pill: '연결 끊김 · 다시 연결 중',
  icon: Icons.link_off_rounded,
  title: '안전 장치와 연결이 끊겼어요',
  body: '후드는 안전 모드로 계속 돌아가요. 인덕션은 켤 수 없어요.',
);

/// 배지는 설계 v1 §4.1 표의 우선순위, 헤드라인은 §4.2 표를 따른다.
/// [recovered]: 방금 위험 잠금이 풀렸는지 (화면이 lock 전이를 보고 잠시 켜 둔다).
Headline headline(CoreState s, {bool recovered = false}) {
  final r = s.reasons;
  final fan = s.fan.level;
  final ind = s.induction.level;
  final hoodTo = fan == 3 ? '후드를 최대로' : '후드를 $fan단으로';
  final smoke = r.contains('SMOKE');
  final hot = r.contains('TEMP_DANGER') || r.contains('TEMP_ABS');
  // 화재위험 (D-054): 65 °C급 고온 ∧ 과한 연기. 이미 불이 난 상황이 아니라 그 직전에 미리 끈 것이다(P-011 §5).
  final fire = smoke && r.contains('TEMP_DANGER');

  // fault는 열화상·SPS30 타임아웃(`SENSOR_FAULT`)과, profile을 못 읽어 Core가 판정을 시작하지
  // 못했을 때(fail-safe 출력, reasons 빈 배열 — `safety_core/src/main.cpp`)에 나온다. 둘 다 인덕션을 끈다.
  if (s.hazard == Hazard.fault) {
    final which = !r.contains('SENSOR_FAULT')
        ? '안전 장치'
        : !s.thermal.valid && !s.pm.valid
            ? '센서'
            : !s.thermal.valid
                ? '온도 센서'
                : !s.pm.valid
                    ? '공기 센서'
                    : '센서';
    return Headline(
      tone: Tone.warning,
      pill: '점검 필요 · $which',
      icon: Icons.build_outlined,
      title: which == '안전 장치' ? '안전 장치가 아직 준비되지 않았어요' : '$which가 응답하지 않아요',
      body: '안전을 위해 $hoodTo 올리고 인덕션을 껐어요.',
    );
  }
  final locked = s.lock == Lock.lockedDanger || s.hazard == Hazard.danger;
  if (locked) {
    final pill = r.contains('ACK_WAIT')
        ? '위험 해소 · 확인 필요'
        : r.contains('UNLOCK_WAIT')
            ? '확인됨 · 해제 대기'
            : fire
                ? '위험 · 화재 위험'
                : smoke
                    ? '위험 · 연기 감지'
                    : hot
                        ? '위험 · 고온'
                        : '위험 · 잠금 중';
    if (r.contains('ACK_WAIT')) {
      return Headline(
        tone: Tone.danger,
        pill: pill,
        icon: Icons.verified_user_outlined,
        title: '위험이 해소됐어요',
        body: '확인을 누르고 조리대 앞에 계시면 잠금이 풀려요.',
      );
    }
    if (r.contains('UNLOCK_WAIT')) {
      return Headline(
        tone: Tone.danger,
        pill: pill,
        icon: Icons.hourglass_top_rounded,
        title: '확인했어요 · 해제 대기 중',
        body: '${_away(s) ? '돌아오시면' : '잠시 뒤'} 잠금이 풀려요.',
      );
    }
    if (fire) {
      return Headline(
        tone: Tone.danger,
        pill: pill,
        icon: Icons.local_fire_department_outlined,
        title: '화재 위험을 미리 감지해 인덕션을 껐어요',
        body: '후드가 최대로 환기하고 있어요. 뜨거운 조리 기구는 그대로 두세요.',
      );
    }
    if (smoke || hot) {
      return Headline(
        tone: Tone.danger,
        pill: pill,
        icon:
            smoke ? Icons.cloud_outlined : Icons.local_fire_department_outlined,
        title: smoke ? '연기가 감지되었어요' : '온도가 너무 높아요',
        body: '인덕션을 ${ind == 0 ? '끄고' : '$ind단으로 낮추고'} $hoodTo 올렸어요.',
      );
    }
    return Headline(
      tone: Tone.danger,
      pill: pill,
      icon: Icons.lock_outline_rounded,
      title: '위험이 가라앉을 때까지 잠겨 있어요',
      body: '환기가 끝나고 확인을 누르면 쓸 수 있어요.',
    );
  }
  if (s.lock == Lock.lockedCat) {
    // 카메라 판정이 끊기면 Core는 CAT_NEAR를 빼고 잠금만 남긴다. 그때 「있어요」라고 하지 않는다.
    return catSeen(s)
        ? const Headline(
            tone: Tone.pet,
            pill: '경고 · 반려동물 감지',
            icon: Icons.pets_rounded,
            title: '반려동물이 조리 공간에 있어요',
            body: '안전을 위해 인덕션을 잠갔어요.',
          )
        : Headline(
            tone: Tone.pet,
            pill: '경고 · 반려동물 잠금 중',
            icon: Icons.pets_rounded,
            title: '반려동물 잠금 중이에요',
            body: '조리대 앞에 사람이 오면 잠금이 풀려요.',
          );
  }
  // D-054·D-059: 재실이면 mid(중불)·high(강불)·warning(과열·과한 연기)에서 인덕션을 건드리지 않는다.
  // 부재(카메라 판정 없음 포함) ∧ mid 이상이면 Core가 인덕션을 1단까지로 내리고 후드를 최대로 올린다.
  // 강불 이상에서 자리를 비운 것은 실제로 보호가 걸린 장면이라 경고 톤, 중불이면 주의 톤.
  if (s.hazard == Hazard.mid ||
      s.hazard == Hazard.high ||
      s.hazard == Hazard.warning) {
    if (s.presence != Presence.present) {
      final strong = s.hazard != Hazard.mid;
      final why = r.contains('TEMP_DANGER')
          ? '온도가 매우 높아'
          : smoke
              ? '연기가 많아'
              : r.contains('TEMP_HIGH') || r.contains('TEMP_MID')
                  ? (ind > 0 ? '불이 켜져 있어' : '남은 열이 있어')
                  : '안전을 위해';
      return Headline(
        tone: strong ? Tone.warning : Tone.caution,
        pill: r.contains('ABSENT')
            ? '${strong ? '경고' : '주의'} · 자리 비움'
            : '${strong ? '경고' : '주의'} · 카메라 정보 없음',
        icon: Icons.person_off_outlined,
        title: r.contains('ABSENT') ? '자리를 비우셨어요' : '조리대 앞에 계신지 확인할 수 없어요',
        body: ind > 0
            ? '$why $hoodTo 올리고 인덕션은 $ind단까지만 쓰게 했어요.'
            : '$why $hoodTo 올렸어요.',
      );
    }
  }
  // 잠금이 풀린 직후처럼 인덕션이 꺼졌는데 남은 열로 mid·high면
  // 중불·강불로 조리 중이라고 하지 않는다 (10/26 Must, 정인). 정상 조리 열이라 주의 톤 (D-059).
  // ESP32 무응답 표시가 먼저라 그때는 넘긴다.
  final residual = ind == 0 &&
      (s.hazard == Hazard.mid || s.hazard == Hazard.high) &&
      !r.contains('IND_NOACK') &&
      !r.contains('FAN_NOACK');
  if (residual) {
    return Headline(
      tone: Tone.caution,
      pill: '남은 열',
      icon: Icons.thermostat_rounded,
      title: '인덕션은 꺼졌어요',
      body: '조리 기구에 열이 남아 있어요. $hoodTo 켜 두었어요.',
    );
  }
  if (s.hazard == Hazard.warning) {
    if (r.contains('TEMP_DANGER')) {
      return Headline(
        tone: Tone.warning,
        pill: '경고 · 과열 주의',
        icon: Icons.local_fire_department_outlined,
        title: '조리 온도가 매우 높아요',
        body: '$hoodTo 올렸어요. 인덕션은 그대로예요.',
      );
    }
    if (smoke) {
      return Headline(
        tone: Tone.warning,
        pill: '경고 · 연기 많음',
        icon: Icons.cloud_outlined,
        title: '연기가 많아요',
        body: '$hoodTo 올렸어요. 인덕션은 그대로예요.',
      );
    }
    return Headline(
      tone: Tone.warning,
      pill: '경고 · 공기 질',
      icon: Icons.air_rounded,
      title: '공기가 탁해요',
      body: '$hoodTo 올렸어요.',
    );
  }
  // ESP32 무응답은 hazard를 바꾸지 않고 reasons 맨 뒤에 붙는다(IND_NOACK은 켜기만 막음).
  // 그래서 위험·잠금·경고 표시를 덮지 않는다.
  if (r.contains('IND_NOACK') || r.contains('FAN_NOACK')) {
    final indNoAck = r.contains('IND_NOACK');
    return Headline(
      tone: Tone.warning,
      pill: indNoAck ? '경고 · 인덕션 응답 없음' : '경고 · 후드 응답 없음',
      icon: Icons.link_off_rounded,
      title: indNoAck ? '인덕션이 응답하지 않아요' : '후드가 응답하지 않아요',
      body: indNoAck ? '연결을 확인해 주세요. 그동안 인덕션을 켤 수 없어요.' : '후드 연결을 확인해 주세요.',
    );
  }
  // 강불(high)은 정상 조리라 주황이 아니라 중불과 같은 주의 톤이다 (PM 10/26, D-059).
  if (s.hazard == Hazard.high) {
    return Headline(
      tone: Tone.caution,
      pill: '강불 조리 중',
      icon: Icons.thermostat_rounded,
      title: '강불로 조리 중이에요',
      body: '$hoodTo 올렸어요. 인덕션은 그대로예요.',
    );
  }
  if (s.hazard == Hazard.mid) {
    return Headline(
      tone: Tone.caution,
      pill: '중불 조리 중',
      icon: Icons.thermostat_rounded,
      title: '중불로 조리 중이에요',
      body: '$hoodTo 켜 두었어요.',
    );
  }
  // 사용자 잠금(D-058): 위험·반려동물 잠금, 자리 비움, 온도·연기 표시가 먼저다. 그 밖일 때만 이 헤드라인.
  if (s.lock == Lock.lockedUser) {
    return const Headline(
      tone: Tone.idle,
      pill: '사용자 잠금',
      icon: Icons.lock_outline_rounded,
      title: '인덕션을 잠가 두었어요',
      body: '잠금을 풀기 전에는 켤 수 없어요. 후드는 그대로 쓸 수 있어요.',
    );
  }
  // 「모든 센서 작동 중」은 실제로 그럴 때만. 아니면 빠진 것을 적는다.
  final sensors = !s.vision.fresh
      ? '카메라 정보 없음'
      : s.thermal.valid && s.pm.valid
          ? '모든 센서 작동 중'
          : '센서 확인 중';
  if (recovered) {
    return const Headline(
      tone: Tone.normal,
      pill: '정상 · 잠금 풀림',
      icon: Icons.lock_open_rounded,
      title: '다시 안전하게 쓸 수 있어요',
      body: '잠금이 풀렸어요. 인덕션을 켜면 다시 지켜볼게요.',
    );
  }
  if (ind == 0 && fan == 0) {
    return Headline(
      tone: Tone.normal,
      pill: '대기 · $sensors',
      icon: Icons.shield_outlined,
      // 인덕션·센서 상태는 옆 카드와 배지에 이미 있어, 지금 할 수 있는 것을 재실로 나눠 말한다 (정인·병직 10/26).
      // 켜기 가능 여부는 Core의 `allowed_on` 그대로 — 사람이 있어도 막혀 있으면 「바로」라고 하지 않는다.
      title: switch (s.presence) {
        Presence.present when s.induction.allowedOn => '조리를 시작해 볼까요?',
        Presence.present => '곧 조리를 시작할 수 있어요',
        Presence.absent => '빈 주방을 안전하게 지키고 있어요',
        Presence.unknown => '조리대 앞에 서면 시작할 수 있어요',
      },
      body: switch (s.presence) {
        Presence.present when s.induction.allowedOn =>
          '인덕션과 음성 명령을 바로 쓸 수 있어요.',
        Presence.present => '안전 장치가 확인을 마치면 인덕션을 켤 수 있어요.',
        Presence.absent => '자리가 비어 있는 동안에는 반려동물이나 실수로 인덕션이 켜지지 않아요.',
        Presence.unknown => '안전을 위해 사람이 확인될 때만 인덕션 켜기를 허용해요.',
      },
    );
  }
  return Headline(
    tone: Tone.normal,
    pill: '정상 · $sensors',
    icon: Icons.gpp_good_outlined,
    title: '안전하게 조리 중입니다',
    // PM_WARN은 「연기 있음」 표시일 뿐 hazard를 바꾸지 않는다 (D-054).
    body: r.contains('PM_WARN') ? '온도는 정상이고 연기가 조금 있어요.' : '온도와 공기 질이 정상 범위예요.',
  );
}

/// 온도차 게이지 색·문구. 값이 아니라 SafetyCore의 TEMP_* 사유 코드로 정한다.
(Tone, String) temperatureGrade(CoreState s) {
  final r = s.reasons;
  if (!s.thermal.valid) return (Tone.warning, '점검');
  if (r.contains('TEMP_ABS')) return (Tone.danger, '위험');
  // 65 °C급 고온은 과한 연기와 함께일 때만 위험(차단)이고, 혼자면 경고다 (D-054).
  if (r.contains('TEMP_DANGER')) {
    return s.hazard == Hazard.danger
        ? (Tone.danger, '위험')
        : (Tone.warning, '과열');
  }
  final off = s.induction.level == 0;
  if (r.contains('TEMP_HIGH')) return (Tone.caution, off ? '남은 열' : '강불');
  if (r.contains('TEMP_MID')) return (Tone.caution, off ? '남은 열' : '중불');
  if (s.induction.level == 0 && s.hazard == Hazard.low) {
    return (Tone.normal, '대기');
  }
  // 조리 중 low는 약불이다. 중불·강불 칸과 나란히 읽히게 단계를 붙인다 (정인 10/26).
  return (Tone.normal, s.induction.level > 0 ? '정상 (약불)' : '정상');
}

/// 공기 질 게이지 색·문구. SMOKE·PM_WARN 사유 코드로 정한다.
/// SMOKE는 과한 연기(위험일 때만 빨강), PM_WARN은 hazard를 안 바꾸는 「연기 있음」 표시다 (D-054).
/// PM_WARN도 수치가 높으면 연기가 보이므로 온도 중불과 같은 주의 톤으로 둔다 (10/26 Must, 정인).
(Tone, String) pmGrade(CoreState s) {
  final r = s.reasons;
  if (!s.pm.valid) return (Tone.warning, '점검');
  if (r.contains('SMOKE')) {
    return (s.hazard == Hazard.danger ? Tone.danger : Tone.warning, '연기 많음');
  }
  if (r.contains('PM_WARN')) return (Tone.caution, '연기 있음');
  return (Tone.normal, '좋음');
}

/// 게이지 아래 「다음 기준」. 지금 걸린 사유의 다음 단계 임계값을 표시용 사본에서 읽는다.
String temperatureFooter(CoreState s) {
  final t = s.thresholds;
  final r = s.reasons;
  if (r.contains('TEMP_ABS') ||
      r.contains('TEMP_DANGER') ||
      r.contains('TEMP_HIGH')) {
    // t_danger는 65 °C급 경계다. 단독이면 warning이라 「위험」이라고 하지 않는다 (D-054, UI 설계 v1 §4.6).
    return '과열 기준 ${threshold(t.tDanger)} °C';
  }
  if (r.contains('TEMP_MID')) return '강불 기준 ${threshold(t.tHigh)} °C';
  return '중불 기준 ${threshold(t.tMid)} °C';
}

/// PM 선은 계약대로 `baseline + pm_warn` / `baseline + pm_smoke`.
String pmFooter(CoreState s) {
  final baseline = s.pm.baseline;
  if (baseline == null) return '기준값 측정 중';
  final t = s.thresholds;
  final r = s.reasons;
  // 「연기 많음」은 팬 최대에서 센서 최대치가 이어질 때 걸리고, baseline + pm_smoke는
  // 그게 풀리는 선이다(`policy.cpp` P_PM_smoke). 걸리는 선처럼 보이지 않게 쓴다.
  if (r.contains('SMOKE')) {
    return '해제 기준 ${threshold(baseline + t.pmSmoke)} µg/m³';
  }
  if (r.contains('PM_WARN')) return '센서 최대치가 이어지면 연기 많음';
  return '경고 기준 ${threshold(baseline + t.pmWarn)} µg/m³';
}

/// 온도 게이지 채움: 카드 문구 단계대로 끊는다 — 대기 0 · 약불 1/3 · 중불 2/3 · 강불·과열·위험 꽉
/// (정인 10/26, 온도차 비율로는 세 단계 차이가 잘 안 보였음). 남은 열은 그 열의 단계를 따른다.
double? temperatureFill(CoreState s) {
  if (!s.thermal.valid) return null;
  final r = s.reasons;
  if (r.contains('TEMP_ABS') ||
      r.contains('TEMP_DANGER') ||
      r.contains('TEMP_HIGH')) {
    return 1;
  }
  if (r.contains('TEMP_MID')) return 2 / 3;
  // 온도 사유 없이 인덕션이 꺼져 있으면 문구가 「대기」·「정상」이라 비워 둔다(꺼진 채 과한 연기 등, #192 PM 리뷰).
  return s.induction.level == 0 ? 0 : 1 / 3;
}

/// PM 게이지 채움(표시용 눈금): 연기선까지를 한 바퀴로 본다.

double? pmFill(CoreState s) => switch (s.pm.pm25) {
      final v? => v / ((s.pm.baseline ?? 0) + s.thresholds.pmSmoke),
      null => null,
    };

String levelText(int level) => level == 0 ? '꺼짐' : '$level단';

String inductionText(InductionState induction) =>
    '인덕션 ${levelText(induction.level)}';

String hazardLabel(Hazard hazard) => switch (hazard) {
      Hazard.low => '정상',
      Hazard.mid => '중불',
      Hazard.high => '강불',
      Hazard.warning => '경고',
      Hazard.danger => '위험',
      Hazard.fault => '고장',
    };

/// 정상 조리 low·mid·high는 초록·노랑·노랑, 주의가 필요한 warning부터 주황·빨강 (D-059).
Tone hazardTone(Hazard hazard) => switch (hazard) {
      Hazard.low => Tone.normal,
      Hazard.mid || Hazard.high => Tone.caution,
      Hazard.warning || Hazard.fault => Tone.warning,
      Hazard.danger => Tone.danger,
    };

String lockLabel(Lock lock) => switch (lock) {
      Lock.unlocked => '잠금 풀림',
      Lock.lockedCat => '반려동물 잠금',
      Lock.lockedDanger => '위험 잠금',
      Lock.lockedUser => '사용자 잠금',
    };

String presenceLabel(Presence presence) => switch (presence) {
      Presence.present => '사람 감지',
      Presence.absent => '자리 비움',
      Presence.unknown => '사람 확인 불가',
    };

/// `state.p.voice.last_intent` → 문구. 모르는 값은 원문 그대로.
// 후드 명령은 시연 대본(「하이엘지 후드 강풍」)·후드 카드와 같은 말로 「후드」 (설계 v1 §7 #9, PM 10/25).
String intentLabel(VoiceResult voice) => switch (voice.lastIntent) {
      'FAN_SET' => switch (voice.value) {
          final n? => '후드 ${const ['꺼짐', '약풍', '중풍', '강풍'][n]}',
          null => '후드 단수 설정',
        },
      'FAN_OFF' => '후드 끄기',
      'IND_OFF' => '인덕션 끄기',
      'IND_ON' => '인덕션 켜기',
      'IND_SET' => '인덕션 ${voice.value}단 설정',
      'IND_UP' => '화력 올리기',
      'IND_DOWN' => '화력 내리기',
      'NONE' => '알아듣지 못한 문장',
      final other => other,
    };

/// 음성 명령 대상 기기의 지금 단수. Core가 요청을 받았다는 것과 실제 단수는 따로 보인다
/// (설계 v1 §4.4·§6 C2 — 안전 하한 때문에 요청과 다를 수 있음).
String voiceDeviceNow(VoiceResult voice, CoreState s) =>
    voice.lastIntent.startsWith('FAN')
        ? '지금 후드 ${levelText(s.fan.level)}'
        : '지금 ${inductionText(s.induction)}';

/// 화면에 보이는 음성 예시. 호출어와 명령을 이어서 한 번에 말하는 지원 명령만 쓴다
/// (설계 v1 §4.4·§6, #107 한 문장 발화).
const voiceExamples = ['하이 LG, 후드 약풍', '하이 LG, 인덕션 꺼 줘'];

/// `state.p.voice.reason` → 문구 (`contracts/README.md` 코드표의 제안 문구).
/// 모르는 코드는 원문 그대로.
String voiceReasonLabel(String? code) => switch (code) {
      null => '사유 없음',
      'LOCKED' => '잠금 중이라 거부됨',
      'HAZARD' => '지금 단계에서는 올릴 수 없음',
      'ABSENT' => '자리 비움',
      'IND_NOACK' => '인덕션 응답 없음',
      'NO_INTENT' => '지원하지 않는 명령',
      final other => other,
    };

/// 거부된 음성 명령의 큰 제목.
String voiceRejectTitle(VoiceResult voice) => switch (voice.reason) {
      'LOCKED' => '지금은 잠겨 있어요',
      // 강불 이상(강불·과열·과한 연기·위험)이거나 센서 확인 전 — 강불도 포함이라 「위험」이라고 하지 않는다 (D-059).
      'HAZARD' => '지금은 더 올릴 수 없어요',
      'ABSENT' => '자리에 사람이 없어 할 수 없어요',
      'IND_NOACK' => '인덕션이 응답하지 않아요',
      'NO_INTENT' => '지원하지 않는 명령이에요',
      _ => '지금은 할 수 없어요',
    };

/// `reasons` 코드 → 문구 (`contracts/README.md` 코드표의 제안 문구, #63에서 확정).
/// 모르는 코드는 null — 부르는 쪽이 원문 그대로 보여 준다.
String? reasonLabel(String code) => switch (code) {
      'SENSOR_FAULT' => '센서 고장',
      'TEMP_ABS' => '절대 고온',
      'TEMP_DANGER' => '과열',
      'SMOKE' => '연기 많음',
      'TEMP_HIGH' => '강불',
      'PM_WARN' => '연기 있음',
      'TEMP_MID' => '중불',
      'HOLD' => '해제 대기 중',
      'ABSENT' => '자리 비움',
      'VISION_STALE' => '카메라 정보 없음',
      'CAT_NEAR' => '반려동물 접근',
      'ACK_WAIT' => '위험 해소 · 확인 필요',
      'UNLOCK_WAIT' => '확인됨 · 해제 대기',
      'FAN_NOACK' => '후드 응답 없음',
      'IND_NOACK' => '인덕션 응답 없음',
      _ => null,
    };

/// 온도차는 소수 한 자리(「8.2」), PM은 정수(`digits: 0`, 「12」). 값이 없으면 「—」.
String number(double? value, {int digits = 1}) =>
    value == null ? '—' : value.toStringAsFixed(digits);

/// SPS30 PM2.5 측정 상한. Core가 넘는 값을 이 값으로 잘라 보낸다(`sensors.h` kSpsPm25Max).
const pmSensorMax = 2000.0;

/// PM2.5 정수 표시. 상한에 닿으면 실제 값은 더 높을 수 있어 「2000+」로 쓴다.
String pmNumber(double? value) => value != null && value >= pmSensorMax
    ? '${number(value, digits: 0)}+'
    : number(value, digits: 0);

// 임계값은 profile 값 그대로(22.79 → 「22.79」). 한 자리로 반올림하면 경계를 잘못 읽는다.
String threshold(double value) =>
    value.toStringAsFixed(2).replaceFirst(RegExp(r'\.?0+$'), '');

/// 「오후 7:42」.
String clock(DateTime t) {
  final h = t.hour % 12 == 0 ? 12 : t.hour % 12;
  return '${t.hour < 12 ? '오전' : '오후'} $h:${_two(t.minute)}';
}

/// 「19:42:07」.
String clockSeconds(DateTime t) =>
    '${_two(t.hour)}:${_two(t.minute)}:${_two(t.second)}';

/// 「12초」 「4분 12초」.
String duration(Duration d) {
  final s = d.inSeconds;
  return s < 60 ? '$s초' : '${s ~/ 60}분 ${s % 60}초';
}

/// 「방금 전」 「2분 전」.
String ago(Duration d) => d.inSeconds < 60
    ? '방금 전'
    : d.inMinutes < 60
        ? '${d.inMinutes}분 전'
        : '${d.inHours}시간 전';

String _two(int n) => n.toString().padLeft(2, '0');

/// 해제 대기 설명의 「돌아오시면」 조건: 자리 비움 또는 카메라 판정 없음.
bool _away(CoreState s) =>
    s.reasons.contains('ABSENT') || s.reasons.contains('VISION_STALE');

/// 지금 반려동물이 보이는지. `locked_cat`은 카메라가 끊겨도 남으므로 잠금만으로 말하지 않는다
/// (설계 v1 §4.2 `CAT_NEAR + locked_cat`).
bool catSeen(CoreState s) => s.reasons.contains('CAT_NEAR');

/// 초 표시: 10.0 → 「10」, 7.5 → 「7.5」.
String seconds(double v) =>
    v == v.roundToDouble() ? '${v.round()}' : v.toStringAsFixed(1);

/// 잠금 해제 조건 하나의 지금 상태 (설계 v1 §4.5).
enum Check { met, unmet, waiting }

class ReleaseCondition {
  const ReleaseCondition(this.name, this.check, this.text, this.short,
      {this.progress, this.extra});

  final String name;
  final Check check;

  /// 카드 설명 (해제 대기 대화상자).
  final String text;

  /// 한 줄 알약 (위험 대화상자).
  final String short;

  /// ① 안전 시간이 지난 비율 0..1. Core가 보낸 남은 초를 옮긴 것이고 진행 중일 때만 있다.
  final double? progress;

  /// 카드 설명 뒤에 옅게 붙는 말 (「· 10초 중」).
  final String? extra;
}

/// `locked_danger` 해제 3조건(불변식 ⑨)을 state 값 그대로 옮긴다. 앱은 조건을
/// 판정하지 않고 시간도 세지 않는다 — ①은 Core가 보낸 남은 초(`lock_release_left_s`,
/// state v2 #116)를 옮기고, 그 값이 없으면(state v1) 해제 순간 전까지 「지켜보는 중」이다.
/// Core는 hazard ≤ mid(중불)가 되는 즉시 `ACK_WAIT`/`UNLOCK_WAIT`를 붙이고,
/// hazard가 mid보다 높으면(강불 열이 남아 있어도) ACK를 버린다 (`policy.cpp` 7·11단계).
List<ReleaseCondition> releaseConditions(CoreState s) {
  final high = s.hazard.index > Hazard.mid.index;
  final r = s.reasons;
  final total = s.thresholds.lockReleaseS;
  return [
    if (high)
      const ReleaseCondition('안전한 상태 유지', Check.unmet, '위험 진행 중', '위험 진행 중')
    else
      switch (s.lockReleaseLeftS) {
        0 => const ReleaseCondition(
            '안전한 상태 유지', Check.met, '안정 시간을 채웠어요', '안정 시간 채움'),
        final left? => ReleaseCondition(
            '안전한 상태 유지', Check.waiting, '$left초 남음', '$left초 남음',
            progress: total == null || total <= 0
                ? null
                : ((total - left) / total).clamp(0.0, 1.0),
            extra: total == null ? null : '· ${seconds(total)}초 중'),
        null => const ReleaseCondition(
            '안전한 상태 유지', Check.waiting, '안정될 때까지 지켜보는 중', '안정되는 중'),
      },
    r.contains('UNLOCK_WAIT')
        ? const ReleaseCondition('위험 확인', Check.met, '확인했어요', '확인했어요')
        : r.contains('ACK_WAIT')
            ? const ReleaseCondition(
                '위험 확인', Check.unmet, '아래 버튼을 눌러 주세요', '위험 확인 필요')
            : const ReleaseCondition('위험 확인', Check.waiting,
                '위험이 내려간 뒤 누를 수 있어요', '위험 확인 — 위험이 내려간 뒤'),
    switch (s.presence) {
      Presence.present =>
        const ReleaseCondition('조리대 앞 사람', Check.met, '주방에 계세요', '주방에 계세요'),
      Presence.absent =>
        const ReleaseCondition('조리대 앞 사람', Check.unmet, '자리를 비우셨어요', '자리 비움'),
      Presence.unknown =>
        const ReleaseCondition('조리대 앞 사람', Check.unmet, '카메라 확인 중', '카메라 확인 중'),
    },
  ];
}

/// 음성 결과 안내 클립(`assets/audio/results/`, 조건표 README). 아직 확인되지 않았으면 null.
/// 후드는 `requested`가 목표이고 ACK가 있을 때만: 실제 단수가 목표면 그 단수,
/// 안전 하한(`fan.min`)으로 더 높으면 「안전을 위해 … 유지」. 인덕션은 ACK가 있고
/// 끄기면 0, 켜기·올리기·내리기면 시작 단수에서 바뀐 최종 단수.
String? voiceResultClip(VoiceResult v, int fromInduction, CoreState s) {
  if (!v.accepted) return null;
  switch (v.lastIntent) {
    case 'IND_SET':
      return v.value != null && s.induction.ack && s.induction.level == v.value
          ? 'results/RESULT_IND_${v.value}'
          : null;
    case 'FAN_SET' || 'FAN_OFF':
      final target = v.lastIntent == 'FAN_OFF' ? 0 : v.value;
      if (target == null || s.fan.requested != target || !s.fan.ack) {
        return null;
      }
      if (s.fan.level == target) {
        return target == 0
            ? 'results/RESULT_FAN_OFF'
            : 'results/RESULT_FAN_$target';
      }
      return s.fan.level > target && s.fan.level == s.fan.min
          ? 'results/RESULT_FAN_MIN_${s.fan.level}'
          : null;
    case 'IND_OFF' || 'IND_ON' || 'IND_UP' || 'IND_DOWN':
      final level = s.induction.level;
      if (!s.induction.ack) return null;
      final done =
          v.lastIntent == 'IND_OFF' ? level == 0 : level != fromInduction;
      if (!done) return null;
      return level == 0
          ? 'results/RESULT_IND_OFF'
          : 'results/RESULT_IND_$level';
  }
  return null;
}
