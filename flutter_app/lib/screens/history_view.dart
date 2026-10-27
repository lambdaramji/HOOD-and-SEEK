import 'package:flutter/material.dart';

// 기본이 실제 DB(분석·시험이 같은 타입을 보게), 웹만 stub.
import '../data/history_db.dart'
    if (dart.library.js_interop) '../data/history_db_stub.dart';
import '../widgets/hs.dart';
import 'history_log.dart';
import 'labels.dart';
import 'sensor_view.dart';

/// 기록 화면 본문 (1824×776, UI 설계 v1 §3.3, 배치 #156 재범). 왼쪽 1050 열은 조리 리듬(A, 590)과
/// 지켜 준 기록(⑥, 162), 오른쪽은 필터 타임라인(590)과 후드 케어(C, 162). 홈 카메라 카드·인덕션
/// 카드와 같은 줄이다. A·⑥·타임라인은 앱을 켠 뒤 받은 state로 만든 앱 메모리 기록이고, 그 사실은
/// 상단 바에 한 번 밝힌다.
class HistoryView extends StatefulWidget {
  const HistoryView({
    super.key,
    required this.log,
    required this.events,
    required this.now,
    this.hoodCare,
    this.stored = const [],
  });

  /// 기록 DB에 남은 지난 조리 요약(앱을 켜기 전 것). 최신이 앞.
  final List<StoredSession> stored;

  final HistoryLog log;
  final List<HsEvent> events;
  final DateTime now;

  /// 후드 케어 카드. 없으면 타임라인이 오른쪽을 다 쓴다.
  final Widget? hoodCare;

  static const left = 1050.0;
  static const top = 590.0;

  /// 조리 리듬 높이. 띠 세 줄 아래가 비어 줄이고, 그만큼 지켜 준 기록을 2×2로 키운다 (정인 10/26).
  static const rhythm = 440.0;

  @override
  State<HistoryView> createState() => _HistoryViewState();
}

class _HistoryViewState extends State<HistoryView> {
  /// null = 전체.
  EventKind? _filter;

  @override
  Widget build(BuildContext context) {
    final filter = _filter;
    final events = filter == null
        ? widget.events
        : widget.events.where((e) => e.kind == filter).toList();
    final timeline = EventList(
      events: events,
      rows: null,
      title: '최근 일어난 일',
      action: _Filter(
        value: filter,
        onChanged: (kind) => setState(() => _filter = kind),
      ),
    );
    return Row(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        SizedBox(
          width: HistoryView.left,
          child: Column(
            children: [
              SizedBox(
                  height: HistoryView.rhythm,
                  child: _RhythmCard(
                      log: widget.log, now: widget.now, stored: widget.stored)),
              const SizedBox(height: 24),
              Expanded(child: _GuardCard(log: widget.log)),
            ],
          ),
        ),
        const SizedBox(width: 24),
        Expanded(
          child: widget.hoodCare == null
              ? timeline
              : Column(
                  children: [
                    SizedBox(height: HistoryView.top, child: timeline),
                    const SizedBox(height: 24),
                    Expanded(child: widget.hoodCare!),
                  ],
                ),
        ),
      ],
    );
  }
}

class _Filter extends StatelessWidget {
  const _Filter({required this.value, required this.onChanged});

  final EventKind? value;
  final ValueChanged<EventKind?> onChanged;

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.all(4),
        decoration: BoxDecoration(
            color: Hs.bg, borderRadius: BorderRadius.circular(22)),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            for (final (label, kind) in const [
              ('전체', null),
              ('안전·잠금', EventKind.safety),
              ('음성', EventKind.voice),
            ])
              Semantics(
                selected: kind == value,
                child: SizedBox(
                  height: 36,
                  child: TextButton(
                    onPressed: () => onChanged(kind),
                    style: TextButton.styleFrom(
                      backgroundColor:
                          kind == value ? Hs.card : Colors.transparent,
                      padding: const EdgeInsets.symmetric(horizontal: 16),
                      shape: const StadiumBorder(),
                    ),
                    child: Text(label,
                        style: hs(16,
                            weight: FontWeight.w500,
                            color: kind == value ? Hs.t1 : Hs.t2)),
                  ),
                ),
              ),
          ],
        ),
      );
}

/// 「오후 2:02부터 조리 중 · 6분」, 끝난 조리는 「오후 2:02 ~ 오후 2:31」. 시작·끝이 같은 분이면
/// 「오후 2:02 · 6초 조리」.
/// DB에 남은 지난 조리 한 줄. 다른 날 것일 수 있어 날짜를 앞에 붙인다.
String _storedLine(StoredSession s) {
  final day = '${s.start.month}/${s.start.day}';
  return '$day ${clock(s.start)} ~ ${clock(s.end)} · ${s.summary}';
}

String _span(CookSession s, DateTime now) {
  final length = duration(s.length(now));
  if (s.active) return '${clock(s.start)}부터 조리 중 · $length';
  return clock(s.start) == clock(s.end!)
      ? '${clock(s.start)} · $length 조리'
      : '${clock(s.start)} ~ ${clock(s.end!)}';
}

/// A. 조리 리듬: 진행 중인 1회(없으면 최근 끝난 1회)를 단계·후드·연기 띠 3줄과 핀으로.
class _RhythmCard extends StatelessWidget {
  const _RhythmCard(
      {required this.log, required this.now, this.stored = const []});

  final HistoryLog log;
  final DateTime now;
  final List<StoredSession> stored;

  @override
  Widget build(BuildContext context) {
    final session = log.shown;
    // 지난 조리 한 줄씩: 앱을 켠 뒤 끝난 것, 그다음 DB에 남은 앱을 켜기 전 것.
    final older = [
      for (final s in log.recent)
        if (!identical(s, session))
          '${_span(s, now)} · ${s.chips(now).skip(1).join(' · ')}',
      for (final s in stored) _storedLine(s),
    ];
    final storedLines = [for (final s in stored) _storedLine(s)];
    return HsCard(
      padding: const EdgeInsets.fromLTRB(32, 24, 32, 24),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text('조리 리듬', style: hs(22, weight: FontWeight.w500)),
              const Spacer(),
              if (session != null)
                Text(_span(session, now), style: hs(18, color: Hs.t2)),
            ],
          ),
          if (session == null && storedLines.isEmpty)
            Expanded(
              child: Center(
                  child: Text('아직 기록이 없어요', style: hs(20, color: Hs.t2))),
            )
          else if (session == null) ...[
            const SizedBox(height: 16),
            Text('지난 조리 · 기기에 저장', style: hs(18, color: Hs.t2)),
            const SizedBox(height: 8),
            // 카드(440)에 들어가는 만큼만 보이고 나머지는 스크롤 — 리허설·E2E로 금방 10회가 넘는다 (#170 PM 리뷰).
            Expanded(
              child: ListView(
                padding: EdgeInsets.zero,
                children: [
                  for (final line in storedLines.take(20))
                    Padding(
                      padding: const EdgeInsets.only(top: 6),
                      child: Text(line,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: hs(18, color: Hs.t1)),
                    ),
                ],
              ),
            ),
          ] else ...[
            const SizedBox(height: 20),
            Expanded(child: _Rhythm(session: session, now: now)),
            const SizedBox(height: 20),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                for (final chip in session.chips(now))
                  Container(
                    padding:
                        const EdgeInsets.symmetric(horizontal: 14, vertical: 7),
                    decoration: BoxDecoration(
                        color: Hs.bg, borderRadius: BorderRadius.circular(18)),
                    child: Text(chip,
                        style: hs(17, weight: FontWeight.w500, color: Hs.t1)),
                  ),
              ],
            ),
            // 조리 중이면 띠 세 줄·시간 줄이 칩과 겹치지 않게 지난 조리는 한 줄만 (정인 10/26 사진).
            if (older.isNotEmpty) const SizedBox(height: 8),
            for (final line in older.take(1))
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(line,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: hs(16, color: Hs.t2)),
              ),
          ],
        ],
      ),
    );
  }
}

class _Rhythm extends StatelessWidget {
  const _Rhythm({required this.session, required this.now});

  final CookSession session;
  final DateTime now;

  static const _label = 64.0;
  // 범례 글자가 다 보이게(LG 글꼴 포함) 넓히고 띠를 그만큼 줄인다 (정인 10/26).
  static const _legend = 320.0;

  @override
  Widget build(BuildContext context) {
    final total = session.length(now).inMilliseconds;
    // 음성 핀은 띠 위에서 정신없어 뺀다. 음성 기록은 타임라인 「음성」과 지켜 준 기록에 그대로 있다 (정인 10/26).
    final pins = [
      for (final p in session.pins)
        if (p.kind != PinKind.voiceAccepted && p.kind != PinKind.voiceRejected)
          p,
    ];
    double at(DateTime t) => total <= 0
        ? 0
        : (t.difference(session.start).inMilliseconds / total).clamp(0.0, 1.0);
    Widget dot(Color color) => Container(
        width: 10,
        height: 10,
        decoration: BoxDecoration(color: color, shape: BoxShape.circle));
    Widget legend(List<(String, Color)> items) => SizedBox(
          width: _legend,
          child: Padding(
            padding: const EdgeInsets.only(left: 16),
            child: Wrap(
              spacing: 12,
              runSpacing: 4,
              children: [
                for (final (name, color) in items)
                  Row(mainAxisSize: MainAxisSize.min, children: [
                    dot(color),
                    const SizedBox(width: 6),
                    Text(name, style: hs(15, color: Hs.t2)),
                  ]),
              ],
            ),
          ),
        );
    Widget band(String name, _Band kind, List<(String, Color)> items) =>
        SizedBox(
          height: 48,
          child: Row(
            children: [
              SizedBox(
                  width: _label,
                  child: Text(name, style: hs(16, color: Hs.t2))),
              Expanded(
                child: ClipRRect(
                  borderRadius: BorderRadius.circular(8),
                  child: CustomPaint(
                    painter: _BandPainter(session.samples, kind),
                    child: const SizedBox.expand(),
                  ),
                ),
              ),
              legend(items),
            ],
          ),
        );
    return Column(
      children: [
        SizedBox(
          height: 44,
          child: Row(
            children: [
              const SizedBox(width: _label),
              Expanded(
                child: LayoutBuilder(
                  builder: (context, box) => Stack(
                    clipBehavior: Clip.none,
                    children: [
                      for (final pin in pins)
                        if (pin.kind == PinKind.absent)
                          Positioned(
                            left: box.maxWidth * at(pin.at),
                            width: box.maxWidth *
                                (pin.length.inMilliseconds /
                                        (total <= 0 ? 1 : total))
                                    .clamp(0.0, 1.0),
                            bottom: 0,
                            height: 6,
                            child: ColoredBox(color: Tone.warning.tint),
                          ),
                      // 글자 폭(약 64 px)보다 가까운 핀은 아이콘만 둔다 — 글자가 겹치지 않게.
                      for (final (pin, labeled)
                          in _spaced(pins, (p) => box.maxWidth * at(p.at)))
                        Positioned(
                          left: box.maxWidth * at(pin.at) - 32,
                          width: 64,
                          top: 0,
                          child: Column(
                            children: [
                              Icon(_pinIcon(pin.kind),
                                  size: 22, color: _pinTone(pin.kind).main),
                              if (labeled)
                                Text(_pinLabel(pin.kind),
                                    maxLines: 1,
                                    overflow: TextOverflow.visible,
                                    softWrap: false,
                                    style:
                                        hs(13, color: _pinTone(pin.kind).ink)),
                            ],
                          ),
                        ),
                    ],
                  ),
                ),
              ),
              const SizedBox(width: _legend),
            ],
          ),
        ),
        const SizedBox(height: 8),
        // 범례는 기록과 상관없이 늘 같은 항목·순서 (정인 10/26).
        // 띠 색은 Core가 잰 열 단계(temperatureGrade)라 인덕션 단수가 아니다 — 「화력」 (#170 PM 리뷰).
        band('화력', _Band.grade, [
          for (final (grade, tone) in _gradeLegend)
            (grade, _gradeColor(grade, tone)),
        ]),
        const SizedBox(height: 6),
        band('후드', _Band.fan, [
          for (var level = 1; level <= 3; level++)
            ('$level단', _fanColor(level)),
          ('자동 상향 (빗금)', Hs.t3),
        ]),
        const SizedBox(height: 6),
        band('연기', _Band.smoke, [
          ('연기 있음', Tone.caution.main),
          ('연기 많음', Tone.warning.main),
        ]),
        const SizedBox(height: 6),
        Row(
          children: [
            const SizedBox(width: _label),
            Text(clock(session.start), style: hs(15, color: Hs.t3)),
            const Spacer(),
            Text(session.active ? '지금' : clock(session.end!),
                style: hs(15, color: Hs.t3)),
            const SizedBox(width: _legend),
          ],
        ),
        const Spacer(),
      ],
    );
  }

  /// 시각 순서대로, 앞 핀 글자와 64 px 안이면 글자를 달지 않는다.
  static List<(RhythmPin, bool)> _spaced(
      List<RhythmPin> pins, double Function(RhythmPin) x) {
    final sorted = [...pins]..sort((a, b) => a.at.compareTo(b.at));
    final out = <(RhythmPin, bool)>[];
    double? last;
    for (final pin in sorted) {
      final px = x(pin);
      final labeled = last == null || px - last >= 64;
      if (labeled) last = px;
      out.add((pin, labeled));
    }
    return out;
  }

  static IconData _pinIcon(PinKind kind) => switch (kind) {
        PinKind.absent => Icons.person_off_outlined,
        PinKind.voiceAccepted => Icons.mic_none_rounded,
        PinKind.voiceRejected => Icons.mic_off_outlined,
        PinKind.dangerLock => Icons.lock_outline_rounded,
        PinKind.cutoff => Icons.power_settings_new_rounded,
      };

  static String _pinLabel(PinKind kind) => switch (kind) {
        PinKind.absent => '자리 비움',
        PinKind.voiceAccepted => '음성',
        PinKind.voiceRejected => '음성 거부',
        PinKind.dangerLock => '위험 잠금',
        PinKind.cutoff => '위험 차단',
      };

  static Tone _pinTone(PinKind kind) => switch (kind) {
        PinKind.absent => Tone.warning,
        PinKind.voiceAccepted => Tone.voice,
        PinKind.voiceRejected ||
        PinKind.dangerLock ||
        PinKind.cutoff =>
          Tone.danger,
      };
}

enum _Band { grade, fan, smoke }

/// 띠 한 줄. 표본 하나가 같은 폭을 차지하고, 끊긴 표본(null)은 비워 둔다.
class _BandPainter extends CustomPainter {
  _BandPainter(this.samples, this.kind);

  final List<RhythmSample?> samples;
  final _Band kind;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, Paint()..color = Hs.track);
    if (samples.isEmpty) return;
    final w = size.width / samples.length;
    for (var i = 0; i < samples.length; i++) {
      final s = samples[i];
      if (s == null) continue;
      final x = i * w;
      switch (kind) {
        case _Band.grade:
          canvas.drawRect(Rect.fromLTWH(x, 0, w + 0.5, size.height),
              Paint()..color = _gradeColor(s.grade, s.tone));
        case _Band.fan:
          if (s.fan == 0) continue;
          canvas.drawRect(Rect.fromLTWH(x, 0, w + 0.5, size.height),
              Paint()..color = _fanColor(s.fan));
        case _Band.smoke:
          if (s.smoke == 0) continue;
          canvas.drawRect(
              Rect.fromLTWH(x, 0, w + 0.5, size.height),
              Paint()
                ..color = s.smoke == 2 ? Tone.warning.main : Tone.caution.main);
      }
    }
    // Core가 안전 하한으로 올린 구간(fan.level > requested)은 색이 아니라 빗금으로.
    // 이어지는 칸을 한 구간으로 묶어 한 번만 그린다 — 30분이면 3,600칸이라 (#170 PM 리뷰).
    if (kind == _Band.fan) {
      final hatch = Paint()
        ..color = Colors.white.withValues(alpha: 0.7)
        ..strokeWidth = 2;
      int? from;
      for (var i = 0; i <= samples.length; i++) {
        final auto = i < samples.length && (samples[i]?.auto ?? false);
        if (auto) {
          from ??= i;
          continue;
        }
        if (from == null) continue;
        final rect =
            Rect.fromLTWH(from * w, 0, (i - from) * w + 0.5, size.height);
        canvas.save();
        canvas.clipRect(rect);
        for (var hx = (rect.left / 8).floor() * 8.0 - size.height;
            hx < rect.right + 1;
            hx += 8) {
          canvas.drawLine(
              Offset(hx, size.height), Offset(hx + size.height, 0), hatch);
        }
        canvas.restore();
        from = null;
      }
    }
  }

  // 진행 중인 조리는 같은 List에 표본이 붙으므로 매번 다시 그린다 (0.5 s 간격).
  @override
  bool shouldRepaint(_BandPainter oldDelegate) => true;
}

/// ⑥ 지켜 준 기록: 사용자를 지켜 준 일 4칸을 크게, 센서·연결은 제목 옆 상태 칩 하나로.
/// 판정이 아니라 Core가 한 일을 앱을 켠 뒤 받은 state로 센 것이다.
class _GuardCard extends StatelessWidget {
  const _GuardCard({required this.log});

  final HistoryLog log;

  @override
  Widget build(BuildContext context) {
    Duration avg(List<Duration> d) => Duration(
        milliseconds:
            d.fold(0, (sum, x) => sum + x.inMilliseconds) ~/ d.length);
    final tiles = [
      (
        Icons.lock_outline_rounded,
        '위험 잠금',
        '${log.dangerLocks}번',
        log.dangerReleases.isEmpty
            ? (log.dangerLocks == 0 ? '위험 차단이 없었어요' : '해제를 기다리는 중')
            : '확인과 재실 뒤에만 풀렸어요 · 평균 ${duration(avg(log.dangerReleases))}',
      ),
      (
        Icons.person_off_outlined,
        '자리 비움 보호',
        '${log.absentGuards}번',
        log.absentGuards == 0
            ? '자리를 비운 동안 위험이 없었어요'
            : '총 ${duration(log.absentGuardTime)} 후드·화력을 자동 조절했어요',
      ),
      // 칸은 늘 네 개, 없으면 0번 (정인 10/26 — #156 「있을 때만」을 바꿈).
      (
        Icons.pets_rounded,
        '반려동물 잠금',
        '${log.catLocks}번',
        log.catLocks == 0 ? '반려동물이 잠금을 건 일이 없었어요' : '사람이 안 보일 때 반려동물이 보여 잠갔어요',
      ),
      (
        Icons.mic_none_rounded,
        '음성 명령',
        '수락 ${log.voiceAccepted}번 · 거부 ${log.voiceRejected}번',
        // 계약 voice에 순번이 없다 (PM 설계 10/26 ⑥).
        '같은 결과가 연달아 오면 한 번으로 셉니다',
      ),
    ];
    return HsCard(
      padding: const EdgeInsets.fromLTRB(32, 18, 32, 18),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text('지켜 준 기록', style: hs(22, weight: FontWeight.w500)),
              const Spacer(),
              if (log.samples > 0) _status(),
            ],
          ),
          const SizedBox(height: 10),
          // 2×2. 기록이 없어도 칸은 늘 보이고 0번이다 (정인 10/26).
          Expanded(
            child: Column(
              children: [
                for (var row = 0; row < 2; row++) ...[
                  if (row > 0) const Divider(height: 17, color: Hs.line),
                  Expanded(
                    child: Row(
                      children: [
                        Expanded(child: _tile(tiles[row * 2])),
                        const VerticalDivider(
                            width: 33, thickness: 1, color: Hs.line),
                        Expanded(child: _tile(tiles[row * 2 + 1])),
                      ],
                    ),
                  ),
                ],
              ],
            ),
          ),
        ],
      ),
    );
  }

  /// 센서·연결 상태 칩. 이상이 있었을 때만 무엇이 몇 번인지 적는다.
  Widget _status() {
    final sensor = (log.sensorOk * 100 / log.samples).floor();
    final issues = [
      if (sensor < 100) '센서 정상 $sensor %',
      if (log.disconnects > 0) '연결 복구 ${log.recoveries.length}번',
      if (log.coreRestarts > 0) '안전 장치 재시작 ${log.coreRestarts}번',
      if (log.noAcks > 0) '기기 응답 없음 ${log.noAcks}번',
    ];
    final ok = issues.isEmpty;
    final tone = ok ? Tone.normal : Tone.caution;
    return SmallPill(
      text: ok ? '센서·연결 이상 없음' : issues.join(' · '),
      background: tone.tint,
      color: tone.ink,
      pulse: false,
    );
  }

  Widget _tile((IconData, String, String, String) t) {
    final (icon, name, value, detail) = t;
    return Row(
      children: [
        Icon(icon, size: 28, color: Hs.t2),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(name, style: hs(15, color: Hs.t2, height: 1.2)),
              Text(value,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: hs(24, weight: FontWeight.w500, height: 1.25)),
              Text(detail,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: hs(13, color: Hs.t3, height: 1.2)),
            ],
          ),
        ),
      ],
    );
  }
}

/// 조리 리듬 띠·범례 색. 강불은 중불과 같은 주의 톤이라(D-059) 띠에서 갈리도록 진한 쪽을 쓴다.
Color _gradeColor(String grade, Tone tone) =>
    grade == '강불' ? Tone.caution.ink : tone.main;

/// 화력 띠 범례. 기록에 없던 단계도 늘 같은 순서로 보인다 (정인 10/26). 색은 [temperatureGrade]와 같다.
const _gradeLegend = [
  ('약불', Tone.normal),
  ('중불', Tone.caution),
  ('강불', Tone.caution),
  ('과열', Tone.warning),
  ('위험', Tone.danger),
];

/// 후드 띠 색. 1·2·3단을 인덕션 약불·중불·강불 색과 맞춘다 (정인 10/26).
Color _fanColor(int level) => switch (level) {
      1 => _gradeColor('약불', Tone.normal),
      2 => _gradeColor('중불', Tone.caution),
      _ => _gradeColor('강불', Tone.caution),
    };
