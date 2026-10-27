import 'dart:math' as math;

import 'package:flutter/material.dart';

/// 디자인 토큰. 기준은 사용자 디스플레이 UI 시안(`Tokens.html`)이며 수치는 모두
/// 1920×1080 화면 px이다. 화면 전체를 이 크기로 그린 뒤 실제 화면에 맞춰 줄인다.
abstract final class Hs {
  static const bg = Color(0xFFF4F5F7);
  static const card = Color(0xFFFFFFFF);
  static const t1 = Color(0xFF1A1A1A);
  static const t2 = Color(0xFF6B7076);
  static const t3 = Color(0xFF9AA0A6);
  static const line = Color(0xFFE6E8EB);
  static const track = Color(0xFFEEF0F2);
  static const chip = Color(0xFFECEEF1);

  /// LG 레드. 위험 대화상자 1차 버튼과 시작 화면 마크에서만 쓴다 (설계 v1 §2.1).
  static const lgRed = Color(0xFFA50034);
  static const shadow = [
    BoxShadow(color: Color(0x0F000000), offset: Offset(0, 4), blurRadius: 24),
  ];
  static const modalShadow = [
    BoxShadow(color: Color(0x47000000), offset: Offset(0, 24), blurRadius: 80),
  ];

  /// LG EI 글꼴은 저장소에 넣지 않고 기기에서 읽는다(`brand_fonts.dart`).
  /// 못 읽으면 앱에 넣은 Noto Sans KR로 그린다. Headline에는 한글이 없어 Text로 이어진다.
  static const text = 'LG EI Text';
  static const number = 'LG EI Headline';
  static const fallback = ['LG EI Text', 'Noto Sans KR'];
}

/// 시안의 글자 규칙: 크기·굵기·색만 바꿔 쓴다. [number]는 핵심 수치용 글꼴.
TextStyle hs(
  double size, {
  FontWeight weight = FontWeight.w400,
  Color color = Hs.t1,
  double? height,
  double spacing = 0,
  bool number = false,
}) =>
    TextStyle(
      fontFamily: number ? Hs.number : Hs.text,
      fontFamilyFallback: Hs.fallback,
      fontSize: size,
      fontWeight: weight,
      color: color,
      height: height,
      letterSpacing: spacing * size,
      fontFeatures: const [FontFeature.tabularFigures()],
    );

/// 단계 색 한 벌: 진한 색 · 글자 색 · 옅은 바탕.
class Tone {
  const Tone(this.main, this.ink, this.tint);

  final Color main;
  final Color ink;
  final Color tint;

  static const normal =
      Tone(Color(0xFF2BB673), Color(0xFF167A4B), Color(0xFFE6F6EE));
  static const caution =
      Tone(Color(0xFFF5B800), Color(0xFF7A5A00), Color(0xFFFFF6DB));
  static const warning =
      Tone(Color(0xFFF0761F), Color(0xFFB5410A), Color(0xFFFDEBDD));
  static const danger =
      Tone(Color(0xFFD7263D), Color(0xFFB8202F), Color(0xFFFBE7EA));
  static const voice =
      Tone(Color(0xFF3B6DFF), Color(0xFF1E4FD6), Color(0xFFE8EFFF));
  static const pet =
      Tone(Color(0xFFB5753A), Color(0xFF7A4A1C), Color(0xFFF4EBE2));
  static const idle = Tone(Hs.t3, Hs.t2, Hs.chip);
}

/// 시안 곡선 `cubic-bezier(.2,.8,.2,1)`.
const hsEase = Cubic(0.2, 0.8, 0.2, 1);

/// 「동작 줄이기」 설정이면 0, 아니면 [d].
Duration motion(BuildContext context, Duration d) =>
    (MediaQuery.maybeDisableAnimationsOf(context) ?? false) ? Duration.zero : d;

/// 나타날 때 0.35 s 동안 [dy]만큼 떠오르며 커지는 진입 (설계 v1 §5.1 모달 진입).
class Appear extends StatelessWidget {
  const Appear(
      {super.key, required this.child, this.dy = 24, this.scale = .96});

  final Widget child;
  final double dy;
  final double scale;

  @override
  Widget build(BuildContext context) => TweenAnimationBuilder<double>(
        tween: Tween(begin: 0, end: 1),
        duration: motion(context, const Duration(milliseconds: 350)),
        curve: hsEase,
        builder: (context, t, child) => Opacity(
          opacity: t.clamp(0.0, 1.0),
          child: Transform.translate(
            offset: Offset(0, dy * (1 - t)),
            child:
                Transform.scale(scale: scale + (1 - scale) * t, child: child),
          ),
        ),
        child: child,
      );
}

/// 흰 카드(모서리 24 · 그림자). [border]가 있으면 2 px 테두리로 강조한다.
class HsCard extends StatelessWidget {
  const HsCard({
    super.key,
    required this.child,
    this.padding = EdgeInsets.zero,
    this.border,
    this.onTap,
    this.label,
  });

  final Widget child;
  final EdgeInsets padding;
  final Color? border;
  final VoidCallback? onTap;
  final String? label;

  @override
  Widget build(BuildContext context) {
    final shape = RoundedRectangleBorder(
      borderRadius: BorderRadius.circular(24),
      side: border == null
          ? BorderSide.none
          : BorderSide(color: border!, width: 2),
    );
    final body = Padding(padding: padding, child: child);
    return DecoratedBox(
      decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(24), boxShadow: Hs.shadow),
      child: Material(
        color: Hs.card,
        shape: shape,
        clipBehavior: Clip.antiAlias,
        child: onTap == null
            ? body
            : Semantics(
                button: true,
                label: label,
                child: InkWell(onTap: onTap, child: body),
              ),
      ),
    );
  }
}

/// LG전자 로고 (기기 폴더의 공식 PNG). 파일 안에 여백이 있어 높이 48이면 심벌 지름이
/// 32쯤으로 홈 아이콘과 같다. 비율·색을 바꾸지 않고, 못 그리면 자리를 비운다 (설계 v1 §1).
class LgLogo extends StatelessWidget {
  const LgLogo({super.key, required this.image, this.height = 48});

  final ImageProvider image;
  final double height;

  /// 원본 3211×1127.
  static const aspect = 3211 / 1127;

  @override
  Widget build(BuildContext context) => Semantics(
        label: 'LG전자',
        image: true,
        child: Image(
          image: image,
          width: height * aspect,
          height: height,
          fit: BoxFit.contain,
          filterQuality: FilterQuality.medium,
          excludeFromSemantics: true,
          errorBuilder: (context, error, stack) => const SizedBox.shrink(),
        ),
      );
}

/// 상단 오른쪽 상태 알약 (높이 52).
class StatusPill extends StatelessWidget {
  const StatusPill({super.key, required this.tone, required this.text});

  final Tone tone;
  final String text;

  @override
  Widget build(BuildContext context) => Semantics(
        liveRegion: true,
        label: '상태 $text',
        excludeSemantics: true,
        child: Container(
          height: 52,
          padding: const EdgeInsets.fromLTRB(18, 0, 24, 0),
          decoration: BoxDecoration(
              color: tone.tint, borderRadius: BorderRadius.circular(26)),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              _Dot(size: 12, color: tone.main),
              const SizedBox(width: 12),
              Text(text,
                  style: hs(22, weight: FontWeight.w500, color: tone.ink)),
            ],
          ),
        ),
      );
}

/// 작은 알약 (높이 30, 「켜짐」·「환기 중」 등). [pulse]면 앞 점이 깜빡인다.
class SmallPill extends StatelessWidget {
  const SmallPill({
    super.key,
    required this.text,
    required this.background,
    required this.color,
    this.pulse = true,
  });

  final String text;
  final Color background;
  final Color color;
  final bool pulse;

  @override
  Widget build(BuildContext context) => Container(
        height: 30,
        padding: const EdgeInsets.symmetric(horizontal: 12),
        decoration: BoxDecoration(
            color: background, borderRadius: BorderRadius.circular(15)),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            pulse
                ? Pulse(child: _Dot(size: 8, color: color))
                : _Dot(size: 8, color: color),
            const SizedBox(width: 8),
            Text(text, style: hs(16, weight: FontWeight.w500, color: color)),
          ],
        ),
      );
}

/// 수치 칩 (높이 44, 대화상자 안).
class InfoChip extends StatelessWidget {
  const InfoChip(this.text, {super.key, required this.background, this.color});

  final String text;
  final Color background;
  final Color? color;

  @override
  Widget build(BuildContext context) => Container(
        height: 44,
        padding: const EdgeInsets.symmetric(horizontal: 18),
        decoration: BoxDecoration(
            color: background, borderRadius: BorderRadius.circular(22)),
        child: Center(
          widthFactor: 1,
          child: Text(text,
              style: hs(20, weight: FontWeight.w500, color: color ?? Hs.t1)),
        ),
      );
}

class _Dot extends StatelessWidget {
  const _Dot({required this.size, required this.color});

  final double size;
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
        width: size,
        height: size,
        decoration: BoxDecoration(color: color, shape: BoxShape.circle),
      );
}

/// 반복 애니메이션의 공통 뼈대. 「동작 줄이기」 설정이면 멈춘 첫 장면만 그린다.
abstract class _LoopState<T extends StatefulWidget> extends State<T>
    with SingleTickerProviderStateMixin {
  late final AnimationController loop =
      AnimationController(vsync: this, duration: period);

  Duration get period;
  bool get running => true;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _sync();
  }

  @override
  void didUpdateWidget(covariant T oldWidget) {
    super.didUpdateWidget(oldWidget);
    final changed = loop.duration != period;
    loop.duration = period;
    if (changed && loop.isAnimating) loop.repeat();
    _sync();
  }

  void _sync() {
    final still = MediaQuery.maybeDisableAnimationsOf(context) ?? false;
    if (running && !still) {
      if (!loop.isAnimating) loop.repeat();
    } else {
      loop
        ..stop()
        ..value = 0;
    }
  }

  @override
  void dispose() {
    loop.dispose();
    super.dispose();
  }
}

/// 1.2 s 주기로 작아졌다 커지는 점 (시안 `hs-pulse`).
class Pulse extends StatefulWidget {
  const Pulse({super.key, required this.child});

  final Widget child;

  @override
  State<Pulse> createState() => _PulseState();
}

class _PulseState extends _LoopState<Pulse> {
  @override
  Duration get period => const Duration(milliseconds: 1200);

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
        animation: loop,
        builder: (context, child) {
          final k = (1 - math.cos(loop.value * 2 * math.pi)) / 2;
          return Opacity(
            opacity: 1 - 0.55 * k,
            child: Transform.scale(scale: 1 - 0.45 * k, child: child),
          );
        },
        child: widget.child,
      );
}

/// 기기 아이콘 바탕 원. [tone]이 있으면 바깥으로 퍼지는 고리를 그린다 (`hs-halo`).
class Halo extends StatefulWidget {
  const Halo({
    super.key,
    required this.size,
    required this.child,
    this.tone,
    this.background = Hs.bg,
  });

  final double size;
  final Widget child;
  final Color? tone;
  final Color background;

  @override
  State<Halo> createState() => _HaloState();
}

class _HaloState extends _LoopState<Halo> {
  @override
  Duration get period => const Duration(milliseconds: 1800);

  @override
  bool get running => widget.tone != null;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
        animation: loop,
        builder: (context, child) => Container(
          width: widget.size,
          height: widget.size,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            color: widget.background,
            shape: BoxShape.circle,
            boxShadow: [
              if (widget.tone case final tone? when loop.isAnimating)
                BoxShadow(
                  color: tone.withValues(alpha: 0.35 * (1 - loop.value)),
                  spreadRadius: 16 * loop.value,
                ),
            ],
          ),
          child: child,
        ),
        child: widget.child,
      );
}

/// 후드 팬 아이콘. 단수가 높을수록 빨리 돈다 (2 s · 1.1 s · 0.6 s 한 바퀴).
class SpinningFan extends StatefulWidget {
  const SpinningFan(
      {super.key, required this.level, this.size = 44, this.color = Hs.t1});

  final int level;
  final double size;
  final Color color;

  @override
  State<SpinningFan> createState() => _SpinningFanState();
}

class _SpinningFanState extends _LoopState<SpinningFan> {
  @override
  Duration get period => Duration(
      milliseconds: switch (widget.level) { 1 => 2000, 2 => 1100, _ => 600 });

  @override
  bool get running => widget.level > 0;

  @override
  Widget build(BuildContext context) => RotationTransition(
        turns: loop,
        child: CustomPaint(
          size: Size.square(widget.size),
          painter: _FanPainter(widget.color),
        ),
      );
}

/// 시안의 4날 팬 아이콘 (viewBox 24, 선 1.4). 한 날을 그리고 90°씩 돌린다.
class _FanPainter extends CustomPainter {
  _FanPainter(this.color);

  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.4
      ..strokeJoin = StrokeJoin.round
      ..strokeCap = StrokeCap.round;
    canvas.scale(size.width / 24);
    canvas.drawCircle(const Offset(12, 12), 2, paint);
    final blade = Path()
      ..moveTo(12, 10)
      ..cubicTo(12, 6, 14, 4, 16, 4)
      ..cubicTo(18, 4, 19, 7, 16, 9)
      ..cubicTo(14, 10, 12, 10, 12, 10)
      ..close();
    for (var i = 0; i < 4; i++) {
      canvas
        ..save()
        ..translate(12, 12)
        ..rotate(i * math.pi / 2)
        ..translate(-12, -12)
        ..drawPath(blade, paint)
        ..restore();
    }
  }

  @override
  bool shouldRepaint(_FanPainter oldDelegate) => oldDelegate.color != color;
}

/// 인덕션 LED 링 그림 (시안 viewBox 56). [level] 0..3을 주황 호로 채운다.
/// 켜져 있으면 링이 2.4 s 주기로 숨 쉰다(불투명 60↔100 %, 설계 v1 §5.2).
class LedRing extends StatefulWidget {
  const LedRing({super.key, required this.level, this.size = 56});

  final int level;
  final double size;

  @override
  State<LedRing> createState() => _LedRingState();
}

class _LedRingState extends _LoopState<LedRing> {
  @override
  Duration get period => const Duration(milliseconds: 2400);

  @override
  bool get running => widget.level > 0;

  @override
  Widget build(BuildContext context) => Semantics(
        label:
            widget.level == 0 ? '인덕션 LED 링, 꺼짐' : '인덕션 LED 링, ${widget.level}단',
        child: AnimatedBuilder(
          animation: loop,
          builder: (context, _) => CustomPaint(
            size: Size.square(widget.size),
            painter: _LedRingPainter(
                widget.level, 0.8 + 0.2 * math.cos(loop.value * 2 * math.pi)),
          ),
        ),
      );
}

class _LedRingPainter extends CustomPainter {
  _LedRingPainter(this.level, this.glow);

  final int level;
  final double glow;

  @override
  void paint(Canvas canvas, Size size) {
    final k = size.width / 56;
    final c = Offset(28 * k, 28 * k);
    final stroke = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 4 * k
      ..color = Hs.line;
    canvas.drawCircle(c, 22 * k, stroke);
    if (level > 0) {
      canvas.drawArc(
        Rect.fromCircle(center: c, radius: 22 * k),
        -math.pi / 2,
        2 * math.pi * level / 3,
        false,
        stroke
          ..color = Tone.warning.main.withValues(alpha: glow)
          ..strokeCap = StrokeCap.round,
      );
    }
    // 켜지면 가운데는 바깥 호와 같은 주황 점 하나 (정인 10/26). 꺼져 있으면 지금처럼 검은 화구.
    if (level > 0) {
      canvas.drawCircle(
          c, 4 * k, Paint()..color = Tone.warning.main.withValues(alpha: glow));
      return;
    }
    canvas.drawCircle(
      c,
      11 * k,
      Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 1.6 * k
        ..color = Hs.t1,
    );
    canvas.drawCircle(c, 3 * k, Paint()..color = Hs.t1);
  }

  @override
  bool shouldRepaint(_LedRingPainter oldDelegate) =>
      oldDelegate.level != level || oldDelegate.glow != glow;
}

/// 단수 막대 3개 (폭 180).
class LevelBars extends StatelessWidget {
  const LevelBars({super.key, required this.level, required this.color});

  final int level;
  final Color color;

  @override
  Widget build(BuildContext context) => SizedBox(
        width: 180,
        height: 10,
        child: Row(
          children: [
            for (var i = 1; i <= 3; i++) ...[
              if (i > 1) const SizedBox(width: 6),
              Expanded(
                child: DecoratedBox(
                  decoration: BoxDecoration(
                    color: i <= level ? color : Hs.line,
                    borderRadius: BorderRadius.circular(5),
                  ),
                ),
              ),
            ],
          ],
        ),
      );
}

/// 270° 원호 게이지 (132 px, 시안 viewBox 160). 채움 비율은 표시용 눈금이고
/// 색·문구는 SafetyCore가 보낸 사유 코드에서 온다.
class ArcGauge extends StatelessWidget {
  const ArcGauge({
    super.key,
    required this.fraction,
    required this.tone,
    required this.label,
    this.dashed = false,
    this.semanticLabel,
  });

  final double? fraction;
  final Tone tone;
  final String label;
  final bool dashed;
  final String? semanticLabel;

  @override
  Widget build(BuildContext context) => Semantics(
        label: semanticLabel,
        child: SizedBox.square(
          dimension: 132,
          child: TweenAnimationBuilder<double>(
            tween: Tween(end: (fraction ?? 0).clamp(0.0, 1.0)),
            duration: motion(context, const Duration(milliseconds: 600)),
            curve: hsEase,
            builder: (context, f, child) => CustomPaint(
              painter:
                  _ArcPainter(fraction == null ? null : f, tone.main, dashed),
              child: child,
            ),
            // 「정상 (약불)」처럼 긴 문구도 링 안쪽(약 96 px)을 넘지 않게 줄인다.
            child: Center(
              child: SizedBox(
                width: 96,
                child: FittedBox(
                  fit: BoxFit.scaleDown,
                  child: Text(label,
                      maxLines: 1,
                      style: hs(18, weight: FontWeight.w500, color: tone.ink)),
                ),
              ),
            ),
          ),
        ),
      );
}

class _ArcPainter extends CustomPainter {
  _ArcPainter(this.fraction, this.color, this.dashed);

  final double? fraction;
  final Color color;
  final bool dashed;

  @override
  void paint(Canvas canvas, Size size) {
    final k = size.width / 160;
    final rect =
        Rect.fromCircle(center: Offset(80 * k, 80 * k), radius: 64 * k);
    final paint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 10 * k
      ..strokeCap = StrokeCap.round;
    const start = 3 * math.pi / 4;
    const sweep = 3 * math.pi / 2;
    if (dashed) {
      // 점검: 값이 없으므로 전체 원을 점선으로 (시안 SensorFault).
      const dashes = 18;
      for (var i = 0; i < dashes; i++) {
        canvas.drawArc(rect, i * 2 * math.pi / dashes, math.pi / dashes / 2,
            false, paint..color = color);
      }
      return;
    }
    canvas.drawArc(rect, start, sweep, false, paint..color = Hs.track);
    final f = fraction;
    if (f != null && f > 0) {
      canvas.drawArc(
          rect, start, sweep * f.clamp(0.0, 1.0), false, paint..color = color);
    }
  }

  @override
  bool shouldRepaint(_ArcPainter oldDelegate) =>
      oldDelegate.fraction != fraction ||
      oldDelegate.color != color ||
      oldDelegate.dashed != dashed;
}

/// 둥근 큰 버튼 (높이 72). [primary]가 아니면 흰 바탕 + 회색 테두리.
class HsButton extends StatelessWidget {
  const HsButton({
    super.key,
    required this.label,
    required this.onPressed,
    this.background = Hs.t1,
    this.primary = true,
    this.icon,
  });

  final String label;
  final VoidCallback? onPressed;
  final Color background;
  final bool primary;
  final IconData? icon;

  @override
  Widget build(BuildContext context) {
    final enabled = onPressed != null;
    final fg = !enabled
        ? Hs.t3
        : primary
            ? Colors.white
            : Hs.t1;
    return SizedBox(
      height: 72,
      child: TextButton(
        onPressed: onPressed,
        style: TextButton.styleFrom(
          backgroundColor: !enabled
              ? Hs.chip
              : primary
                  ? background
                  : Hs.card,
          foregroundColor: fg,
          disabledForegroundColor: Hs.t3,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(36),
            side: primary
                ? BorderSide.none
                : const BorderSide(color: Hs.line, width: 1.5),
          ),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            if (icon != null) ...[
              Icon(icon, size: 24, color: fg),
              const SizedBox(width: 8),
            ],
            Text(label,
                style: hs(24,
                    weight: primary ? FontWeight.w600 : FontWeight.w500,
                    color: fg)),
          ],
        ),
      ),
    );
  }
}

/// 원형 닫기 버튼.
class CloseCircle extends StatelessWidget {
  const CloseCircle({super.key, required this.onPressed, this.size = 72});

  final VoidCallback onPressed;
  final double size;

  @override
  Widget build(BuildContext context) => SizedBox.square(
        dimension: size,
        child: IconButton(
          tooltip: '닫기',
          onPressed: onPressed,
          style: IconButton.styleFrom(backgroundColor: Hs.bg),
          icon: Icon(Icons.close_rounded, size: size * 0.42, color: Hs.t1),
        ),
      );
}

/// 「확인 중」 회전 표시 (1.4 s 한 바퀴, 설계 v1 §4.6). 「동작 줄이기」면 멈춘다.
class WaitSpinner extends StatefulWidget {
  const WaitSpinner({super.key, this.size = 34});

  final double size;

  @override
  State<WaitSpinner> createState() => _WaitSpinnerState();
}

class _WaitSpinnerState extends _LoopState<WaitSpinner> {
  @override
  Duration get period => const Duration(milliseconds: 1400);

  @override
  Widget build(BuildContext context) => RotationTransition(
        turns: loop,
        child: CustomPaint(
            size: Size.square(widget.size), painter: const _SpinnerPainter()),
      );
}

class _SpinnerPainter extends CustomPainter {
  const _SpinnerPainter();

  @override
  void paint(Canvas canvas, Size size) {
    canvas.scale(size.width / 24);
    final rect = Rect.fromCircle(center: const Offset(12, 12), radius: 8);
    final paint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = 2
      ..strokeCap = StrokeCap.round;
    canvas
      ..drawCircle(
          const Offset(12, 12), 8, paint..color = const Color(0xFFD5D8DC))
      ..drawArc(rect, -math.pi / 2, math.pi / 2, false, paint..color = Hs.t2);
  }

  @override
  bool shouldRepaint(_SpinnerPainter oldDelegate) => false;
}
