import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../data/core_state.dart';
import '../widgets/hs.dart';
import 'labels.dart';
import 'overlays.dart';

/// 홈 본문 (1824×776): 왼쪽 카메라 판정 + 기기 카드, 오른쪽 헤드라인·지표·음성 카드.
/// [state]는 마지막으로 받은 state이고, [offline]이면 값 대신 「—」와 마지막 값을 보인다.
class HomeView extends StatelessWidget {
  const HomeView({
    super.key,
    required this.state,
    required this.headline,
    required this.offline,
    required this.now,
    required this.lastAt,
    required this.absentSince,
    required this.thermalLostSince,
    required this.voiceAt,
    required this.onDevices,
    required this.onInduction,
    required this.onSensors,
    required this.onInductionOn,
    this.video,
  });

  final CoreState? state;
  final Headline headline;
  final bool offline;
  final DateTime now;
  final DateTime? lastAt;
  final DateTime? absentSince;
  final DateTime? thermalLostSince;
  final DateTime? voiceAt;
  final VoidCallback onDevices;
  final VoidCallback onInduction;
  final VoidCallback onSensors;
  final VoidCallback onInductionOn;

  /// 카메라 카드 영상. 없으면 그림과 판정 칩만 (설계 v1 §3.2).
  final Stream<Uint8List>? video;

  @override
  Widget build(BuildContext context) {
    final s = state;
    final since = lastAt == null ? null : now.difference(lastAt!);
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        SizedBox(
          width: 1050,
          child: Column(
            children: [
              SizedBox(
                height: 590,
                child: CameraPanel(
                  video: video,
                  state: s,
                  offline: offline,
                  lastAt: lastAt,
                  sinceLast: since,
                  absentFor:
                      absentSince == null ? null : now.difference(absentSince!),
                ),
              ),
              const SizedBox(height: 24),
              SizedBox(
                height: 162,
                child: Row(
                  children: [
                    Expanded(
                      child: InductionCard(
                        state: s,
                        offline: offline,
                        onOpen: onInduction,
                        onTurnOn: onInductionOn,
                      ),
                    ),
                    const SizedBox(width: 24),
                    Expanded(
                      child: HoodCard(
                          state: s, offline: offline, onOpen: onDevices),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
        const SizedBox(width: 24),
        Expanded(
          child: Column(
            children: [
              // 왼쪽 카메라(590)·기기 카드(162)와 줄이 맞게 226 + 24 + 340 = 590, 음성 162.
              SizedBox(height: 226, child: HeroCard(headline: headline)),
              const SizedBox(height: 24),
              SizedBox(
                height: 340,
                child: Row(
                  children: [
                    Expanded(
                      child: _temperatureCard(s, since),
                    ),
                    const SizedBox(width: 24),
                    Expanded(child: _pmCard(s, since)),
                  ],
                ),
              ),
              const SizedBox(height: 24),
              SizedBox(
                height: 162,
                child: VoiceCard(
                  state: s,
                  voice: s?.voice,
                  offline: offline,
                  heardFor: voiceAt == null ? null : now.difference(voiceAt!),
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  Widget _temperatureCard(CoreState? s, Duration? since) {
    if (offline || s == null) {
      return MetricCard.offline(
        label: '조리 온도차',
        unit: '°C',
        last: s == null || since == null
            ? '마지막 값 없음'
            : '마지막 값 ${number(s.thermal.metric)} °C · ${duration(since)} 전',
        onDetail: onSensors,
      );
    }
    final (tone, grade) = temperatureGrade(s);
    final lost =
        thermalLostSince == null ? null : now.difference(thermalLostSince!);
    return MetricCard(
      label: '조리 온도차',
      value: number(s.thermal.metric),
      unit: '°C',
      tone: tone,
      grade: grade,
      fill: temperatureFill(s),
      fault: !s.thermal.valid,
      footer: s.thermal.valid
          ? temperatureFooter(s)
          : '${lost == null ? '' : '${duration(lost)}째 '}응답 없음 · 연결을 확인해 주세요',
      onDetail: onSensors,
    );
  }

  Widget _pmCard(CoreState? s, Duration? since) {
    if (offline || s == null) {
      return MetricCard.offline(
        label: '공기 질 PM2.5',
        unit: 'µg/m³',
        last: s == null || since == null
            ? '마지막 값 없음'
            : '마지막 값 ${pmNumber(s.pm.pm25)} µg/m³ · ${duration(since)} 전',
        onDetail: onSensors,
      );
    }
    final (tone, grade) = pmGrade(s);
    return MetricCard(
      label: '공기 질 PM2.5',
      value: pmNumber(s.pm.pm25),
      unit: 'µg/m³',
      tone: tone,
      grade: grade,
      fill: pmFill(s),
      fault: !s.pm.valid,
      footer: s.pm.valid ? pmFooter(s) : '응답 없음 · 연결을 확인해 주세요',
      onDetail: onSensors,
    );
  }
}

/// 카메라 카드 (설계 v1 §3.2). [video]가 있으면 vision-node가 읽은 프레임(최대 약 16 FPS, #152)을
/// 그리고, 없으면 어두운 주방 그림을 둔다. 영상은 **표시 전용**이다 — 칩·테두리·잠금 표시는 영상이
/// 아니라 state 값으로만 정하고, 영상에 보이는 것과 달라도 고치지 않는다. 왼쪽 위 「● 실시간 HH:MM:SS」는
/// 프레임을 받은 시각이다(#151). 위치가 없으니 감지 테두리도 그리지 않는다.
class CameraPanel extends StatelessWidget {
  const CameraPanel({
    super.key,
    required this.state,
    required this.offline,
    required this.lastAt,
    required this.sinceLast,
    required this.absentFor,
    this.video,
  });

  /// 프레임이 이만큼 안 오면 마지막 프레임 위에 「영상 일시 중지」 (설계 v1 §3.2).
  static const videoStale = Duration(seconds: 2);

  final Stream<Uint8List>? video;

  final CoreState? state;
  final bool offline;
  final DateTime? lastAt;
  final Duration? sinceLast;
  final Duration? absentFor;

  @override
  Widget build(BuildContext context) {
    final s = state;
    if (offline || s == null) return _offline();
    final frames = video;
    if (frames == null) return _panel(s, null);
    return _VideoFrames(
      frames: frames,
      // 프레임을 한 장도 못 받았으면(영상이 꺼진 vision-node 등) 그림으로 둔다.
      builder: (frame) => _panel(s, frame.jpeg == null ? null : frame),
    );
  }

  Widget _panel(CoreState s, _Frame? frame) {
    final fresh = s.vision.fresh;
    // vision 2 s 만료면 person·cat 판정을 그리지 않는다 (불변식 3).
    final person = fresh && s.vision.person;
    final cat = fresh && s.vision.cat;
    final smoke = s.reasons.contains('SMOKE');
    final pm = !smoke && s.reasons.contains('PM_WARN');
    // 과한 연기(SMOKE)는 위험일 때만 빨강, 혼자면 경고다. PM_WARN은 hazard를 안 바꾸는 「연기 있음」 (D-054).
    final smokeTone = s.hazard == Hazard.danger ? Tone.danger : Tone.warning;
    final ring = s.lock == Lock.lockedCat
        ? Tone.pet.main
        : smoke
            ? smokeTone.main
            : (s.hazard == Hazard.mid ||
                        s.hazard == Hazard.high ||
                        s.hazard == Hazard.warning) &&
                    s.presence != Presence.present
                ? Tone.warning.main
                : null;
    final summary = fresh
        ? '사람 ${person ? '있음' : '없음'} · 반려동물 ${cat ? '있음' : '없음'}'
        : '카메라 판정 없음';
    return Semantics(
      label: '주방 카메라 판정, $summary',
      child: DecoratedBox(
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(24),
          boxShadow: [
            if (ring != null) BoxShadow(color: ring, spreadRadius: 2),
          ],
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(24),
          child: Stack(
            children: [
              if (frame == null)
                ..._kitchen(pot: s.induction.level > 0)
              else
                Positioned.fill(
                  child: ColoredBox(
                    color: const Color(0xFF24272B),
                    child: frame.jpeg == null
                        ? null
                        // 판정은 프레임 전체로 하니 화면도 전체를 보인다. 640×480을 카드에
                        // 꽉 채우면(cover) 위아래 약 25%가 잘려 인덕션 끝이 안 보였다(10/26 현장).
                        : Image.memory(
                            frame.jpeg!,
                            fit: BoxFit.contain,
                            gaplessPlayback: true,
                            excludeFromSemantics: true,
                            errorBuilder: (_, __, ___) => const SizedBox(),
                          ),
                  ),
                ),
              // 연기 그림·「연기 있음 · 미세먼지 센서」 태그는 그림(영상 없음)용이다. 실시간 영상 위에는
              // 엉뚱한 자리를 가리켜 빼고, 연기는 공기 질 카드·헤드라인으로 본다 (수현 10/26).
              if ((smoke || pm) && frame == null) ..._haze(strong: smoke),
              ..._shade(),
              if ((smoke || pm) && frame == null)
                Positioned(
                  left: 290,
                  top: 70,
                  child: _Tag(
                    smoke ? '연기 많음 · 미세먼지 센서' : '연기 있음 · 미세먼지 센서',
                    background: smoke ? smokeTone.main : Tone.normal.main,
                  ),
                ),
              // 영상이 오면 왼쪽 위 하나로 「실시간 시각」만 (PM 요청 10/26).
              Positioned(
                left: 24,
                top: 24,
                child: frame != null
                    ? _Badge(
                        dot: frame.stale ? Hs.t3 : const Color(0xFFFF3B30),
                        text: '실시간 ${clockSeconds(frame.at!)}',
                        dim: frame.stale,
                      )
                    : _Badge(
                        dot: fresh ? const Color(0xFFFF3B30) : Hs.t3,
                        text: fresh ? '카메라 판정' : '판정 없음',
                      ),
              ),
              if (frame == null && lastAt != null)
                Positioned(
                  right: 24,
                  top: 24,
                  child: _Badge(text: clockSeconds(lastAt!)),
                ),
              Positioned(
                left: 24,
                bottom: 24,
                child: _Badge(
                  icon: cat ? Icons.pets_rounded : Icons.person_outline_rounded,
                  text: summary,
                  large: true,
                ),
              ),
              if (frame != null && frame.stale && frame.jpeg != null)
                const Positioned(
                  left: 0,
                  right: 0,
                  top: 90,
                  child: Center(
                    child: _CenterChip(
                      icon: Icons.pause_circle_outline_rounded,
                      text: '영상 일시 중지',
                      color: Color(0xE6454B52),
                    ),
                  ),
                ),
              if (!fresh)
                const Center(
                  child: _CenterChip(
                    icon: Icons.videocam_off_outlined,
                    text: '카메라 판정을 받지 못했어요',
                    color: Color(0xE6454B52),
                  ),
                )
              else if (s.presence == Presence.absent)
                Center(
                  child: _CenterChip(
                    icon: Icons.person_off_outlined,
                    text: absentFor == null
                        ? '사람이 없어요'
                        : '사람이 없어요 · ${duration(absentFor!)}',
                    color: Tone.warning.main.withValues(alpha: 0.9),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _offline() => Semantics(
        label: '주방 카메라 판정 없음, 연결 끊김',
        child: ClipRRect(
          borderRadius: BorderRadius.circular(24),
          child: ColoredBox(
            color: const Color(0xFF24272B),
            child: Stack(
              children: [
                Center(
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Container(
                        width: 120,
                        height: 120,
                        decoration: const BoxDecoration(
                            color: Color(0x0FFFFFFF), shape: BoxShape.circle),
                        child: const Icon(Icons.videocam_off_outlined,
                            size: 56, color: Color(0xCCFFFFFF)),
                      ),
                      const SizedBox(height: 20),
                      Text('판정 정보를 받을 수 없어요',
                          style: hs(28,
                              weight: FontWeight.w500, color: Colors.white)),
                      const SizedBox(height: 20),
                      Text(
                        sinceLast == null
                            ? '안전 장치와 다시 연결하는 중'
                            : '안전 장치와 다시 연결하는 중 · ${duration(sinceLast!)}',
                        style: hs(20, color: const Color(0xBFFFFFFF)),
                      ),
                    ],
                  ),
                ),
                const Positioned(
                  left: 24,
                  top: 24,
                  child: _Badge(dot: Hs.t3, text: '끊김', dim: true),
                ),
                Positioned(
                  right: 24,
                  top: 24,
                  child: _Badge(
                    text: lastAt == null
                        ? '수신 기록 없음'
                        : '마지막 수신 ${clockSeconds(lastAt!)}',
                    dim: true,
                  ),
                ),
                const Positioned(
                  right: 24,
                  bottom: 24,
                  child: _Badge(text: '주방 카메라', large: true),
                ),
              ],
            ),
          ),
        ),
      );

  static List<Widget> _kitchen({required bool pot}) => [
        const Positioned.fill(
          child: DecoratedBox(
            decoration: BoxDecoration(
              gradient: LinearGradient(
                begin: Alignment.topCenter,
                end: Alignment.bottomCenter,
                colors: [Color(0xFF2F3338), Color(0xFF1F2225)],
              ),
            ),
          ),
        ),
        const Positioned(
          left: 0,
          right: 0,
          bottom: 0,
          height: 220,
          child: DecoratedBox(
            decoration: BoxDecoration(
              color: Color(0xFF33373C),
              border: Border(top: BorderSide(color: Color(0xFF3F444A))),
            ),
          ),
        ),
        _box(300, 318, 450, 190, const Color(0xFF15171A), radius: 10),
        _ring(352, 356, 96),
        _ring(600, 356, 96),
        if (pot) ...[
          Positioned(
            left: 336,
            top: 322,
            width: 128,
            height: 128,
            child: Container(
              decoration: const BoxDecoration(
                color: Color(0xFF3C4147),
                shape: BoxShape.circle,
                boxShadow: [
                  BoxShadow(color: Color(0xFF52585F), offset: Offset(0, 2)),
                ],
              ),
            ),
          ),
          _box(352, 338, 96, 96, const Color(0xFF2A2E33), radius: 48),
        ],
      ];

  static List<Widget> _shade() => [
        const Positioned.fill(
          child: DecoratedBox(
            decoration: BoxDecoration(
              gradient: RadialGradient(
                center: Alignment(0, -0.1),
                radius: 0.9,
                colors: [
                  Color(0x00000000),
                  Color(0x00000000),
                  Color(0x80000000)
                ],
                stops: [0, 0.52, 1],
              ),
            ),
          ),
        ),
        const Positioned.fill(child: CustomPaint(painter: _GridPainter())),
      ];

  static List<Widget> _haze({required bool strong}) => [
        Positioned(
          left: 300,
          top: 60,
          width: 420,
          height: 300,
          child: _cloud(strong ? 0.55 : 0.3),
        ),
        Positioned(
          left: 180,
          top: -40,
          width: 700,
          height: 320,
          child: _cloud(strong ? 0.4 : 0.2),
        ),
        Positioned.fill(
          child: ColoredBox(
              color: const Color(0xFFBEBEBE)
                  .withValues(alpha: strong ? 0.14 : 0.06)),
        ),
      ];

  static Widget _cloud(double alpha) => DecoratedBox(
        decoration: BoxDecoration(
          gradient: RadialGradient(
            center: const Alignment(0, 0.4),
            colors: [
              const Color(0xFFD6D6D6).withValues(alpha: alpha),
              const Color(0xFFD6D6D6).withValues(alpha: alpha * 0.45),
              const Color(0x00D6D6D6),
            ],
            stops: const [0, 0.45, 0.72],
          ),
        ),
      );

  static Positioned _box(double l, double t, double w, double h, Color color,
          {double radius = 0}) =>
      Positioned(
        left: l,
        top: t,
        width: w,
        height: h,
        child: Container(
          width: w,
          height: h,
          decoration: BoxDecoration(
              color: color, borderRadius: BorderRadius.circular(radius)),
        ),
      );

  static Positioned _ring(double l, double t, double d) => Positioned(
        left: l,
        top: t,
        width: d,
        height: d,
        child: Container(
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            border: Border.all(color: const Color(0xFF2B2F34), width: 2),
          ),
        ),
      );
}

class _GridPainter extends CustomPainter {
  const _GridPainter();

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()..color = const Color(0x09FFFFFF);
    for (var x = 0.0; x < size.width; x += 48) {
      canvas.drawRect(Rect.fromLTWH(x, 0, 1, size.height), paint);
    }
    for (var y = 0.0; y < size.height; y += 48) {
      canvas.drawRect(Rect.fromLTWH(0, y, size.width, 1), paint);
    }
  }

  @override
  bool shouldRepaint(_GridPainter oldDelegate) => false;
}

/// 패널 위 판정 이름표 (연기·공기 질).
class _Tag extends StatelessWidget {
  const _Tag(this.text, {required this.background});

  final String text;
  final Color background;

  @override
  Widget build(BuildContext context) => Container(
        height: 32,
        padding: const EdgeInsets.symmetric(horizontal: 12),
        alignment: Alignment.center,
        decoration: BoxDecoration(
            color: background, borderRadius: BorderRadius.circular(8)),
        child: Text(text,
            style: hs(17, weight: FontWeight.w600, color: Colors.white)),
      );
}

/// 패널 모서리의 반투명 검은 알약.
/// 카메라 카드에 그릴 마지막 프레임과 받은 시각. [stale]이면 2 s 넘게 새 프레임이 없다.
class _Frame {
  const _Frame(this.jpeg, this.at, {required this.stale});

  final Uint8List? jpeg;
  final DateTime? at;
  final bool stale;
}

/// 영상 스트림을 듣고 마지막 프레임을 들고 있는다. 화면은 프레임이 올 때와 2 s 멈출 때만 다시 그린다.
class _VideoFrames extends StatefulWidget {
  const _VideoFrames({required this.frames, required this.builder});

  final Stream<Uint8List> frames;
  final Widget Function(_Frame frame) builder;

  @override
  State<_VideoFrames> createState() => _VideoFramesState();
}

class _VideoFramesState extends State<_VideoFrames> {
  StreamSubscription<Uint8List>? _sub;
  Timer? _staleTimer;
  Uint8List? _jpeg;
  DateTime? _at;
  bool _stale = true;

  @override
  void initState() {
    super.initState();
    _listen();
  }

  @override
  void didUpdateWidget(_VideoFrames old) {
    super.didUpdateWidget(old);
    if (!identical(old.frames, widget.frames)) {
      _sub?.cancel();
      _listen();
    }
  }

  void _listen() {
    _sub = widget.frames.listen((jpeg) {
      _staleTimer?.cancel();
      _staleTimer = Timer(CameraPanel.videoStale, () {
        if (mounted) setState(() => _stale = true);
      });
      setState(() {
        _jpeg = jpeg;
        _at = DateTime.now();
        _stale = false;
      });
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    _staleTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) =>
      widget.builder(_Frame(_jpeg, _at, stale: _stale));
}

class _Badge extends StatelessWidget {
  const _Badge({
    required this.text,
    this.dot,
    this.icon,
    this.large = false,
    this.dim = false,
  });

  final String text;
  final Color? dot;
  final IconData? icon;
  final bool large;
  final bool dim;

  @override
  Widget build(BuildContext context) {
    final color = dim ? const Color(0xB3FFFFFF) : Colors.white;
    return Container(
      height: large ? 44 : 40,
      padding: EdgeInsets.fromLTRB(
          dot != null || icon != null ? 14 : 16, 0, large ? 18 : 16, 0),
      decoration: BoxDecoration(
        color: const Color(0x8C000000),
        borderRadius: BorderRadius.circular(22),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (dot != null) ...[
            Container(
              width: 10,
              height: 10,
              decoration: BoxDecoration(color: dot, shape: BoxShape.circle),
            ),
            const SizedBox(width: 10),
          ],
          if (icon != null) ...[
            Icon(icon, size: 22, color: Colors.white),
            const SizedBox(width: 10),
          ],
          Text(
            text,
            style: hs(large ? 20 : 18,
                weight: dot != null ? FontWeight.w600 : FontWeight.w400,
                color: color),
          ),
        ],
      ),
    );
  }
}

class _CenterChip extends StatelessWidget {
  const _CenterChip(
      {required this.icon, required this.text, required this.color});

  final IconData icon;
  final String text;
  final Color color;

  @override
  Widget build(BuildContext context) => Container(
        height: 56,
        padding: const EdgeInsets.fromLTRB(18, 0, 24, 0),
        decoration: BoxDecoration(
            color: color, borderRadius: BorderRadius.circular(28)),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 26, color: Colors.white),
            const SizedBox(width: 12),
            Text(text,
                style: hs(22, weight: FontWeight.w600, color: Colors.white)),
          ],
        ),
      );
}

/// 안전 헤드라인 카드.
class HeroCard extends StatelessWidget {
  const HeroCard({super.key, required this.headline});

  final Headline headline;

  @override
  Widget build(BuildContext context) => HsCard(
        padding: const EdgeInsets.symmetric(horizontal: 40, vertical: 36),
        child: AnimatedSwitcher(
          duration: motion(context, const Duration(milliseconds: 400)),
          child: Semantics(
            key: ValueKey(headline.title),
            liveRegion: true,
            child: Row(
              children: [
                Container(
                  width: 112,
                  height: 112,
                  decoration: BoxDecoration(
                      color: headline.tone.tint, shape: BoxShape.circle),
                  child: Builder(builder: (context) {
                    final color = headline.tone == Tone.normal
                        ? const Color(0xFF1F9E63)
                        : headline.tone.main;
                    final icon = Icon(headline.icon, size: 56, color: color);
                    // 테두리만 있는 방패는 가운데가 비어 보여 속이 찬 방패를 작게 하나 더 넣는다 (정인 10/26).
                    if (headline.icon != Icons.shield_outlined) return icon;
                    return Stack(alignment: Alignment.center, children: [
                      icon,
                      Icon(Icons.shield, size: 26, color: color),
                    ]);
                  }),
                ),
                const SizedBox(width: 28),
                Expanded(
                  child: FittedBox(
                    fit: BoxFit.scaleDown,
                    alignment: Alignment.centerLeft,
                    child: SizedBox(
                      width: 530,
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Text(headline.title,
                              style: hs(40,
                                  weight: FontWeight.w600,
                                  height: 1.25,
                                  spacing: -0.01)),
                          const SizedBox(height: 10),
                          Text(headline.body,
                              style: hs(22, color: Hs.t2, height: 1.5)),
                        ],
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      );
}

/// 지표 카드 (조리 온도차 · 공기 질). 값·게이지·다음 기준·「자세히」.
class MetricCard extends StatelessWidget {
  const MetricCard({
    super.key,
    required this.label,
    required this.value,
    required this.unit,
    required this.tone,
    required this.grade,
    required this.fill,
    required this.footer,
    required this.onDetail,
    this.fault = false,
  }) : offline = false;

  const MetricCard.offline({
    super.key,
    required this.label,
    required this.unit,
    required String last,
    required this.onDetail,
  })  : value = '—',
        tone = Tone.idle,
        grade = '수신 없음',
        fill = null,
        footer = last,
        fault = false,
        offline = true;

  final String label;
  final String value;
  final String unit;
  final Tone tone;
  final String grade;
  final double? fill;
  final String footer;
  final VoidCallback onDetail;
  final bool fault;
  final bool offline;

  @override
  Widget build(BuildContext context) {
    final highlight =
        !offline && (fault || tone == Tone.warning || tone == Tone.danger);
    return HsCard(
      padding: const EdgeInsets.fromLTRB(28, 28, 28, 20),
      border: highlight ? tone.main : null,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(label, style: hs(20, color: Hs.t2)),
                    const SizedBox(height: 6),
                    FittedBox(
                      fit: BoxFit.scaleDown,
                      alignment: Alignment.centerLeft,
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.baseline,
                        textBaseline: TextBaseline.alphabetic,
                        children: [
                          Text(
                            value,
                            style: hs(96,
                                weight: FontWeight.w300,
                                height: 1,
                                spacing: -0.03,
                                number: true,
                                color: offline || fault ? Hs.t3 : Hs.t1),
                          ),
                          const SizedBox(width: 8),
                          Text(unit, style: hs(28, color: Hs.t2)),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
              ArcGauge(
                fraction: fill,
                tone: tone,
                label: grade,
                dashed: fault,
                semanticLabel: '$label 게이지, $grade',
              ),
            ],
          ),
          Row(
            children: [
              Expanded(
                child: Text(
                  footer,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: hs(18,
                      color: fault ? Tone.warning.ink : Hs.t2,
                      weight: fault ? FontWeight.w500 : FontWeight.w400),
                ),
              ),
              DetailButton(onPressed: onDetail),
            ],
          ),
        ],
      ),
    );
  }
}

class DetailButton extends StatelessWidget {
  const DetailButton({super.key, required this.onPressed});

  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) => TextButton(
        onPressed: onPressed,
        style: TextButton.styleFrom(
          minimumSize: const Size(0, 48),
          padding: const EdgeInsets.fromLTRB(12, 0, 6, 0),
          foregroundColor: Hs.t1,
          shape:
              RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text('자세히', style: hs(20, weight: FontWeight.w500)),
            const Icon(Icons.chevron_right_rounded, size: 24, color: Hs.t1),
          ],
        ),
      );
}

/// 기기 카드 공통 틀 (높이 162): 아이콘 · 이름·단수·알약 · 막대 · 설명 · 오른쪽 끝.
class _DeviceCard extends StatelessWidget {
  const _DeviceCard({
    required this.label,
    required this.icon,
    required this.level,
    required this.bars,
    required this.barColor,
    required this.note,
    required this.onOpen,
    this.pill,
    this.noteColor = Hs.t2,
    this.trailing,
    this.meter,
  });

  final String label;
  final Widget icon;
  final String level;
  final int bars;
  final Color barColor;
  final String note;
  final Color noteColor;
  final Widget? pill;
  final Widget? trailing;

  /// 단수 막대 대신 그릴 것 (위험 잠금 중 해제 조건 막대).
  final Widget? meter;
  final VoidCallback onOpen;

  @override
  Widget build(BuildContext context) => HsCard(
        onTap: onOpen,
        label: '$label 제어 열기',
        padding: const EdgeInsets.symmetric(horizontal: 28),
        child: SizedBox(
          height: 162,
          child: Row(
            children: [
              icon,
              const SizedBox(width: 24),
              Expanded(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      crossAxisAlignment: CrossAxisAlignment.center,
                      children: [
                        Text(label, style: hs(20, color: Hs.t2)),
                        const SizedBox(width: 14),
                        Text(level,
                            style: hs(40, weight: FontWeight.w600, height: 1)),
                        if (pill != null) ...[
                          const SizedBox(width: 14),
                          pill!,
                        ],
                      ],
                    ),
                    const SizedBox(height: 8),
                    meter ?? LevelBars(level: bars, color: barColor),
                    const SizedBox(height: 8),
                    Text(note,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: hs(18,
                            color: noteColor,
                            weight: noteColor == Hs.t2
                                ? FontWeight.w400
                                : FontWeight.w500)),
                  ],
                ),
              ),
              trailing ??
                  const Icon(Icons.chevron_right_rounded,
                      size: 32, color: Hs.t3),
            ],
          ),
        ),
      );
}

/// 인덕션 카드. 켜기 허용·잠금은 SafetyCore 값(`allowed_on`·`lock`)만 따른다.
class InductionCard extends StatelessWidget {
  const InductionCard({
    super.key,
    required this.state,
    required this.offline,
    required this.onOpen,
    required this.onTurnOn,
  });

  final CoreState? state;
  final bool offline;
  final VoidCallback onOpen;
  final VoidCallback onTurnOn;

  @override
  Widget build(BuildContext context) {
    final s = state;
    if (offline || s == null) {
      return _DeviceCard(
        label: '인덕션',
        icon: const Halo(
            size: 88,
            child:
                Icon(Icons.power_settings_new_rounded, size: 40, color: Hs.t3)),
        level: s == null ? '—' : levelText(s.induction.level),
        pill: s == null
            ? null
            : const SmallPill(
                text: '마지막 확인',
                background: Hs.chip,
                color: Hs.t2,
                pulse: false),
        bars: 0,
        barColor: Hs.t3,
        note: '연결되면 다시 보여 드려요',
        onOpen: onOpen,
        trailing:
            const Icon(Icons.lock_outline_rounded, size: 28, color: Hs.t3),
      );
    }
    final ind = s.induction;
    final lockTone = switch (s.lock) {
      Lock.lockedCat => Tone.pet,
      Lock.lockedDanger => Tone.danger,
      Lock.lockedUser => Tone.idle,
      Lock.unlocked => null,
    };
    if (lockTone != null) {
      final danger = s.lock == Lock.lockedDanger;
      final conditions = releaseConditions(s);
      return _DeviceCard(
        label: '인덕션',
        icon: Halo(
          size: 88,
          background: lockTone.tint,
          child:
              Icon(Icons.lock_outline_rounded, size: 40, color: lockTone.main),
        ),
        level: '잠김',
        bars: ind.level,
        barColor: Tone.warning.main,
        meter: danger ? ConditionBars(conditions) : null,
        note: danger
            ? '잠금 해제 조건 ${conditions.where((c) => c.check == Check.met).length}/3 충족'
                '${(s.lockReleaseLeftS ?? 0) > 0 ? ' · ${s.lockReleaseLeftS}초 남음' : ''}'
            : s.lock == Lock.lockedUser
                ? '기기 제어에서 잠금을 풀 수 있어요'
                : catSeen(s)
                    ? '반려동물이 떠나고 사람이 오면 풀려요'
                    : '사람이 오면 풀려요',
        noteColor: lockTone.ink,
        onOpen: onOpen,
      );
    }
    final on = ind.level > 0;
    return _DeviceCard(
      label: '인덕션',
      icon: Halo(
        size: 88,
        tone: on ? Tone.warning.main : null,
        child: on
            ? LedRing(level: ind.level)
            : const Icon(Icons.power_settings_new_rounded,
                size: 40, color: Hs.t1),
      ),
      level: levelText(ind.level),
      pill: on
          ? SmallPill(
              text: '가열 중',
              background: Tone.warning.tint,
              color: Tone.warning.ink)
          : null,
      bars: ind.level,
      barColor: Tone.warning.main,
      note: !ind.ack
          ? '인덕션 응답 없음'
          : !on
              ? ind.allowedOn
                  ? '탭해서 켜기'
                  : '지금은 켤 수 없어요'
              : s.hazard == Hazard.mid ||
                      s.hazard == Hazard.high ||
                      s.hazard == Hazard.warning
                  ? '불을 조금 줄여 보세요'
                  : '눌러서 단수를 바꿀 수 있어요',
      noteColor: !ind.ack ? Tone.warning.ink : Hs.t2,
      onOpen: onOpen,
      trailing: !on && ind.allowedOn
          ? SizedBox(
              height: 64,
              child: FilledButton(
                onPressed: onTurnOn,
                style: FilledButton.styleFrom(
                  backgroundColor: Hs.t1,
                  padding: const EdgeInsets.symmetric(horizontal: 28),
                  shape: const StadiumBorder(),
                ),
                child: Text('켜기',
                    style:
                        hs(22, weight: FontWeight.w600, color: Colors.white)),
              ),
            )
          : null,
    );
  }
}

/// 후드 카드. 하한(`fan.min`)과 요청(`fan.requested`)은 SafetyCore 값 그대로.
class HoodCard extends StatelessWidget {
  const HoodCard({
    super.key,
    required this.state,
    required this.offline,
    required this.onOpen,
  });

  final CoreState? state;
  final bool offline;
  final VoidCallback onOpen;

  @override
  Widget build(BuildContext context) {
    final s = state;
    if (offline || s == null) {
      return _DeviceCard(
        label: '후드',
        icon: const Halo(size: 88, child: SpinningFan(level: 0, color: Hs.t3)),
        level: s == null ? '—' : levelText(s.fan.level),
        pill: s == null
            ? null
            : const SmallPill(
                text: '마지막 확인',
                background: Hs.chip,
                color: Hs.t2,
                pulse: false),
        bars: 0,
        barColor: Hs.t3,
        note: '연결되면 다시 보여 드려요',
        onOpen: onOpen,
        trailing:
            const Icon(Icons.lock_outline_rounded, size: 28, color: Hs.t3),
      );
    }
    final fan = s.fan;
    final on = fan.level > 0;
    final held = fan.min > 0;
    // 안전 장치가 요청보다 팬을 올렸을 때만 (PM 결정 10/24). 인덕션은 요청 단수가 state에 없어 두지 않는다.
    final auto = fan.level > fan.requested;
    return _DeviceCard(
      label: '후드',
      icon: Halo(
        size: 88,
        tone: on ? Hs.t1 : null,
        child: SpinningFan(level: fan.level),
      ),
      level: levelText(fan.level),
      pill: auto
          ? SmallPill(
              text: '자동 조절 중',
              background: Tone.caution.tint,
              color: Tone.caution.ink)
          : on
              ? const SmallPill(text: '환기 중', background: Hs.chip, color: Hs.t1)
              : null,
      bars: fan.level,
      barColor: Hs.t1,
      note: !fan.ack
          ? '후드 응답 없음'
          : fan.requested < fan.level && held
              ? '요청 ${levelText(fan.requested)} → 안전 최소 ${fan.min}단 유지'
              : held
                  ? '안전을 위해 ${fan.min}단 이상 유지'
                  : on
                      ? '요청한 단수로 운전 중'
                      : '꺼져 있어요',
      noteColor: !fan.ack
          ? Tone.warning.ink
          : held
              ? Tone.caution.ink
              : Hs.t2,
      onOpen: onOpen,
    );
  }
}

/// 마지막 음성 명령 카드. 허가는 SafetyCore가 판정했고 앱은 옮기기만 한다.
class VoiceCard extends StatelessWidget {
  const VoiceCard({
    super.key,
    required this.state,
    required this.voice,
    required this.offline,
    required this.heardFor,
  });

  final CoreState? state;
  final VoiceResult? voice;
  final bool offline;
  final Duration? heardFor;

  @override
  Widget build(BuildContext context) {
    final v = voice;
    final s = state;
    final (icon, label, text, tone) = offline
        ? (
            Icons.mic_off_outlined,
            '음성 명령',
            '연결되면 다시 보여 드려요',
            Tone.idle,
          )
        : v == null
            ? (
                Icons.mic_none_rounded,
                '음성으로 시작하기',
                '“${voiceExamples.first}”',
                Tone.voice,
              )
            : v.accepted
                ? (
                    Icons.mic_none_rounded,
                    '요청을 받았어요',
                    '“${intentLabel(v)}”${s == null ? '' : ' · ${voiceDeviceNow(v, s)}'}',
                    Tone.voice,
                  )
                : (
                    Icons.mic_none_rounded,
                    '마지막 음성 명령',
                    '“${intentLabel(v)}” — ${voiceReasonLabel(v.reason)}',
                    Tone.voice,
                  );
    return HsCard(
      padding: const EdgeInsets.symmetric(horizontal: 32),
      child: Row(
        children: [
          Container(
            width: 72,
            height: 72,
            decoration: BoxDecoration(color: tone.tint, shape: BoxShape.circle),
            child: Icon(icon, size: 34, color: tone.ink),
          ),
          const SizedBox(width: 24),
          Expanded(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(label, style: hs(18, color: Hs.t2)),
                const SizedBox(height: 6),
                Text(text,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: hs(26,
                        weight: FontWeight.w500,
                        color: offline ? Hs.t2 : Hs.t1)),
              ],
            ),
          ),
          if (!offline && v != null && heardFor != null) ...[
            const SizedBox(width: 16),
            Text(ago(heardFor!), style: hs(18, color: Hs.t2)),
          ],
        ],
      ),
    );
  }
}
