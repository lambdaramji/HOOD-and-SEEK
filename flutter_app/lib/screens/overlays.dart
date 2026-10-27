import 'package:flutter/material.dart';

import '../brand.dart';
import '../data/core_state.dart';
import '../widgets/hs.dart';
import 'labels.dart';

/// 화면 위에 뜨는 것들: 경고 대화상자 4종, 음성 결과, 기기 제어 시트, 토스트, 시작 화면.
/// 뜨는 조건은 모두 state 값(hazard·lock·reasons·voice)이고, 버튼은 ui_cmd 요청만 보낸다.

/// 화면 전체를 덮는 어두운 막. 누르면 [onTap].
class Dim extends StatelessWidget {
  const Dim({super.key, required this.alpha, this.onTap});

  final double alpha;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) => Positioned.fill(
        child: GestureDetector(
          onTap: onTap,
          child: TweenAnimationBuilder<double>(
            tween: Tween(begin: 0, end: alpha),
            duration: motion(context, const Duration(milliseconds: 250)),
            builder: (context, a, _) =>
                ColoredBox(color: const Color(0xFF1A1A1A).withValues(alpha: a)),
          ),
        ),
      );
}

/// 가운데 큰 대화상자 (1040×620): 위험(연기·고온). 1차 버튼만 LG 레드다 (설계 v1 §2.1).
class CenterAlert extends StatelessWidget {
  const CenterAlert({
    super.key,
    required this.tone,
    required this.icon,
    required this.title,
    required this.body,
    required this.chips,
    required this.primary,
    required this.onPrimary,
    required this.onDetail,
    this.conditions,
  });

  final Tone tone;
  final IconData icon;
  final String title;
  final String body;
  final List<Widget> chips;
  final String primary;
  final VoidCallback onPrimary;
  final VoidCallback onDetail;

  /// 위험 잠금 중이면 해제 조건 한 줄(설계 v1 §4.5)을 칩 아래에 둔다.
  final List<ReleaseCondition>? conditions;

  @override
  Widget build(BuildContext context) => Positioned(
        left: 440,
        top: conditions == null ? 230 : 200,
        width: 1040,
        height: conditions == null ? 620 : 680,
        child: Appear(
            child: Semantics(
          scopesRoute: true,
          explicitChildNodes: true,
          namesRoute: true,
          label: title,
          liveRegion: true,
          child: Container(
            padding: const EdgeInsets.fromLTRB(64, 56, 64, 48),
            decoration: BoxDecoration(
              color: Hs.card,
              borderRadius: BorderRadius.circular(32),
              boxShadow: Hs.modalShadow,
            ),
            child: Column(
              children: [
                Expanded(
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: Alignment.topCenter,
                    child: SizedBox(
                      width: 912,
                      child: Column(
                        children: [
                          _IconBadge(tone: tone, icon: icon, size: 120),
                          const SizedBox(height: 24),
                          Text(title,
                              textAlign: TextAlign.center,
                              style: hs(48,
                                  weight: FontWeight.w600,
                                  height: 1.2,
                                  spacing: -0.01)),
                          const SizedBox(height: 24),
                          Text(body,
                              textAlign: TextAlign.center,
                              style: hs(26, color: Hs.t2, height: 1.5)),
                          const SizedBox(height: 32),
                          Wrap(spacing: 12, children: chips),
                          if (conditions case final c?) ...[
                            const SizedBox(height: 24),
                            Semantics(
                              label: '잠금 해제 조건',
                              child: FittedBox(
                                fit: BoxFit.scaleDown,
                                child: Row(
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Text('잠금 해제 조건',
                                        style: hs(18, color: Hs.t2)),
                                    for (final condition in c) ...[
                                      const SizedBox(width: 10),
                                      ConditionPill(condition),
                                    ],
                                  ],
                                ),
                              ),
                            ),
                          ],
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                Row(
                  children: [
                    Expanded(
                        child: HsButton(
                            label: primary,
                            background: Hs.lgRed,
                            onPressed: onPrimary)),
                    const SizedBox(width: 16),
                    SizedBox(
                      width: 300,
                      child: HsButton(
                          label: '자세히 보기', primary: false, onPressed: onDetail),
                    ),
                  ],
                ),
              ],
            ),
          ),
        )),
      );
}

/// 위험 잠금 해제 대기 대화상자 (1040×660): 해제 조건 카드 3장 (설계 v1 §4.3·§4.5).
/// `ACK_WAIT`면 1차 버튼이 「위험 확인」(`ACK_ALERT`), `UNLOCK_WAIT`면 「닫기」다.
/// 어느 버튼도 잠금을 직접 풀지 않는다 (불변식 ⑨).
class ReleaseAlert extends StatelessWidget {
  const ReleaseAlert({
    super.key,
    required this.state,
    required this.onAck,
    required this.onClose,
    required this.onDetail,
  });

  final CoreState state;
  final VoidCallback onAck;
  final VoidCallback onClose;
  final VoidCallback onDetail;

  @override
  Widget build(BuildContext context) {
    // 「위험 확인」은 Core가 ACK를 기다릴 때(`ACK_WAIT`)만 낸다. 그 밖에는 「닫기」.
    final ackWait = state.reasons.contains('ACK_WAIT');
    final acked = state.reasons.contains('UNLOCK_WAIT');
    final away = state.presence != Presence.present;
    final conditions = releaseConditions(state);
    final left = conditions.where((c) => c.check != Check.met).length;
    final title = acked ? '확인했어요' : '위험 상황이 끝났어요';
    final body = left == 1
        ? '남은 조건은 하나예요. 채워지면 인덕션 잠금이 자동으로 풀려요.'
        : acked
            ? '${away ? '자리로 돌아오시면' : '잠시 뒤'} 잠금이 풀려요. 세 가지가 모두 충족돼야 해요.'
            : '아래 세 가지가 모두 충족되면 인덕션 잠금이 자동으로 풀려요.';
    return Positioned(
      left: 440,
      top: 210,
      width: 1040,
      height: 660,
      child: Appear(
        child: Semantics(
          scopesRoute: true,
          explicitChildNodes: true,
          namesRoute: true,
          label: title,
          liveRegion: true,
          child: Container(
            padding: const EdgeInsets.fromLTRB(56, 48, 56, 44),
            decoration: BoxDecoration(
              color: Hs.card,
              borderRadius: BorderRadius.circular(32),
              boxShadow: Hs.modalShadow,
            ),
            child: Column(
              children: [
                Expanded(
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: Alignment.topCenter,
                    child: SizedBox(
                      width: 928,
                      child: Column(
                        children: [
                          Container(
                            width: 96,
                            height: 96,
                            decoration: BoxDecoration(
                                color: Tone.normal.tint,
                                shape: BoxShape.circle),
                            child: const Icon(Icons.verified_user_outlined,
                                size: 48, color: Color(0xFF1F9E63)),
                          ),
                          const SizedBox(height: 16),
                          Text(title,
                              style: hs(44,
                                  weight: FontWeight.w600,
                                  height: 1.2,
                                  spacing: -0.01)),
                          const SizedBox(height: 16),
                          Text(body,
                              textAlign: TextAlign.center,
                              style: hs(24, color: Hs.t2, height: 1.5)),
                          const SizedBox(height: 32),
                          Semantics(
                            label: '잠금 해제 조건',
                            child: Row(
                              children: [
                                for (final (i, c) in conditions.indexed) ...[
                                  if (i > 0) const SizedBox(width: 16),
                                  Expanded(child: ConditionCard(c)),
                                ],
                              ],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                Row(
                  children: [
                    Expanded(
                      child: ackWait
                          ? HsButton(
                              label: '위험 확인',
                              background: Hs.lgRed,
                              onPressed: onAck)
                          : HsButton(
                              label: '닫기', primary: false, onPressed: onClose),
                    ),
                    const SizedBox(width: 16),
                    SizedBox(
                      width: 300,
                      child: HsButton(
                          label: '자세히 보기', primary: false, onPressed: onDetail),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

(Color, Color, Color, Widget) _checkStyle(Check check) => switch (check) {
      Check.met => (
          Tone.normal.tint,
          Tone.normal.tint,
          Tone.normal.ink,
          _CheckDot(Tone.normal.main,
              const Icon(Icons.check_rounded, size: 28, color: Colors.white)),
        ),
      Check.unmet => (
          Hs.card,
          Tone.danger.main,
          Tone.danger.ink,
          _CheckDot(
              Tone.danger.main,
              const Icon(Icons.priority_high_rounded,
                  size: 26, color: Colors.white)),
        ),
      Check.waiting => (
          Hs.bg,
          Hs.bg,
          Hs.t2,
          const _CheckDot(Hs.card, WaitSpinner()),
        ),
    };

class _CheckDot extends StatelessWidget {
  const _CheckDot(this.color, this.child, {this.size = 48});

  final Color color;
  final Widget child;
  final double size;

  @override
  Widget build(BuildContext context) => Container(
        width: size,
        height: size,
        alignment: Alignment.center,
        decoration: BoxDecoration(color: color, shape: BoxShape.circle),
        child: child,
      );
}

/// ① 안전 시간 진행 원 (시안 `hs-count`, 지름 38 · 선 5). [value]는 Core가 보낸 남은 초에서
/// 옮긴 지난 비율이고, 값이 바뀌면 1 s 동안 이어서 그린다. 앱이 시간을 세지는 않는다.
class _CountRing extends StatelessWidget {
  const _CountRing(this.value);

  final double value;

  @override
  Widget build(BuildContext context) => TweenAnimationBuilder<double>(
        tween: Tween(end: value),
        duration: motion(context, const Duration(seconds: 1)),
        builder: (context, v, _) => SizedBox.square(
          dimension: 38,
          child: CircularProgressIndicator(
            value: v,
            strokeWidth: 5,
            strokeCap: StrokeCap.round,
            color: Tone.normal.main,
            backgroundColor: Hs.line,
          ),
        ),
      );
}

/// 해제 조건 카드 (높이 156).
class ConditionCard extends StatelessWidget {
  const ConditionCard(this.condition, {super.key});

  final ReleaseCondition condition;

  @override
  Widget build(BuildContext context) {
    final (bg, border, ink, mark) = _checkStyle(condition.check);
    final progress = condition.progress;
    final dot =
        progress == null ? mark : _CheckDot(Hs.card, _CountRing(progress));
    return Semantics(
      label: '${condition.name}: ${switch (condition.check) {
        Check.met => '충족',
        Check.unmet => '미충족',
        Check.waiting => progress == null ? '확인 중' : '진행 중',
      }} — ${condition.text}',
      excludeSemantics: true,
      child: Container(
        height: 156,
        padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 22),
        decoration: BoxDecoration(
          color: bg,
          borderRadius: BorderRadius.circular(24),
          border: Border.all(color: border, width: 2),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Row(
              children: [
                dot,
                const SizedBox(width: 14),
                Expanded(
                  child: Text(condition.name,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: hs(22, weight: FontWeight.w600, height: 1.3)),
                ),
              ],
            ),
            Flexible(
              child: Text.rich(
                  TextSpan(text: condition.text, children: [
                    if (condition.extra case final extra?)
                      TextSpan(
                          text: ' $extra',
                          style: hs(18, color: Hs.t3, height: 1.4)),
                  ]),
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style:
                      hs(20, weight: FontWeight.w500, color: ink, height: 1.4)),
            ),
          ],
        ),
      ),
    );
  }
}

/// 해제 조건 한 줄 알약 (높이 48, 위험 대화상자).
class ConditionPill extends StatelessWidget {
  const ConditionPill(this.condition, {super.key});

  final ReleaseCondition condition;

  @override
  Widget build(BuildContext context) {
    final (bg, border, ink, icon) = switch (condition.check) {
      Check.met => (
          Tone.normal.tint,
          Tone.normal.tint,
          Tone.normal.ink,
          _CheckDot(Tone.normal.main,
              const Icon(Icons.check_rounded, size: 20, color: Colors.white),
              size: 32),
        ),
      Check.unmet => (
          Hs.card,
          Tone.danger.main,
          Tone.danger.ink,
          _CheckDot(
              Tone.danger.main,
              const Icon(Icons.priority_high_rounded,
                  size: 20, color: Colors.white),
              size: 32),
        ),
      Check.waiting => (
          Hs.bg,
          Hs.line,
          Hs.t2,
          const _CheckDot(
              Hs.line, Icon(Icons.remove_rounded, size: 20, color: Hs.t2),
              size: 32),
        ),
    };
    return Container(
      height: 48,
      padding: const EdgeInsets.fromLTRB(8, 0, 18, 0),
      decoration: BoxDecoration(
        color: bg,
        borderRadius: BorderRadius.circular(24),
        border: Border.all(color: border, width: 1.5),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          icon,
          const SizedBox(width: 10),
          Text(condition.short,
              style: hs(19, weight: FontWeight.w500, color: ink)),
        ],
      ),
    );
  }
}

/// 해제 조건 3칸 막대 (인덕션 카드, 대화상자를 닫은 뒤).
class ConditionBars extends StatelessWidget {
  const ConditionBars(this.conditions, {super.key});

  final List<ReleaseCondition> conditions;

  @override
  Widget build(BuildContext context) => SizedBox(
        width: 180,
        height: 10,
        child: Row(
          children: [
            for (final (i, c) in conditions.indexed) ...[
              if (i > 0) const SizedBox(width: 6),
              Expanded(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: switch (c.check) {
                      Check.met => Tone.normal.main,
                      Check.unmet => Tone.danger.main,
                      Check.waiting => Hs.line,
                    },
                    borderRadius: BorderRadius.circular(5),
                  ),
                ),
              ),
            ],
          ],
        ),
      );
}

/// 오른쪽 대화상자 (770×700): 자리 비움 경고와 반려동물 잠금. 카메라 판정 패널을 함께 강조한다.
class SideAlert extends StatelessWidget {
  const SideAlert({
    super.key,
    required this.tone,
    required this.icon,
    required this.title,
    required this.body,
    required this.chips,
    this.primary,
    required this.onClose,
  });

  final Tone tone;
  final IconData icon;
  final String title;
  final String body;
  final List<Widget> chips;

  /// 없으면 「확인」만 둔다 — 반려동물 잠금처럼 Core가 알아서 푸는 창 (정인 10/26).
  final Widget? primary;
  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) => Positioned(
        left: 1112,
        top: 190,
        width: 770,
        height: 700,
        child: Appear(
            child: Semantics(
          scopesRoute: true,
          explicitChildNodes: true,
          namesRoute: true,
          label: title.replaceAll('\n', ' '),
          liveRegion: true,
          child: Container(
            padding: const EdgeInsets.fromLTRB(56, 56, 56, 48),
            decoration: BoxDecoration(
              color: Hs.card,
              borderRadius: BorderRadius.circular(32),
              boxShadow: Hs.modalShadow,
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Expanded(
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: Alignment.topLeft,
                    child: SizedBox(
                      width: 658,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          _IconBadge(tone: tone, icon: icon, size: 112),
                          const SizedBox(height: 24),
                          Text(title,
                              style: hs(44,
                                  weight: FontWeight.w600,
                                  height: 1.25,
                                  spacing: -0.01)),
                          const SizedBox(height: 24),
                          Text(body, style: hs(24, color: Hs.t2, height: 1.55)),
                          const SizedBox(height: 24),
                          Wrap(spacing: 12, runSpacing: 12, children: chips),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    if (primary != null) ...[
                      primary!,
                      const SizedBox(height: 12),
                    ],
                    HsButton(label: '확인', primary: false, onPressed: onClose),
                  ],
                ),
              ],
            ),
          ),
        )),
      );
}

/// 카메라 판정 패널 둘레 강조 (오른쪽 대화상자와 함께). CSS `box-shadow: 0 0 0 2px`와
/// 달리 Flutter 그림자는 안쪽까지 칠하므로 패널 바깥 2 px 테두리로 그린다.
class PanelHighlight extends StatelessWidget {
  const PanelHighlight({super.key, required this.color});

  final Color color;

  @override
  Widget build(BuildContext context) => Positioned(
        left: 46,
        top: 142,
        width: 1054,
        height: 594,
        child: IgnorePointer(
          child: DecoratedBox(
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(26),
              border: Border.all(color: color, width: 2),
            ),
          ),
        ),
      );
}

class _IconBadge extends StatelessWidget {
  const _IconBadge(
      {required this.tone, required this.icon, required this.size});

  final Tone tone;
  final IconData icon;
  final double size;

  @override
  Widget build(BuildContext context) => Container(
        width: size,
        height: size,
        decoration: BoxDecoration(
          color: tone.tint,
          shape: BoxShape.circle,
          boxShadow: [
            BoxShadow(
                color: tone.main.withValues(alpha: 0.08), spreadRadius: 12),
          ],
        ),
        child: Icon(icon,
            size: size / 2,
            color: tone == Tone.normal ? const Color(0xFF1F9E63) : tone.main),
      );
}

/// 음성 결과 (아래 1824×320). state.voice가 바뀌면 잠깐 띄운다.
/// 계약에 「듣는 중」·인식 원문이 없으므로 SafetyCore의 수락·거부 결과만 보인다.
class VoiceResultPanel extends StatelessWidget {
  const VoiceResultPanel({
    super.key,
    required this.voice,
    required this.state,
    required this.closesIn,
    required this.onClose,
  });

  final VoiceResult voice;
  final CoreState state;
  final Duration closesIn;
  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) {
    final ok = voice.accepted;
    final title = ok ? intentLabel(voice) : voiceRejectTitle(voice);
    // 후드는 요청보다 안전 하한(fan.min)이 높으면 실제 단수가 다를 수 있다 (불변식 ②, UI 설계 v1 §6 C2).
    final detail = ok
        ? voice.lastIntent.startsWith('FAN')
            ? '${voiceDeviceNow(voice, state)} · 실제 단수는 안전 하한을 따라요'
            : voiceDeviceNow(voice, state)
        : '“${intentLabel(voice)}” — ${voiceReasonLabel(voice.reason)}';
    return Positioned(
      left: 48,
      right: 48,
      bottom: 48,
      height: 320,
      child: Appear(
          child: Semantics(
        liveRegion: true,
        label: '음성 명령 ${ok ? '실행' : '거부'}. $title. $detail',
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 48, vertical: 40),
          decoration: BoxDecoration(
            color: Hs.card,
            borderRadius: BorderRadius.circular(32),
            boxShadow: Hs.modalShadow,
          ),
          child: Row(
            children: [
              ok
                  ? Container(
                      width: 160,
                      height: 160,
                      alignment: Alignment.center,
                      decoration: BoxDecoration(
                          color: Tone.voice.main.withValues(alpha: 0.12),
                          shape: BoxShape.circle),
                      child: Container(
                        width: 136,
                        height: 136,
                        decoration: BoxDecoration(
                          color: Tone.voice.main,
                          shape: BoxShape.circle,
                          boxShadow: [
                            BoxShadow(
                                color: Tone.voice.main.withValues(alpha: 0.35),
                                offset: const Offset(0, 12),
                                blurRadius: 32),
                          ],
                        ),
                        child: const Icon(Icons.check_rounded,
                            size: 72, color: Colors.white),
                      ),
                    )
                  : Container(
                      width: 160,
                      height: 160,
                      decoration: BoxDecoration(
                          color: Tone.danger.tint, shape: BoxShape.circle),
                      child: Icon(
                          voice.reason == 'LOCKED'
                              ? Icons.lock_outline_rounded
                              : Icons.block_rounded,
                          size: 76,
                          color: Tone.danger.main),
                    ),
              const SizedBox(width: 40),
              Expanded(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Container(
                          height: 40,
                          padding: const EdgeInsets.fromLTRB(12, 0, 16, 0),
                          decoration: BoxDecoration(
                              color: Tone.voice.tint,
                              borderRadius: BorderRadius.circular(20)),
                          child: Row(
                            children: [
                              Icon(Icons.mic_none_rounded,
                                  size: 20, color: Tone.voice.ink),
                              const SizedBox(width: 8),
                              Text('음성 명령',
                                  style: hs(18,
                                      weight: FontWeight.w500,
                                      color: Tone.voice.ink)),
                            ],
                          ),
                        ),
                        const SizedBox(width: 14),
                        Text(ok ? '요청을 받았어요' : '지금은 할 수 없어요',
                            style: hs(22,
                                weight: FontWeight.w500,
                                color: ok ? Tone.voice.ink : Tone.danger.ink)),
                      ],
                    ),
                    const SizedBox(height: 18),
                    Text(title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: hs(52,
                            weight: FontWeight.w600,
                            height: 1.25,
                            spacing: -0.01)),
                    const SizedBox(height: 18),
                    Text(detail,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: hs(22, color: ok ? Hs.t3 : Hs.t2)),
                    // 거부되면 지금 말할 수 있는 지원 명령을 보여 준다 (설계 v1 §6 C3). 누르는 버튼이 아니다.
                    if (!ok) ...[
                      const SizedBox(height: 16),
                      Row(
                        children: [
                          for (final (i, example) in voiceExamples.indexed) ...[
                            if (i > 0) const SizedBox(width: 12),
                            InfoChip(example, background: Hs.bg),
                          ],
                        ],
                      ),
                    ],
                  ],
                ),
              ),
              Column(
                crossAxisAlignment: CrossAxisAlignment.end,
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  CloseCircle(onPressed: onClose),
                  Text('${closesIn.inSeconds.clamp(1, 9)}초 뒤 닫혀요',
                      style: hs(18, color: Hs.t3)),
                ],
              ),
            ],
          ),
        ),
      )),
    );
  }
}

/// 기기 제어 시트에 무엇을 보일지. 홈의 인덕션·후드 카드는 그 기기만, 도크 「기기」는 둘 다.
enum DevicePart { both, induction, hood }

/// 기기 제어 시트 (아래 1824×600). 단수 버튼은 ui_cmd 요청만 보낸다.
/// 비활성 조건은 SafetyCore 값 그대로: 인덕션 켜기·올리기는 `allowed_on`,
/// 후드는 `fan.min` 아래 (설계서 §3.5·§5.4). 끄기·내리기는 항상 보낼 수 있다.
/// 「인덕션 잠그기·잠금 풀기」(인덕션이 보이는 창에만)는 사용자 잠금(USER_LOCK)만 바꾼다 — 위험·반려동물 잠금 중에는
/// 그 잠금이 풀릴 때까지 누를 수 없다(사용자 잠금으로 안전 잠금을 풀 수 없다는 것을 화면에서도, D-058).
class DeviceSheet extends StatelessWidget {
  const DeviceSheet({
    super.key,
    this.part = DevicePart.both,
    required this.state,
    required this.onInduction,
    required this.onFan,
    required this.onUserLock,
    required this.onClose,
  });

  final DevicePart part;
  final CoreState state;
  final ValueChanged<int> onInduction;
  final ValueChanged<int> onFan;
  final ValueChanged<bool> onUserLock;
  final VoidCallback onClose;

  @override
  Widget build(BuildContext context) {
    final ind = state.induction;
    final fan = state.fan;
    final locked = state.lock != Lock.unlocked;
    final userLocked = state.lock == Lock.lockedUser;
    final inductionColumn = _DeviceColumn(
      icon: Halo(
        size: 120,
        tone: ind.level > 0 && !locked ? Tone.warning.main : null,
        child: locked
            ? Icon(Icons.lock_outline_rounded,
                size: 56,
                color: switch (state.lock) {
                  Lock.lockedCat => Tone.pet.main,
                  Lock.lockedUser => Tone.idle.main,
                  _ => Tone.danger.main,
                })
            : LedRing(level: ind.level, size: 84),
      ),
      name: '인덕션',
      level: locked ? '잠김' : levelText(ind.level),
      // 꺼져 있으면 위 굵은 「꺼짐」과 겹쳐 빈 줄로 높이만 둔다 (정인 10/27).
      status: _StatusLine(
        locked
            ? lockLabel(state.lock)
            : ind.level > 0
                ? '가열 중'
                : null,
      ),
      group: '인덕션 단계',
      // 사용자 잠금(USER_LOCK)만 바꾼다. 위험·반려동물 잠금 중에는 누를 수 없다(D-058).
      leading: _LockButton(
        locked: userLocked,
        onPressed: locked && !userLocked ? null : () => onUserLock(!userLocked),
      ),
      current: ind.level,
      // 켜기·올리기만 막는다. 끄기·내리기는 언제나 보낼 수 있다.
      enabled: (i) => i <= ind.level || ind.allowedOn,
      onSelect: onInduction,
      note: ind.allowedOn
          ? '자리를 비우면 자동으로 낮추고, 화재 위험이나 반려동물이 감지되면 잠가요.'
          : '지금은 켜기·올리기가 막혀 있어요. 끄기·내리기는 보낼 수 있어요.',
    );
    final hoodColumn = _DeviceColumn(
      icon: Halo(
        size: 120,
        tone: fan.level > 0 ? Hs.t1 : null,
        child: SpinningFan(level: fan.level, size: 64),
      ),
      name: '후드',
      level: levelText(fan.level),
      status: fan.requested != fan.level
          ? SmallPill(
              text: '요청 ${levelText(fan.requested)} → 안전 최소 ${fan.min}단 유지',
              background: Tone.caution.tint,
              color: Tone.caution.ink,
              pulse: false,
            )
          : _StatusLine(fan.level > 0 ? '환기 중' : null),
      group: '후드 단계',
      current: fan.level,
      enabled: (i) => i >= fan.min,
      onSelect: onFan,
      note: fan.min > 0
          ? '안전 최소 ${fan.min}단 아래로는 내릴 수 없어요. 온도·연기에 따라 더 올라갈 수 있어요.'
          : '온도·연기에 따라 자동으로 더 올라갈 수 있어요.',
    );
    // 한 기기만이면 가운데 912 폭(둘 다일 때 한 칸 + 여백과 같음).
    final single = part != DevicePart.both;
    return Positioned(
      left: single ? 504 : 48,
      right: single ? 504 : 48,
      bottom: 48,
      height: 600,
      child: Appear(
          child: Semantics(
        scopesRoute: true,
        explicitChildNodes: true,
        namesRoute: true,
        label: '기기 제어',
        child: Container(
          padding: const EdgeInsets.fromLTRB(48, 36, 48, 40),
          decoration: BoxDecoration(
            color: Hs.card,
            borderRadius: BorderRadius.circular(32),
            boxShadow: Hs.modalShadow,
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  Text(
                      switch (part) {
                        DevicePart.both => '기기 제어',
                        DevicePart.induction => '인덕션',
                        DevicePart.hood => '후드',
                      },
                      style: hs(32, weight: FontWeight.w600, spacing: -0.01)),
                  const Spacer(),
                  CloseCircle(onPressed: onClose, size: 64),
                ],
              ),
              const SizedBox(height: 28),
              Expanded(
                child: switch (part) {
                  DevicePart.induction => inductionColumn,
                  DevicePart.hood => hoodColumn,
                  DevicePart.both => Row(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Expanded(
                          child: Container(
                            padding: const EdgeInsets.only(right: 48),
                            decoration: const BoxDecoration(
                              border:
                                  Border(right: BorderSide(color: Hs.track)),
                            ),
                            child: inductionColumn,
                          ),
                        ),
                        const SizedBox(width: 48),
                        Expanded(child: hoodColumn),
                      ],
                    ),
                },
              ),
            ],
          ),
        ),
      )),
    );
  }
}

/// 기기 이름·단수 아래 연한 상태 줄. [text]가 null이면 같은 높이로 비워 둔다.
class _StatusLine extends StatelessWidget {
  const _StatusLine(this.text);

  final String? text;

  @override
  Widget build(BuildContext context) => Visibility(
        visible: text != null,
        maintainSize: true,
        maintainAnimation: true,
        maintainState: true,
        child: Text(text ?? '꺼짐', style: hs(20, color: Hs.t2)),
      );
}

class _DeviceColumn extends StatelessWidget {
  const _DeviceColumn({
    required this.icon,
    required this.name,
    required this.level,
    required this.status,
    required this.group,
    required this.current,
    required this.enabled,
    required this.onSelect,
    required this.note,
    this.leading,
  });

  final Widget icon;
  final String name;
  final String level;
  final Widget status;
  final String group;
  final int current;
  final bool Function(int level) enabled;
  final ValueChanged<int> onSelect;
  final String note;

  /// 단수 버튼 줄 맨 왼쪽(인덕션의 「잠그기」, 정인 10/26).
  final Widget? leading;

  @override
  Widget build(BuildContext context) => Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              icon,
              const SizedBox(width: 24),
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(name, style: hs(22, color: Hs.t2)),
                  const SizedBox(height: 6),
                  Row(
                    children: [
                      Text(level,
                          style: hs(56, weight: FontWeight.w600, height: 1)),
                      const SizedBox(width: 10),
                      status,
                    ],
                  ),
                ],
              ),
            ],
          ),
          const SizedBox(height: 24),
          Semantics(
            label: group,
            child: Row(
              children: [
                if (leading != null) ...[
                  Expanded(child: leading!),
                  const SizedBox(width: 12),
                ],
                for (var i = 0; i <= 3; i++) ...[
                  if (i > 0) const SizedBox(width: 12),
                  Expanded(
                    child: LevelButton(
                      level: i,
                      selected: i == current,
                      onPressed: enabled(i) ? () => onSelect(i) : null,
                    ),
                  ),
                ],
              ],
            ),
          ),
          const SizedBox(height: 24),
          Text(note, style: hs(18, color: Hs.t2, height: 1.5)),
        ],
      );
}

/// 인덕션 단수 줄 맨 왼쪽 「잠그기」·「잠금 풀기」 (높이 104, [LevelButton]과 같은 틀).
class _LockButton extends StatelessWidget {
  const _LockButton({required this.locked, required this.onPressed});

  final bool locked;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final fg = onPressed == null ? Hs.t3 : Hs.t1;
    return SizedBox(
      height: 104,
      child: TextButton(
        onPressed: onPressed,
        style: TextButton.styleFrom(
          backgroundColor: onPressed == null ? Hs.bg : Hs.card,
          foregroundColor: fg,
          disabledForegroundColor: fg,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(24),
            side: const BorderSide(color: Hs.line, width: 1.5),
          ),
        ),
        child: FittedBox(
          fit: BoxFit.scaleDown,
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                  locked ? Icons.lock_open_rounded : Icons.lock_outline_rounded,
                  size: 22,
                  color: fg),
              const SizedBox(width: 6),
              Text(locked ? '잠금 풀기' : '잠그기',
                  style: hs(22, weight: FontWeight.w500, color: fg)),
            ],
          ),
        ),
      ),
    );
  }
}

/// 단수 버튼 (높이 104). 0은 「끄기」.
class LevelButton extends StatelessWidget {
  const LevelButton({
    super.key,
    required this.level,
    required this.selected,
    required this.onPressed,
  });

  final int level;
  final bool selected;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    final disabled = onPressed == null;
    final fg = selected
        ? Colors.white
        : disabled
            ? Hs.t3
            : Hs.t1;
    return Semantics(
      selected: selected,
      child: SizedBox(
        height: 104,
        child: TextButton(
          onPressed: onPressed,
          style: TextButton.styleFrom(
            backgroundColor: selected
                ? Hs.t1
                : disabled
                    ? Hs.bg
                    : Hs.card,
            foregroundColor: fg,
            disabledForegroundColor: fg,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(24),
              side: selected
                  ? BorderSide.none
                  : const BorderSide(color: Hs.line, width: 1.5),
            ),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (disabled && !selected) ...[
                const Icon(Icons.lock_outline_rounded, size: 20, color: Hs.t3),
                const SizedBox(width: 8),
              ],
              Text(
                level == 0 ? '끄기' : '$level',
                style: hs(level == 0 ? 24 : 36,
                    weight: selected ? FontWeight.w600 : FontWeight.w500,
                    color: fg),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

/// 위쪽 가운데 알림 (높이 64).
class Toast extends StatelessWidget {
  const Toast({super.key, required this.text});

  final String text;

  @override
  Widget build(BuildContext context) => Positioned(
        top: 136,
        left: 0,
        right: 0,
        child: Center(
          child: Appear(
              dy: -24,
              scale: 1,
              child: Semantics(
                liveRegion: true,
                child: Container(
                  height: 64,
                  padding: const EdgeInsets.fromLTRB(16, 0, 28, 0),
                  decoration: BoxDecoration(
                    color: Hs.card,
                    borderRadius: BorderRadius.circular(32),
                    boxShadow: const [
                      BoxShadow(
                          color: Color(0x29000000),
                          offset: Offset(0, 12),
                          blurRadius: 40),
                    ],
                  ),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Container(
                        width: 36,
                        height: 36,
                        decoration: BoxDecoration(
                            color: Tone.normal.main, shape: BoxShape.circle),
                        child: const Icon(Icons.check_rounded,
                            size: 22, color: Colors.white),
                      ),
                      const SizedBox(width: 14),
                      Text(text, style: hs(22, weight: FontWeight.w500)),
                    ],
                  ),
                ),
              )),
        ),
      );
}

/// 시작 화면: 첫 state를 받기 전. 배경은 기기 폴더의 LG 그라디언트 02이고, 없으면 01 색조의
/// 그라디언트로 그린다. LG 로고는 상단 바 로고와 같은 자리에 둬서 홈으로 넘어가도 움직이지 않는다.
class Splash extends StatelessWidget {
  const Splash(
      {super.key, required this.version, this.brand = BrandImages.none});

  final String version;
  final BrandImages brand;

  /// 핑크 배경(02) 위에서는 목업 `hs-bg-2` 색: 진행 막대·점은 검정, 보조 글자는 검정 66 %.
  /// 파일이 없어 01 색조로 그릴 때는 `hs-bg-1` 색(막대 LG 레드, 점 초록) 그대로다.
  bool get _pink => brand.splash != null;
  Color get _ink2 => _pink ? const Color(0xA81A1A1A) : Hs.t2;

  @override
  Widget build(BuildContext context) => Semantics(
        liveRegion: true,
        label: 'HOOD&SEEK, 안전 장치와 연결하는 중',
        child: Stack(
          children: [
            const Positioned.fill(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  color: Color(0xFFF7E9EA),
                  gradient: RadialGradient(
                    center: Alignment(-0.35, 0.25),
                    radius: 1.1,
                    colors: [
                      Color(0xFFF5A3AE),
                      Color(0xFFF9D3D6),
                      Color(0xFFFBF1F1),
                    ],
                    stops: [0, 0.45, 1],
                  ),
                ),
              ),
            ),
            const Positioned.fill(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  gradient: RadialGradient(
                    center: Alignment(0.9, -0.7),
                    radius: 0.8,
                    colors: [Color(0x99F7B6BE), Color(0x00F7B6BE)],
                  ),
                ),
              ),
            ),
            if (brand.splash case final background?)
              Positioned.fill(
                child: Image(
                  image: background,
                  fit: BoxFit.cover,
                  excludeFromSemantics: true,
                  errorBuilder: (context, error, stack) =>
                      const SizedBox.shrink(),
                ),
              ),
            // 캔버스 여백 48 + 상단 바 72 안 가운데 (dashboard_screen.dart `_header`).
            if (brand.logo case final logo?)
              Positioned(left: 48, top: 60, child: LgLogo(image: logo)),
            Center(
              child: Transform.translate(
                offset: const Offset(0, -40),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Container(
                      width: 168,
                      height: 168,
                      decoration: const BoxDecoration(
                        color: Color(0x99FFFFFF),
                        shape: BoxShape.circle,
                        boxShadow: [
                          BoxShadow(
                              color: Color(0x14000000),
                              offset: Offset(0, 16),
                              blurRadius: 48),
                        ],
                      ),
                      child: const CustomPaint(painter: _MarkPainter()),
                    ),
                    const SizedBox(height: 36),
                    Text('HOOD&SEEK',
                        style: hs(64,
                            weight: FontWeight.w600,
                            height: 1,
                            spacing: 0.14,
                            number: true)),
                    const SizedBox(height: 14),
                    Text('스마트 주방 안전 시스템',
                        style: hs(22, color: _ink2, spacing: 0.02)),
                  ],
                ),
              ),
            ),
            Positioned(
              left: 0,
              right: 0,
              bottom: 96,
              child: Column(
                children: [
                  SizedBox(
                    width: 240,
                    height: 4,
                    child: ClipRRect(
                      borderRadius: BorderRadius.circular(2),
                      child: LinearProgressIndicator(
                        color: _pink ? Hs.t1 : Hs.lgRed,
                        backgroundColor: _pink
                            ? const Color(0x291A1A1A)
                            : const Color(0x29A50034),
                      ),
                    ),
                  ),
                  const SizedBox(height: 20),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Pulse(
                        child: Container(
                          width: 8,
                          height: 8,
                          decoration: BoxDecoration(
                              color: _pink ? Hs.t1 : Tone.normal.main,
                              shape: BoxShape.circle),
                        ),
                      ),
                      const SizedBox(width: 10),
                      Text('안전 장치와 연결하는 중', style: hs(18, color: _ink2)),
                    ],
                  ),
                ],
              ),
            ),
            Positioned(
              right: 48,
              bottom: 40,
              child: Text('버전 $version', style: hs(16, color: _ink2)),
            ),
          ],
        ),
      );
}

/// HOOD&SEEK 마크 (시안 viewBox 56): 후드 호 + 붉은 고리 + 가운데 점.
class _MarkPainter extends CustomPainter {
  const _MarkPainter();

  @override
  void paint(Canvas canvas, Size size) {
    final k = 96 / 56;
    canvas.translate((size.width - 96) / 2, (size.height - 96) / 2);
    final ink = Paint()
      ..color = Hs.t1
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2.4 * k
      ..strokeCap = StrokeCap.round;
    canvas.drawPath(
      Path()
        ..moveTo(10 * k, 22 * k)
        ..cubicTo(10 * k, 14 * k, 18 * k, 8 * k, 28 * k, 8 * k)
        ..cubicTo(38 * k, 8 * k, 46 * k, 14 * k, 46 * k, 22 * k),
      ink,
    );
    canvas
      ..drawCircle(
        Offset(28 * k, 34 * k),
        12 * k,
        Paint()
          ..color = Hs.lgRed
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2.6 * k,
      )
      ..drawCircle(Offset(28 * k, 34 * k), 3 * k, Paint()..color = Hs.t1);
  }

  @override
  bool shouldRepaint(_MarkPainter oldDelegate) => false;
}
