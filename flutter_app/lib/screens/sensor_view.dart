import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../data/core_state.dart';
import '../widgets/hs.dart';
import 'labels.dart';

/// 기록 탭 타임라인 필터(UI 설계 v1 §3.3): 안전·잠금 / 음성 / 그 밖(재실·단수).
enum EventKind { safety, voice, other }

/// 화면이 state 변화를 보고 남기는 한 줄 기록. SafetyCore 로그가 아니라 앱이 받은
/// state의 변화(단계·잠금·재실·음성·연결)를 받은 시각과 함께 적은 것이다.
class HsEvent {
  const HsEvent(this.at, this.tone, this.text, this.trailing,
      {this.kind = EventKind.other});

  final DateTime at;
  final Tone tone;
  final String text;
  final String trailing;
  final EventKind kind;
}

/// 그래프 범위. [points]는 0.5 s 표본 수.
enum TrendRange {
  s60('60초', 120, ['−60초', '−30초']),
  m10('10분', 1200, ['−10분', '−5분']),
  h1('1시간', 7200, ['−60분', '−30분']);

  const TrendRange(this.label, this.points, this.axis);
  final String label;
  final int points;
  final List<String> axis;
}

/// 센서 화면 본문 (1824×776): 온도차·공기 질 그래프와 센서 상태 세 칸.
class SensorView extends StatelessWidget {
  const SensorView({
    super.key,
    required this.state,
    required this.offline,
    required this.range,
    required this.metric,
    required this.pm,
  });

  final CoreState? state;
  final bool offline;
  final TrendRange range;
  final List<double?> metric;
  final List<double?> pm;

  @override
  Widget build(BuildContext context) {
    final s = state;
    final live = offline ? null : s;
    final t = s?.thresholds;
    final baseline = s?.pm.baseline;
    final (tTone, _) = live == null ? (Tone.idle, '') : temperatureGrade(live);
    final (pTone, _) = live == null ? (Tone.idle, '') : pmGrade(live);
    // 타임라인은 기록 탭에 있어 여기서는 뺐다. 온도차·공기 질·센서 상태 세 칸을 나란히 (정인 10/26).
    return Row(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Expanded(
          child: _ChartCard(
            title: '조리 온도차',
            subtitle: '최고 − 중앙',
            value: number(live?.thermal.metric),
            unit: '°C',
            range: range,
            points: metric,
            tone: tTone,
            lines: t == null
                ? const []
                : [
                    ('중불', t.tMid, Tone.caution),
                    ('강불', t.tHigh, Tone.caution),
                    ('과열', t.tDanger, Tone.warning),
                  ],
          ),
        ),
        const SizedBox(width: 24),
        Expanded(
          child: _ChartCard(
            title: '공기 질 PM2.5',
            subtitle: '기준선 + 상승폭',
            value: pmNumber(live?.pm.pm25),
            unit: 'µg/m³',
            range: range,
            points: pm,
            tone: pTone,
            baseline: baseline,
            lines: t == null || baseline == null
                ? const []
                : [
                    ('경고', baseline + t.pmWarn, Tone.warning),
                    ('연기', baseline + t.pmSmoke, Tone.danger),
                  ],
            note: baseline == null ? '기준값 측정 중이라 임계선 없음' : null,
          ),
        ),
        const SizedBox(width: 24),
        Expanded(child: _SensorStatus(state: s, offline: offline)),
      ],
    );
  }
}

class EventList extends StatelessWidget {
  const EventList({
    super.key,
    required this.events,
    required this.rows,
    this.title = '최근 일어난 일',
    this.action,
  });

  final List<HsEvent> events;

  /// 보일 줄 수. null이면 전부를 스크롤로 보인다(기록 탭).
  final int? rows;
  final String title;

  /// 제목 오른쪽(기록 탭 필터).
  final Widget? action;

  @override
  Widget build(BuildContext context) {
    final shown = rows == null ? events : events.take(rows!).toList();
    return HsCard(
      padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 28),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(
                  child: Text(title,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: hs(22, weight: FontWeight.w500))),
              if (action != null) action!,
            ],
          ),
          const SizedBox(height: 14),
          if (shown.isEmpty)
            Text('아직 기록이 없어요', style: hs(20, color: Hs.t2))
          else if (rows == null)
            Expanded(
              child: ListView.builder(
                itemCount: shown.length,
                itemBuilder: (_, i) => _row(shown, i),
              ),
            )
          else
            for (var i = 0; i < shown.length; i++) _row(shown, i),
        ],
      ),
    );
  }

  /// 기록 탭(전체 목록)에서는 안전·잠금 사건과 음성 거부를 그 색 틴트로 강조한다(#156).
  bool _strong(HsEvent e) =>
      rows == null &&
      e.kind != EventKind.other &&
      e.tone != Tone.normal &&
      e.tone != Tone.idle &&
      e.tone != Tone.voice;

  Widget _row(List<HsEvent> shown, int i) {
    final e = shown[i];
    final strong = _strong(e);
    // 강조 줄과 일반 줄이 같은 여백이라 시각·점·글자가 세로로 맞는다 (정인 10/26).
    return Container(
      height: 56,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: strong
          ? BoxDecoration(
              color: e.tone.tint, borderRadius: BorderRadius.circular(12))
          : BoxDecoration(
              border: i == shown.length - 1
                  ? null
                  : const Border(bottom: BorderSide(color: Hs.track)),
            ),
      child: Row(
        children: [
          SizedBox(
            width: 92,
            // 기록 DB에서 읽은 지난 줄은 오늘 시각처럼 보이지 않게 날짜를 붙인다 (#163 정인 리뷰).
            child: FittedBox(
              fit: BoxFit.scaleDown,
              alignment: Alignment.centerLeft,
              child: Text(_when(e.at), style: hs(18, color: Hs.t2)),
            ),
          ),
          const SizedBox(width: 18),
          Container(
            width: 10,
            height: 10,
            decoration:
                BoxDecoration(color: e.tone.main, shape: BoxShape.circle),
          ),
          const SizedBox(width: 18),
          Expanded(
            child: Text(e.text,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: hs(20,
                    weight: strong ? FontWeight.w600 : FontWeight.w400,
                    color: strong ? e.tone.ink : Hs.t1)),
          ),
          Text(e.trailing, style: hs(18, color: Hs.t2)),
        ],
      ),
    );
  }
}

class _SensorStatus extends StatelessWidget {
  const _SensorStatus({required this.state, required this.offline});

  final CoreState? state;
  final bool offline;

  @override
  Widget build(BuildContext context) {
    final s = offline ? null : state;
    final tiles = [
      (
        Icons.thermostat_rounded,
        '열화상',
        s == null
            ? '수신 없음'
            : s.thermal.valid
                ? '작동 중'
                : '응답 없음',
        s?.thermal.valid ?? false,
      ),
      (
        Icons.air_rounded,
        '미세먼지',
        s == null
            ? '수신 없음'
            : !s.pm.valid
                ? '응답 없음'
                : s.pm.baseline == null
                    ? '작동 중 · 기준값 측정 중'
                    : '작동 중',
        s?.pm.valid ?? false,
      ),
      (
        Icons.videocam_outlined,
        '카메라',
        s == null
            ? '수신 없음'
            : s.vision.fresh
                ? '작동 중 · 사람·반려동물 판정'
                : '판정 정보 없음',
        s?.vision.fresh ?? false,
      ),
      (
        Icons.memory_rounded,
        '안전 장치',
        s == null
            ? '연결 끊김'
            : '연결됨 · 가동 ${duration(Duration(seconds: s.uptimeS))}',
        s != null,
      ),
    ];
    final reasons = s == null || s.reasons.isEmpty
        ? '없음'
        : s.reasons.map((code) => reasonLabel(code) ?? code).join(' · ');
    return HsCard(
      padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 28),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text('센서 상태', style: hs(22, weight: FontWeight.w500)),
          const SizedBox(height: 14),
          // 한 줄에 하나씩 네 줄이 카드 높이를 나눠 꽉 찬다 (정인 10/26, 2×2 → 1×4).
          for (final (i, tile) in tiles.indexed) ...[
            if (i > 0) const SizedBox(height: 12),
            Expanded(child: _tile(tile)),
          ],
          const SizedBox(height: 14),
          // 모르는 사유 코드도 숨기지 않고 원문 그대로 보인다.
          Text('현재 사유 · $reasons',
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: hs(18, color: Hs.t2)),
        ],
      ),
    );
  }

  Widget _tile((IconData, String, String, bool) t) {
    final (icon, name, status, ok) = t;
    // 1×4 칸 한 줄: 아이콘·이름 바로 오른쪽에 상태, 왼쪽 정렬 (정인 10/27).
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 18),
      decoration:
          BoxDecoration(color: Hs.bg, borderRadius: BorderRadius.circular(18)),
      child: Row(
        children: [
          Container(
            width: 56,
            height: 56,
            decoration:
                const BoxDecoration(color: Hs.card, shape: BoxShape.circle),
            child: Icon(icon, size: 30, color: Hs.t1),
          ),
          const SizedBox(width: 18),
          Text(name, style: hs(24, weight: FontWeight.w500)),
          const SizedBox(width: 14),
          Flexible(
            // LG EI Text 18은 24 옆에서 글자가 1 px 위로 그려져서, 글자 가운데를 이름과 맞춘다.
            child: Padding(
              padding: const EdgeInsets.only(top: 2),
              child: Text(status,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style:
                      hs(18, color: ok ? Tone.normal.ink : Tone.warning.ink)),
            ),
          ),
        ],
      ),
    );
  }
}

/// 추이 그래프 카드. 임계선은 state의 표시용 사본(`thresholds`)이고
/// 앱은 이 값으로 비교·판단하지 않는다 (설계서 §5.4, D-042 ②).
class _ChartCard extends StatelessWidget {
  const _ChartCard({
    required this.title,
    required this.subtitle,
    required this.value,
    required this.unit,
    required this.range,
    required this.points,
    required this.tone,
    required this.lines,
    this.baseline,
    this.note,
  });

  final String title;
  final String subtitle;
  final String value;
  final String unit;
  final TrendRange range;
  final List<double?> points;
  final Tone tone;
  final List<(String, double, Tone)> lines;
  final double? baseline;
  final String? note;

  @override
  Widget build(BuildContext context) {
    final shown = points.length > range.points
        ? points.sublist(points.length - range.points)
        : points;
    final legend = [
      for (final (name, v, _) in lines) '$name ${threshold(v)}',
    ].join(' · ');
    return HsCard(
      padding: const EdgeInsets.fromLTRB(32, 28, 32, 20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          // 세 칸으로 좁아져(약 590) 부제는 제목 아래 줄로 내린다.
          Row(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(title,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: hs(22, weight: FontWeight.w500)),
                    Text(subtitle,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: hs(16, color: Hs.t2)),
                  ],
                ),
              ),
              Text(value,
                  style:
                      hs(44, weight: FontWeight.w300, height: 1, number: true)),
              const SizedBox(width: 6),
              Text(unit, style: hs(20, color: Hs.t2)),
            ],
          ),
          const SizedBox(height: 12),
          Expanded(
            child: Semantics(
              label: '$title 최근 ${range.label} 그래프. 현재 $value$unit. '
                  '${note ?? '임계선 $legend'}',
              child: CustomPaint(
                painter: _ChartPainter(
                  points: shown,
                  capacity: range.points,
                  axis: range.axis,
                  color: tone.main,
                  lines: lines,
                  baseline: baseline,
                ),
              ),
            ),
          ),
          if (note != null) Text(note!, style: hs(16, color: Hs.t2)),
        ],
      ),
    );
  }
}

/// 임계선 라벨. 선끼리 [gap]보다 가까우면(연기가 포화해 눈금이 2000대로 커질 때 경고·연기)
/// 글자가 겹치므로 가장 위 선 하나에 「경고 39 · 연기 59」로 합친다 (정인 10/26).
@visibleForTesting
List<(String, double, Tone)> thresholdLabels(
    List<(String, double, Tone)> lines, double Function(double) y,
    {double gap = 20}) {
  final sorted = [...lines]..sort((a, b) => a.$2.compareTo(b.$2));
  final groups = <List<(String, double, Tone)>>[];
  for (final line in sorted) {
    final last = groups.isEmpty ? null : groups.last;
    if (last != null && (y(last.last.$2) - y(line.$2)).abs() < gap) {
      last.add(line);
    } else {
      groups.add([line]);
    }
  }
  return [
    for (final g in groups)
      (
        g.map((l) => '${l.$1} ${threshold(l.$2)}').join(' · '),
        y(g.last.$2),
        g.last.$3,
      ),
  ];
}

class _ChartPainter extends CustomPainter {
  _ChartPainter({
    required this.points,
    required this.capacity,
    required this.axis,
    required this.color,
    required this.lines,
    required this.baseline,
  });

  final List<double?> points;
  final int capacity;
  final List<String> axis;
  final Color color;
  final List<(String, double, Tone)> lines;
  final double? baseline;

  static const _left = 50.0;
  static const _top = 20.0;
  static const _bottomGap = 60.0;

  @override
  void paint(Canvas canvas, Size size) {
    final right = size.width - 6;
    final bottom = size.height - _bottomGap + 20;
    // 눈금: 값·임계선 최대의 1.1배를 4칸으로 나눈 「보기 좋은」 간격.
    final peak = [
      for (final v in points)
        if (v != null) v,
      for (final (_, v, _) in lines) v,
    ].fold(1.0, math.max);
    final step = _niceStep(peak * 1.1 / 4);
    final top = step * 4;
    double y(double v) => bottom - (bottom - _top) * (v / top).clamp(0.0, 1.0);

    final grid = Paint()..color = Hs.track;
    for (var i = 0; i <= 4; i++) {
      final gy = y(step * i);
      canvas.drawRect(Rect.fromLTWH(_left, gy, right - _left, 1),
          i == 0 ? (Paint()..color = Hs.line) : grid);
      _text(canvas, _tick(step * i), Offset(_left - 10, gy), 16, Hs.t3,
          align: TextAlign.right);
    }
    if (baseline case final b?) {
      _dash(canvas, y(b), right, Hs.t3, 3, 5);
      _text(
          canvas, '기준선 ${threshold(b)}', Offset(_left + 4, y(b) - 6), 15, Hs.t2,
          above: true);
    }
    for (final (_, v, tone) in lines) {
      _dash(canvas, y(v), right, tone.main, 6, 6);
    }
    for (final (text, at, tone) in thresholdLabels(lines, y)) {
      _text(canvas, text, Offset(right - 4, at - 6), 15, tone.ink,
          align: TextAlign.right, above: true);
    }

    // 최신 값이 오른쪽 끝. 범위가 안 찼으면 왼쪽이 빈다. 값이 없던 구간은 선을 끊는다.
    final dx = (right - _left) / (capacity - 1);
    final first = capacity - points.length;
    final path = Path();
    final area = Path();
    var drawing = false;
    double? lastX;
    double? lastY;
    double? segmentStart;
    void closeArea() {
      if (segmentStart != null && lastX != null) {
        area
          ..lineTo(lastX, bottom)
          ..lineTo(segmentStart, bottom)
          ..close();
      }
    }

    for (var i = 0; i < points.length; i++) {
      final v = points[i];
      if (v == null) {
        if (drawing) closeArea();
        drawing = false;
        segmentStart = null;
        continue;
      }
      final x = _left + (first + i) * dx;
      if (drawing) {
        path.lineTo(x, y(v));
        area.lineTo(x, y(v));
      } else {
        path.moveTo(x, y(v));
        area.moveTo(x, y(v));
        segmentStart = x;
      }
      drawing = true;
      lastX = x;
      lastY = y(v);
    }
    if (drawing) closeArea();
    canvas
      ..drawPath(area, Paint()..color = color.withValues(alpha: 0.08))
      ..drawPath(
        path,
        Paint()
          ..color = color
          ..style = PaintingStyle.stroke
          ..strokeWidth = 3
          ..strokeJoin = StrokeJoin.round
          ..strokeCap = StrokeCap.round,
      );
    if (points.isNotEmpty && points.last != null) {
      final end = Offset(lastX!, lastY!);
      canvas
        ..drawCircle(end, 8.5, Paint()..color = Colors.white)
        ..drawCircle(end, 5.5, Paint()..color = color);
    }

    final ty = bottom + 18;
    _text(canvas, axis[0], Offset(_left, ty), 16, Hs.t3);
    _text(canvas, axis[1], Offset((_left + right) / 2, ty), 16, Hs.t3,
        align: TextAlign.center);
    _text(canvas, '지금', Offset(right, ty), 16, Hs.t3, align: TextAlign.right);
  }

  void _dash(Canvas canvas, double y, double right, Color color, double on,
      double off) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = 1.5;
    for (var x = _left; x < right; x += on + off) {
      canvas.drawLine(Offset(x, y), Offset(math.min(x + on, right), y), paint);
    }
  }

  void _text(Canvas canvas, String text, Offset at, double size, Color color,
      {TextAlign align = TextAlign.left, bool above = false}) {
    final painter = TextPainter(
      text: TextSpan(text: text, style: hs(size, color: color)),
      textDirection: TextDirection.ltr,
    )..layout();
    final dx = switch (align) {
      TextAlign.right => at.dx - painter.width,
      TextAlign.center => at.dx - painter.width / 2,
      _ => at.dx,
    };
    final dy = above ? at.dy - painter.height : at.dy - painter.height / 2;
    painter.paint(canvas, Offset(dx, dy));
  }

  static double _niceStep(double raw) {
    final exp = math.pow(10, (math.log(raw) / math.ln10).floor()).toDouble();
    for (final m in [1.0, 2.0, 2.5, 5.0, 10.0]) {
      if (raw <= m * exp) return m * exp;
    }
    return 10 * exp;
  }

  static String _tick(double v) =>
      v == v.roundToDouble() ? v.toInt().toString() : v.toStringAsFixed(1);

  // 표본 목록은 같은 List를 고쳐 쓰므로 매번 다시 그린다 (0.5 s 간격).
  @override
  bool shouldRepaint(_ChartPainter oldDelegate) => true;
}

String _when(DateTime at) {
  final now = DateTime.now();
  final today =
      at.year == now.year && at.month == now.month && at.day == now.day;
  return today
      ? clockSeconds(at)
      : '${at.month}/${at.day} ${clockSeconds(at).substring(0, 5)}';
}
