import 'package:flutter/material.dart';

import '../data/hood_care.dart';
import '../widgets/hs.dart';
import 'labels.dart';

/// 「기록」 탭 C. 후드 케어 카드 (1920×1080 시안 기준 750×162 — 홈 인덕션·후드 카드 줄, #156 재범 배치).
/// 후드가 돈 양과 청소 권장까지의 비율을 보여 주고, 「청소했어요」로 누적을 되돌린다.
/// 안전 상태와 섞이지 않게 위험·경고 색을 쓰지 않는다. 버튼은 기기 안 기록만 바꾸고
/// ui_cmd를 보내지 않는다.
class HoodCareCard extends StatelessWidget {
  const HoodCareCard({
    super.key,
    required this.care,
    required this.notice,
    required this.onCleaned,
  });

  final HoodCare care;

  /// 저장·읽기 실패 알림 한 줄. 없으면 null.
  final String? notice;

  /// 확인 창에서 「되돌리기」를 눌렀을 때만 불린다.
  final VoidCallback onCleaned;

  @override
  Widget build(BuildContext context) {
    final progress = care.progress;
    final percent = (progress * 100).floor();
    final cleanedAt = care.cleanedAt;
    // 저장 알림이 먼저, 그다음 시계 알림. 시계가 틀리면 하루 기록·예측만 쉬고 누적은 계속 센다.
    final shown =
        notice ?? (care.clockSuspect ? '기기 시계를 확인해 주세요 · 하루 기록은 쉬어요' : null);
    final note =
        '연기 중 가동 ${_span(Duration(seconds: care.smokeSeconds.floor()))} · '
        '${cleanedAt == null ? '청소 기록 없음' : '마지막 청소 ${cleanedAt.month}월 ${cleanedAt.day}일'}';
    return HsCard(
      padding: const EdgeInsets.fromLTRB(32, 20, 28, 20),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  crossAxisAlignment: CrossAxisAlignment.baseline,
                  textBaseline: TextBaseline.alphabetic,
                  children: [
                    Text('후드 케어',
                        style: hs(22, weight: FontWeight.w500, height: 1.2)),
                    // 저장하지 못하면 「기기에 저장」 자리에 알림을 둔다.
                    Expanded(
                      child: Text(' · ${shown ?? '기기에 저장'}',
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: hs(16,
                              color: shown == null ? Hs.t3 : Tone.warning.ink,
                              height: 1.2)),
                    ),
                  ],
                ),
                FittedBox(
                  fit: BoxFit.scaleDown,
                  alignment: Alignment.centerLeft,
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.baseline,
                    textBaseline: TextBaseline.alphabetic,
                    children: [
                      Text(
                        number(care.levelSeconds / 3600),
                        style: hs(40,
                            weight: FontWeight.w300, height: 1.1, number: true),
                      ),
                      const SizedBox(width: 6),
                      Text('단·시간 가동', style: hs(18, color: Hs.t2)),
                    ],
                  ),
                ),
                Text(note,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: hs(16, color: Hs.t2, height: 1.3)),
              ],
            ),
          ),
          const SizedBox(width: 20),
          SizedBox(
            width: 250,
            child: Semantics(
              label: '청소 권장 기준 대비 $percent퍼센트. ${_forecast(care)}',
              excludeSemantics: true,
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text('청소 권장까지',
                            style: hs(16, color: Hs.t2, height: 1.3)),
                      ),
                      Text('$percent%',
                          style: hs(16, weight: FontWeight.w600, height: 1.3)),
                    ],
                  ),
                  const SizedBox(height: 6),
                  _Bar(progress.clamp(0.0, 1.0)),
                  const SizedBox(height: 8),
                  Text(_forecast(care),
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                      style: hs(16, weight: FontWeight.w500, height: 1.3)),
                  Text(
                    '기준 ${number(cleanAfterLevelSeconds / 3600, digits: 0)} 단·시간 (제안)',
                    style: hs(14, color: Hs.t3, height: 1.3),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(width: 20),
          SizedBox(
            width: 150,
            child: HsButton(
              label: '청소했어요',
              primary: false,
              onPressed: () => _confirm(context),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _confirm(BuildContext context) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (context) => const _CleanDialog(),
    );
    if (ok == true) onCleaned();
  }
}

/// 예측 문구. 비율·남은 날은 [HoodCare]가 센 값을 옮긴다.
String _forecast(HoodCare care) {
  if (care.progress >= 1) return '청소할 때가 됐어요';
  return switch (care.daysLeft()) {
    final n? => '이 속도면 약 $n일 뒤 청소할 때예요',
    null => '기록이 쌓이면 알려 드릴게요',
  };
}

/// 「4분 12초」, 한 시간이 넘으면 「3시간 5분」.
String _span(Duration d) =>
    d.inHours > 0 ? '${d.inHours}시간 ${d.inMinutes % 60}분' : duration(d);

/// 청소 권장 막대 (높이 12). 안전 색이 아닌 중립 색으로 채운다.
class _Bar extends StatelessWidget {
  const _Bar(this.fraction);

  final double fraction;

  @override
  Widget build(BuildContext context) => ClipRRect(
        borderRadius: BorderRadius.circular(6),
        child: SizedBox(
          height: 12,
          child: Stack(
            fit: StackFit.expand,
            children: [
              const ColoredBox(color: Hs.track),
              FractionallySizedBox(
                alignment: Alignment.centerLeft,
                widthFactor: fraction,
                child: const ColoredBox(color: Hs.t2),
              ),
            ],
          ),
        ),
      );
}

/// 「청소했어요」 확인 창. 1차 버튼은 위험 확인(LG 레드)과 섞이지 않게 기본 검정이다.
class _CleanDialog extends StatelessWidget {
  const _CleanDialog();

  @override
  Widget build(BuildContext context) => Dialog(
        backgroundColor: Hs.card,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(32)),
        child: SizedBox(
          width: 640,
          child: Padding(
            padding: const EdgeInsets.all(40),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text('누적을 0으로 되돌릴까요?',
                    style: hs(32, weight: FontWeight.w600, height: 1.3)),
                const SizedBox(height: 12),
                Text('후드를 청소했다면 가동 기록을 처음부터 다시 셉니다. 기록은 이 기기 안에만 남아요.',
                    style: hs(22, color: Hs.t2, height: 1.5)),
                const SizedBox(height: 32),
                Row(
                  children: [
                    Expanded(
                      child: HsButton(
                        label: '취소',
                        primary: false,
                        onPressed: () => Navigator.of(context).pop(false),
                      ),
                    ),
                    const SizedBox(width: 16),
                    Expanded(
                      child: HsButton(
                        label: '되돌리기',
                        onPressed: () => Navigator.of(context).pop(true),
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
        ),
      );
}
