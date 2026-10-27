import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/material.dart';

import '../audio/clip_player.dart';
import '../brand.dart';
import '../data/core_state.dart';
// 기본이 실제 DB(분석·시험이 같은 타입을 보게), 웹만 stub.
import '../data/history_db.dart'
    if (dart.library.js_interop) '../data/history_db_stub.dart';
import '../data/hood_care.dart';
import '../data/hood_care_store_stub.dart'
    if (dart.library.io) '../data/hood_care_store.dart';
import '../data/kitchen_feed.dart';
import '../data/ui_command.dart';
import '../widgets/hs.dart';
import 'history_log.dart';
import 'history_view.dart';
import 'hood_care_card.dart';
import 'home_view.dart';
import 'labels.dart';
import 'overlays.dart';
import 'sensor_view.dart';

/// SafetyCore가 확정한 state를 그대로 보여 준다. 모든 색·문구는 state 값 매핑이고
/// 임계값 비교·잠금 해제 판단은 앱에 두지 않는다 (설계서 §5.4).
///
/// 화면은 시안 크기(1920×1080)로 그린 뒤 실제 화면에 맞춰 줄이거나 늘린다.
/// 기기의 픽셀 배율이 무엇이든 15.8" 1080p 시연 화면에서 시안과 같은 배치가 된다.
class DashboardScreen extends StatefulWidget {
  const DashboardScreen({
    super.key,
    this.feed,
    this.brand = BrandImages.none,
    this.clips = const SilentClipPlayer(),
    this.video,
    this.careStore,
    this.historyDb,
  });

  /// 기록 DB(Pi 5 SQLite 파일 하나). 없으면 기본 경로(`HISTORY_DB`)를 열어 보고, 못 열면 메모리로만 센다.
  final HistoryDb? historyDb;

  final KitchenFeed? feed;

  /// 기기 폴더에서 읽은 LG 로고·시작 화면 배경. 없으면 로고 없이 그린다.
  final BrandImages brand;

  /// 경고음·기동음·Core 상태 반영 뒤의 조작 확인음.
  final ClipPlayer clips;

  /// 카메라 카드 영상(JPEG 한 장씩). 표시 전용이고 판정에 쓰지 않는다 (설계 v1 §3.2). 없으면 그림만.
  final Stream<Uint8List>? video;

  /// 후드 케어 누적을 읽고 쓰는 곳. 없으면 기본 경로(`HOOD_CARE_FILE`), 웹은 저장 없음.
  final HoodCareStore? careStore;

  /// 후드 케어 누적을 파일에 쓰는 간격 (PM 설계 10/26 C).
  static const careSaveEvery = Duration(seconds: 60);

  /// state를 이 시간 동안 못 받으면 「연결 끊김」 (계약 §2.4).
  static const staleAfter = Duration(seconds: 3);

  /// 시작 화면: 최소 노출 시간과, 첫 state를 이만큼 못 받으면 「연결 끊김」 (설계 v1 §5.3).
  static const splashMin = Duration(milliseconds: 1500);
  static const bootTimeout = Duration(seconds: 8);

  /// 그래프 표본 간격과 최대 개수: 0.5 s × 7200 = 최근 1시간.
  static const trendPeriod = Duration(milliseconds: 500);
  static const trendPoints = 7200;

  /// 음성 결과를 띄워 두는 시간.
  static const voiceShown = Duration(seconds: 2);

  /// 거부 사유는 읽을 시간이 더 필요해 조금 더 둔다 (설계 v1 §6은 수락 2 s만 정함).
  static const voiceRejectedShown = Duration(seconds: 3);

  /// 위험 잠금이 풀린 뒤 「다시 쓸 수 있어요」를 보이는 시간과 복구 토스트 시간.
  static const recoveredShown = Duration(seconds: 10);
  static const toastShown = Duration(seconds: 3);

  static const version = '0.4';

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

enum _Tab { home, sensors, history }

/// 설계 v1 §4.3 모달. 우선순위 순서(배지 §4.1과 같음)이고 한 번에 하나만 띄운다.
/// 재실 중 온도 경고(M3)는 모달 없이 카드만 바뀐다.
enum _Alert { fault, release, danger, cat, absent }

class _DashboardScreenState extends State<DashboardScreen> {
  late final KitchenFeed _feed = widget.feed ?? DemoKitchenFeed();
  StreamSubscription<CoreState>? _subscription;
  Timer? _staleTimer;
  Timer? _trendTimer;
  CoreState? _state;
  DateTime? _lastAt;
  bool _stale = false;
  String? _rehearsalId;

  _Tab _tab = _Tab.home;
  TrendRange _range = TrendRange.s60;
  bool _sheet = false;

  /// 홈 카드에서 열면 그 기기만, 도크 「기기」면 둘 다 (정인 10/26).
  DevicePart _sheetPart = DevicePart.both;

  /// 사용자가 닫은 경고. 조건이 사라지면 지워서 다음 발생 때 다시 띄운다.
  final Set<_Alert> _dismissed = {};

  VoiceResult? _voiceShown;
  DateTime? _voiceAt;
  // ponytail: ui_cmd에 요청별 결과 ID가 없어, 최신 로컬 요청과 state·ACK 일치만 본다.
  // 정확한 요청별 수락을 알려야 할 때는 Core 결과 식별자 계약이 필요하다.
  UiCommand? _pendingTouch;
  int? _touchLevel;

  /// 수락된 음성 명령. Core 최종 단수·ACK가 맞으면 결과 안내(#173)를 한 번 내고 지운다.
  /// 같은 ponytail: 계약 voice에 요청 ID가 없어 최신 수락 하나만 본다.
  VoiceResult? _pendingVoice;
  int _voiceFromInduction = 0;
  DateTime? _absentSince;
  DateTime? _thermalLostSince;
  bool _recovered = false;
  String? _toastText;
  bool _splash = true;

  /// 잠깐 띄우는 것들(시작 화면·음성 결과·토스트·복구 헤드라인)은 시각 비교 대신
  /// 타이머로 끈다. 표시용 경과 시간(「42초」 등)만 시계를 읽는다.
  final Map<String, Timer> _timers = {};

  void _after(String key, Duration d, VoidCallback done) {
    _timers[key]?.cancel();
    _timers[key] = Timer(d, () {
      if (mounted) setState(done);
    });
  }

  /// 그래프 값. state 도착 간격(2 Hz + 변화 즉시)과 무관하게 [DashboardScreen.trendPeriod]마다
  /// 마지막 값을 찍는다. 값이 없거나(`valid=false`) 끊긴 동안은 null — 선이 끊긴다.
  final List<double?> _metricTrend = [];
  final List<double?> _pmTrend = [];
  final List<HsEvent> _events = [];

  /// 기록 탭의 조리 리듬·지켜 준 기록. 앱을 켠 뒤 받은 state만 메모리에 둔다 (UI 설계 v1 §3.3).
  final HistoryLog _history = HistoryLog(DateTime.now());

  /// 기록 DB. 타임라인 한 줄과 끝난 조리 요약을 남기고, 켤 때 지난 것을 읽어 온다.
  late final HistoryDb? _db = widget.historyDb ?? HistoryDb.open();
  List<StoredSession> _storedSessions = const [];

  static const _toneNames = {
    'normal': Tone.normal,
    'caution': Tone.caution,
    'warning': Tone.warning,
    'danger': Tone.danger,
    'voice': Tone.voice,
    'pet': Tone.pet,
    'idle': Tone.idle,
  };

  /// 후드 케어(C). 파일을 다 읽기 전에는 세지 않는다 — 읽은 값을 덮어쓰지 않게.
  late final HoodCareStore _careStore = widget.careStore ?? HoodCareStore();
  HoodCare _care = HoodCare();
  bool _careLoaded = false;
  String? _careNotice;
  Timer? _careTimer;

  @override
  void initState() {
    super.initState();
    widget.clips.play('APP_START');
    _loadHistory();
    _subscription = _feed.states.listen(_onState);
    // 재접속 시도와 무관하게 마지막 state 도착 시각에서만 잰다.
    _armStaleTimer(DashboardScreen.bootTimeout);
    _after('splash', DashboardScreen.splashMin, () => _splash = false);
    _trendTimer =
        Timer.periodic(DashboardScreen.trendPeriod, (_) => _sampleTrend());
    unawaited(_loadCare());
    _careTimer = Timer.periodic(
        DashboardScreen.careSaveEvery, (_) => unawaited(_saveCare()));
    unawaited(_feed.connect());
  }

  void _loadHistory() {
    final db = _db;
    if (db == null) return;
    for (final e in db.recentEvents(50).reversed) {
      _events.insert(
          0,
          HsEvent(e.at, _toneNames[e.tone] ?? Tone.idle, e.text, e.trailing,
              kind: EventKind.values.asNameMap()[e.kind] ?? EventKind.other));
    }
    _storedSessions = db.recentSessions(20);
    _history.onFinished = (s) => db.addSession(
        StoredSession(s.start, s.end!, s.chips(s.end!).skip(1).join(' · ')));
  }

  Future<void> _loadCare() async {
    final (care, notice) = await _careStore.load();
    if (!mounted) return;
    setState(() {
      _care = care;
      _careNotice = notice;
      _careLoaded = true;
    });
  }

  Future<void> _saveCare() async {
    if (!_careLoaded) return;
    final saved = await _careStore.save(_care);
    if (!mounted || saved || _careNotice == hoodCareSaveFailed) return;
    setState(() => _careNotice = hoodCareSaveFailed);
  }

  void _cleaned() {
    setState(() => _care.clean(DateTime.now()));
    unawaited(_saveCare());
  }

  void _sampleTrend() {
    final state = _stale ? null : _state;
    setState(() {
      final now = DateTime.now();
      _history.sample(state, now, DashboardScreen.trendPeriod);
      if (_careLoaded) _care.sample(state, DashboardScreen.trendPeriod, now);
      for (final (trend, value) in [
        (_metricTrend, state?.thermal.metric),
        (_pmTrend, state?.pm.pm25),
      ]) {
        trend.add(value);
        if (trend.length > DashboardScreen.trendPoints) trend.removeAt(0);
      }
    });
  }

  void _onState(CoreState next) {
    // 첫 state부터 3 s로 잰다. 8 s는 아직 하나도 못 받았을 때만이다.
    _armStaleTimer(DashboardScreen.staleAfter);
    final prev = _state;
    final now = DateTime.now();
    if (next.alert != Alert.none) {
      _clearTouchFeedback();
      _clearVoiceResult();
    }
    // 같은 alert가 2 Hz로 다시 와도 한 번만 낸다. Core는 alert를 1 s 뒤 NONE으로
    // 돌리므로 같은 경고가 새로 생기면 다시 바뀐 것으로 보인다 (contracts/README 「클립 ID」).
    if (next.alert != Alert.none && next.alert != prev?.alert) {
      widget.clips.play(next.alert.wire);
    }
    setState(() {
      // 켤 때 첫 state가 8 s 넘게 안 와 「연결 끊김」이었어도 복구로 센다 (#158 리뷰).
      if (_stale) _history.recovered(now);
      if (prev == null) {
        _log(now, Tone.normal, '안전 장치 연결 — 모니터링 시작', '모니터링', EventKind.safety);
      } else {
        if (_stale) {
          _log(now, Tone.normal, '다시 연결됨', '상태 받는 중', EventKind.safety);
        }
        _compare(prev, next, now);
      }
      _history.state(prev, next, now);
      _absentSince =
          next.presence == Presence.absent ? (_absentSince ?? now) : null;
      _thermalLostSince =
          next.thermal.valid ? null : (_thermalLostSince ?? now);
      _dismissed.removeWhere((alert) => !_wanted(alert, next));
      _state = next;
      _lastAt = now;
      _stale = false;
    });
  }

  /// 앞뒤 state를 비교해 기록·음성 결과·잠금 해제 알림을 남긴다. 판정이 아니라 변화 표시다.
  void _compare(CoreState prev, CoreState next, DateTime now) {
    final touch = _pendingTouch;
    if (touch != null) {
      final changed = switch (touch.cmd) {
        UiCmd.fanSet => next.fan.requested == touch.value &&
            next.fan.level == touch.value &&
            next.fan.ack &&
            next.fan.level != _touchLevel,
        UiCmd.indSet => next.induction.level == touch.value &&
            next.induction.ack &&
            next.induction.level != _touchLevel,
        _ => false,
      };
      if (changed) {
        _clearTouchFeedback();
        if (next.alert == Alert.none) widget.clips.play('TOUCH_FEEDBACK');
      }
    }
    final fan = '후드 ${levelText(next.fan.level)}';
    if (prev.hazard != next.hazard) {
      final top = next.reasons.isEmpty
          ? null
          : reasonLabel(next.reasons.first) ?? next.reasons.first;
      _log(
          now,
          hazardTone(next.hazard),
          // D-054: 중불·강불도 정상 조리다. low(약불·대기)로 바뀐 것을 「복구」라 부르지 않는다 (#158 PM 리뷰).
          next.hazard == Hazard.low
              ? '정상 · ${top ?? (next.induction.level > 0 ? '약불' : '대기')}'
              : '${hazardLabel(next.hazard)} — ${top ?? '사유 없음'}',
          fan,
          EventKind.safety);
    }
    if (prev.lock != next.lock) {
      _log(
          now,
          switch (next.lock) {
            Lock.unlocked => Tone.normal,
            Lock.lockedCat => Tone.pet,
            Lock.lockedDanger => Tone.danger,
            Lock.lockedUser => Tone.idle,
          },
          switch (next.lock) {
            Lock.lockedDanger => '위험 잠금 작동',
            Lock.lockedCat => '반려동물 잠금 작동',
            Lock.lockedUser => '사용자 잠금 작동',
            Lock.unlocked => '잠금 해제됨',
          },
          inductionText(next.induction),
          EventKind.safety);
      if (prev.lock == Lock.lockedDanger && next.lock == Lock.unlocked) {
        _recovered = true;
        _after('recovered', DashboardScreen.recoveredShown,
            () => _recovered = false);
        _toast('확인했어요 · 인덕션을 다시 쓸 수 있어요');
      }
    }
    if (prev.alert != next.alert && next.alert == Alert.offlineRecover) {
      _toast('기기 연결이 복구됐어요');
    }
    if (prev.presence != next.presence) {
      _log(now, Tone.idle, presenceLabel(next.presence), '');
    }
    if (prev.induction.level != next.induction.level) {
      _log(now, Tone.warning, inductionText(next.induction), fan);
    }
    final voice = next.voice;
    // ponytail: 계약의 voice에는 순번이 없어 같은 결과가 연달아 오면 한 번으로 보인다.
    // 구분이 필요해지면 계약에 seq를 더한다(동결 뒤 PM 승인 필요).
    if (voice != null && !_sameVoice(prev.voice, voice)) {
      _voiceShown = voice;
      _voiceAt = now;
      _after('voice', _voiceFor(voice), () => _voiceShown = null);
      _history.voice(voice, now);
      _clearVoiceResult();
      if (voice.accepted) {
        _pendingVoice = voice;
        _voiceFromInduction = prev.induction.level;
        _after('voiceResult', DashboardScreen.staleAfter, _clearVoiceResult);
      } else if (voice.lastIntent == 'NONE' &&
          voice.reason == 'NO_INTENT' &&
          next.alert == Alert.none) {
        widget.clips.play('results/RESULT_UNSUPPORTED');
      }
      _log(
          now,
          voice.accepted ? Tone.voice : Tone.danger,
          '음성 “${intentLabel(voice)}”',
          voice.accepted ? '실행됨' : voiceReasonLabel(voice.reason),
          EventKind.voice);
    }
    // 수락 안내가 아니라 확인된 최종 설정을 말한다 — Core 단수·ACK가 맞는 state에서 한 번(#173 PM 결정).
    final pending = _pendingVoice;
    if (pending != null) {
      final clip = voiceResultClip(pending, _voiceFromInduction, next);
      if (clip != null) {
        _clearVoiceResult();
        if (next.alert == Alert.none) widget.clips.play(clip);
      }
    }
  }

  void _clearVoiceResult() {
    _pendingVoice = null;
    _timers.remove('voiceResult')?.cancel();
  }

  static Duration _voiceFor(VoiceResult v) => v.accepted
      ? DashboardScreen.voiceShown
      : DashboardScreen.voiceRejectedShown;

  static bool _sameVoice(VoiceResult? a, VoiceResult b) =>
      a != null &&
      a.lastIntent == b.lastIntent &&
      a.value == b.value &&
      a.accepted == b.accepted &&
      a.reason == b.reason;

  void _toast(String text) {
    _toastText = text;
    _after('toast', DashboardScreen.toastShown, () => _toastText = null);
  }

  void _log(DateTime at, Tone tone, String text, String trailing,
      [EventKind kind = EventKind.other]) {
    _events.insert(0, HsEvent(at, tone, text, trailing, kind: kind));
    final name = _toneNames.entries
        .firstWhere((e) => identical(e.value, tone),
            orElse: () => const MapEntry('idle', Tone.idle))
        .key;
    _db?.addEvent(StoredEvent(at, name, text, trailing, kind.name));
    if (_events.length > 50) _events.removeLast();
  }

  void _armStaleTimer(Duration after) {
    _staleTimer?.cancel();
    _staleTimer = Timer(after, () {
      if (!mounted) return;
      // 끊긴 동안 다시 울리지 않는다. 타이머는 다음 state가 와야 다시 걸린다.
      widget.clips.play('OFFLINE');
      setState(() {
        final now = DateTime.now();
        _stale = true;
        _clearTouchFeedback();
        _clearVoiceResult();
        _voiceShown = null;
        _log(now, Tone.idle, '연결 끊김', '다시 연결 중', EventKind.safety);
        _history.lost(now);
      });
    });
  }

  @override
  void dispose() {
    _staleTimer?.cancel();
    _trendTimer?.cancel();
    _careTimer?.cancel();
    unawaited(_saveCare());
    _db?.close();
    for (final timer in _timers.values) {
      timer.cancel();
    }
    unawaited(_subscription?.cancel());
    unawaited(_feed.close());
    super.dispose();
  }

  Future<void> _send(UiCommand command) async {
    _clearTouchFeedback();
    final state = _stale ? null : _state;
    if (state != null) {
      final level = switch (command.cmd) {
        UiCmd.fanSet when command.value != state.fan.requested =>
          state.fan.level,
        UiCmd.indSet => state.induction.level,
        _ => null,
      };
      if (level != null && level != command.value) {
        _pendingTouch = command;
        _touchLevel = level;
        _after('touch', DashboardScreen.staleAfter, _clearTouchFeedback);
      }
    }
    try {
      await _feed.send(command);
    } on Object catch (error) {
      if (identical(_pendingTouch, command)) _clearTouchFeedback();
      // 화면에는 고정 문구만 (D-047). 원인은 로그로 남긴다.
      debugPrint('ui_cmd 전송 실패: $error');
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('명령을 보내지 못했어요. 연결을 확인해 주세요.')),
      );
    }
  }

  void _clearTouchFeedback() {
    _pendingTouch = null;
    _touchLevel = null;
    _timers.remove('touch')?.cancel();
  }

  static bool _wanted(_Alert alert, CoreState s) => switch (alert) {
        _Alert.fault => s.hazard == Hazard.fault,
        // 위험 잠금 중 hazard ≤ mid(중불)이면 해제 대기(ACK_WAIT·UNLOCK_WAIT),
        // 그보다 높으면(강불 열이 남아 있어도) 위험 대화상자 (설계 v1 §4.3).
        _Alert.release =>
          s.lock == Lock.lockedDanger && s.hazard.index <= Hazard.mid.index,
        // 센서 고장이면 고장 창 하나만 둔다 (고장 중 locked_danger가 위험 창을 다시 띄우지 않게).
        _Alert.danger => s.hazard == Hazard.danger ||
            (s.lock == Lock.lockedDanger &&
                (s.hazard == Hazard.high || s.hazard == Hazard.warning)),
        _Alert.cat => s.lock == Lock.lockedCat,
        _Alert.absent =>
          (s.hazard == Hazard.high || s.hazard == Hazard.warning) &&
              s.reasons.contains('ABSENT'),
      };

  void _dismiss(_Alert alert) => setState(() => _dismissed.add(alert));

  @override
  Widget build(BuildContext context) {
    final demo = _feed is DemoKitchenFeed ? _feed : null;
    return Scaffold(
      backgroundColor: Hs.bg,
      body: SafeArea(
        child: Column(
          children: [
            if (demo != null)
              _RehearsalBar(
                selectedId: _rehearsalId,
                onSelected: (scenario) {
                  setState(() => _rehearsalId = scenario.id);
                  switch (scenario.state) {
                    case final s?:
                      demo.play(s, then: scenario.then);
                    case null:
                      demo.silence();
                  }
                },
              ),
            Expanded(
              child: FittedBox(
                child: SizedBox(
                  width: 1920,
                  height: 1080,
                  child: ColoredBox(color: Hs.bg, child: _canvas()),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _canvas() {
    final state = _state;
    final now = DateTime.now();
    if (!_stale && (state == null || _splash)) {
      return Splash(version: DashboardScreen.version, brand: widget.brand);
    }
    // 끊긴 동안은 마지막 state를 「지금 값」으로 쓰지 않는다.
    final live = _stale ? null : state;
    final offline = live == null;
    final recovered = live != null && live.lock == Lock.unlocked && _recovered;
    final top =
        live == null ? offlineHeadline : headline(live, recovered: recovered);

    final alert = live == null
        ? null
        : _Alert.values
            .where((a) => _wanted(a, live) && !_dismissed.contains(a))
            .firstOrNull;
    final voice = alert != null ? null : _voiceShown;
    final voiceLeft = voice == null || _voiceAt == null
        ? Duration.zero
        : _voiceFor(voice) - now.difference(_voiceAt!);
    final toast = live != null && alert == null ? _toastText : null;

    return Stack(
      children: [
        Padding(
          padding: const EdgeInsets.all(48),
          child: Column(
            children: [
              SizedBox(height: 72, child: _header(top, now)),
              const SizedBox(height: 24),
              Expanded(child: _body(state, top, offline, now)),
              const SizedBox(height: 24),
              SizedBox(height: 88, child: _nav(offline)),
            ],
          ),
        ),
        if (toast != null) Toast(text: toast),
        if (live != null) ...[
          if (alert != null)
            ..._alert(alert, live, now)
          else if (voice != null) ...[
            Dim(alpha: 0.2, onTap: () => setState(() => _voiceShown = null)),
            VoiceResultPanel(
              voice: voice,
              state: live,
              closesIn: voiceLeft < const Duration(seconds: 1)
                  ? const Duration(seconds: 1)
                  : voiceLeft,
              onClose: () => setState(() => _voiceShown = null),
            ),
          ] else if (_sheet) ...[
            Dim(alpha: 0.2, onTap: () => setState(() => _sheet = false)),
            DeviceSheet(
              part: _sheetPart,
              state: live,
              onInduction: (level) => _send(UiCommand.indSet(level)),
              onFan: (level) => _send(UiCommand.fanSet(level)),
              onUserLock: (locked) => _send(UiCommand.userLock(locked)),
              onClose: () => setState(() => _sheet = false),
            ),
          ],
        ],
      ],
    );
  }

  Widget _header(Headline headline, DateTime now) {
    final home = _tab == _Tab.home;
    final title = switch (_tab) {
      _Tab.home => '주방',
      _Tab.sensors => '센서',
      _Tab.history => '기록',
    };
    final logo = widget.brand.logo;
    return Row(
      children: [
        // LG 로고는 상단 바와 시작 화면에만, 경고 창에는 넣지 않는다 (설계 v1 §1).
        if (logo != null) ...[
          LgLogo(image: logo),
          const SizedBox(width: 20),
          const SizedBox(
              height: 32, child: VerticalDivider(width: 1, color: Hs.line)),
          const SizedBox(width: 24),
        ],
        if (home)
          const Icon(Icons.home_outlined, size: 32, color: Hs.t1)
        else
          SizedBox.square(
            dimension: 56,
            child: IconButton(
              tooltip: '홈으로',
              onPressed: () => setState(() => _tab = _Tab.home),
              style:
                  IconButton.styleFrom(backgroundColor: Hs.card, elevation: 0),
              icon: const Icon(Icons.chevron_left_rounded,
                  size: 32, color: Hs.t1),
            ),
          ),
        const SizedBox(width: 16),
        Text(title, style: hs(28, weight: FontWeight.w600, spacing: -0.01)),
        const SizedBox(width: 24),
        Text(clock(now), style: hs(24, weight: FontWeight.w300, color: Hs.t2)),
        // A·⑥·타임라인은 앱 메모리 기록이라 한 번만 밝힌다 (PM 설계 10/26, #156).
        if (_tab == _Tab.history) ...[
          const SizedBox(width: 20),
          SmallPill(
              text: _db == null || _db.failed
                  ? '${clock(_history.since)}부터 · 앱을 켠 뒤 기록'
                  : '조리·타임라인 기기에 저장 · 지켜 준 기록은 ${clock(_history.since)}부터',
              background: Hs.chip,
              color: Hs.t2,
              pulse: false),
        ],
        const Spacer(),
        if (_tab == _Tab.sensors) ...[
          _RangeSwitch(
            value: _range,
            onChanged: (range) => setState(() => _range = range),
          ),
          const SizedBox(width: 16),
        ],
        // 배지 색·문구는 0.4 s 크로스페이드, 깜빡이지 않는다 (설계 v1 §5.1).
        AnimatedSwitcher(
          duration: motion(context, const Duration(milliseconds: 400)),
          child: StatusPill(
              key: ValueKey(headline.pill),
              tone: headline.tone,
              text: headline.pill),
        ),
      ],
    );
  }

  Widget _body(
      CoreState? state, Headline headline, bool offline, DateTime now) {
    return switch (_tab) {
      _Tab.home => HomeView(
          state: state,
          headline: headline,
          offline: offline,
          now: now,
          lastAt: _lastAt,
          absentSince: _absentSince,
          thermalLostSince: _thermalLostSince,
          voiceAt: _voiceAt,
          video: widget.video,
          onDevices: () => setState(() {
            _sheet = !offline;
            _sheetPart = DevicePart.hood;
          }),
          // 위험 잠금 중 인덕션 카드는 닫았던 잠금 대화상자를 다시 연다 (설계 v1 §4.5).
          onInduction: () => setState(() {
            if (state?.lock == Lock.lockedDanger && !offline) {
              _dismissed
                ..remove(_Alert.release)
                ..remove(_Alert.danger);
            } else {
              _sheet = !offline;
              _sheetPart = DevicePart.induction;
            }
          }),
          onSensors: () => setState(() => _tab = _Tab.sensors),
          onInductionOn: () => _send(UiCommand.indSet(1)),
        ),
      _Tab.sensors => SensorView(
          state: state,
          offline: offline,
          range: _range,
          metric: _metricTrend,
          pm: _pmTrend,
        ),
      _Tab.history => HistoryView(
          log: _history,
          events: _events,
          now: now,
          stored: _storedSessions,
          hoodCare: HoodCareCard(
              care: _care, notice: _careNotice, onCleaned: _cleaned),
        ),
    };
  }

  Widget _nav(bool offline) {
    Widget item(String label, IconData icon, bool selected, VoidCallback? on) =>
        Semantics(
          selected: selected,
          button: true,
          child: SizedBox(
            width: 176,
            height: 72,
            child: TextButton(
              onPressed: on,
              style: TextButton.styleFrom(
                backgroundColor: selected ? Hs.chip : Colors.transparent,
                foregroundColor: selected ? Hs.t1 : Hs.t2,
                disabledForegroundColor: Hs.t3,
                shape: const StadiumBorder(),
              ),
              child: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(icon, size: 26),
                  const SizedBox(width: 10),
                  Text(label,
                      style: hs(22,
                          weight: FontWeight.w500,
                          color: on == null
                              ? Hs.t3
                              : selected
                                  ? Hs.t1
                                  : Hs.t2)),
                ],
              ),
            ),
          ),
        );
    return Center(
      child: Container(
        padding: const EdgeInsets.all(8),
        decoration: BoxDecoration(
          color: Hs.card,
          borderRadius: BorderRadius.circular(44),
          boxShadow: Hs.shadow,
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            item('홈', Icons.home_outlined, _tab == _Tab.home,
                () => setState(() => _tab = _Tab.home)),
            const SizedBox(width: 4),
            item('센서', Icons.show_chart_rounded, _tab == _Tab.sensors,
                () => setState(() => _tab = _Tab.sensors)),
            const SizedBox(width: 4),
            item(
                '기기',
                Icons.grid_view_rounded,
                _sheet,
                offline
                    ? null
                    : () => setState(() {
                          _sheet = true;
                          _sheetPart = DevicePart.both;
                        })),
            const SizedBox(width: 4),
            item('기록', Icons.schedule_rounded, _tab == _Tab.history,
                () => setState(() => _tab = _Tab.history)),
          ],
        ),
      ),
    );
  }

  List<Widget> _alert(_Alert alert, CoreState s, DateTime now) {
    final metric = InfoChip('온도차 ${number(s.thermal.metric)} °C',
        background: Tone.danger.tint, color: Tone.danger.ink);
    final pm = InfoChip('PM2.5 ${pmNumber(s.pm.pm25)} µg/m³',
        background: Tone.danger.tint, color: Tone.danger.ink);
    final hood = InfoChip('후드 ${levelText(s.fan.level)} 가동', background: Hs.bg);
    void details() => setState(() {
          _dismissed.add(alert);
          _tab = _Tab.sensors;
        });
    switch (alert) {
      case _Alert.fault:
        final lost = _thermalLostSince == null
            ? null
            : now.difference(_thermalLostSince!);
        final h = headline(s);
        return [
          const Dim(alpha: 0.35),
          SideAlert(
            tone: Tone.warning,
            icon: Icons.build_outlined,
            title: h.title.replaceFirst('가 ', '가\n'),
            body: '${h.body}\n연결을 확인해 주세요.',
            chips: [
              InfoChip(lost == null ? '응답 없음' : '${duration(lost)}째 응답 없음',
                  background: Tone.warning.tint, color: Tone.warning.ink),
              hood,
            ],
            primary: HsButton(label: '센서 상태 보기', onPressed: details),
            onClose: () => _dismiss(alert),
          ),
        ];
      case _Alert.release:
        return [
          const Dim(alpha: 0.55),
          // 「위험 확인」은 ACK_ALERT만 보내고 대화상자를 닫지 않는다 — 다음 state의
          // UNLOCK_WAIT로 조건 카드가 바뀌고 1차 버튼이 「닫기」가 된다.
          ReleaseAlert(
            state: s,
            onAck: () => unawaited(_send(const UiCommand.ackAlert())),
            onClose: () => _dismiss(alert),
            onDetail: details,
          ),
        ];
      case _Alert.danger:
        final smoke = s.reasons.contains('SMOKE');
        final hot =
            s.reasons.contains('TEMP_DANGER') || s.reasons.contains('TEMP_ABS');
        // 화재위험 (D-054)은 불이 나기 직전에 미리 끈 것이다. 대피·119처럼 이미 불이 난 걸
        // 가정한 말은 쓰지 않는다 (P-011 §5, PM 결정 10/25).
        final fire = smoke && s.reasons.contains('TEMP_DANGER');
        final ind = s.induction.level;
        final fan = s.fan.level;
        return [
          const Dim(alpha: 0.55),
          CenterAlert(
            tone: Tone.danger,
            icon: fire || hot
                ? Icons.local_fire_department_outlined
                : smoke
                    ? Icons.cloud_outlined
                    : Icons.lock_outline_rounded,
            title: fire
                ? '화재 위험을 미리 감지해 인덕션을 껐어요'
                : smoke
                    ? '연기가 감지되었습니다'
                    : hot
                        ? '온도가 너무 높습니다'
                        : '위험 잠금 중입니다',
            body: fire
                ? '후드가 최대로 환기하고 있어요. 뜨거운 조리 기구는 그대로 두세요.\n'
                    '연기가 걷히고 온도가 내려가면 확인 후 다시 쓸 수 있어요.'
                : smoke || hot
                    ? '인덕션을 ${ind == 0 ? '껐고' : '$ind단으로 낮췄고'} '
                        '후드를 ${fan == 3 ? '최대로' : '$fan단으로'} 올렸어요.\n'
                        '환기가 끝날 때까지 잠시 기다려 주세요.'
                    : '위험이 가라앉을 때까지 인덕션을 잠가 둬요.\n'
                        '환기가 끝나고 확인을 누르면 쓸 수 있어요.',
            chips: fire || hot
                ? [metric, pm, hood]
                : smoke
                    ? [pm, metric, hood]
                    : [hood],
            primary: '확인했어요',
            onPrimary: () => _dismiss(alert),
            onDetail: details,
            conditions:
                s.lock == Lock.lockedDanger ? releaseConditions(s) : null,
          ),
        ];
      case _Alert.cat:
        // 카메라 판정이 끊기면 CAT_NEAR가 빠지고 잠금만 남는다 — 「있어요」라고 하지 않는다.
        final seen = catSeen(s);
        return [
          const Dim(alpha: 0.35),
          PanelHighlight(color: Tone.pet.main),
          SideAlert(
            tone: Tone.pet,
            icon: Icons.pets_rounded,
            title: seen ? '조리 공간에\n반려동물이 있어요' : '반려동물\n잠금 중이에요',
            body: seen
                ? '안전을 위해 인덕션을 잠갔어요.\n반려동물이 떠나고 조리대 앞에 사람이 오면 풀려요.'
                : s.vision.fresh
                    ? '반려동물이 더 보이지 않아요.\n조리대 앞에 사람이 오면 잠금이 풀려요.'
                    : '카메라 판정이 끊겼어요.\n조리대 앞에 사람이 오면 잠금이 풀려요.',
            chips: [
              InfoChip(
                  seen
                      ? '반려동물 감지'
                      : s.vision.fresh
                          ? '반려동물 안 보임'
                          : '카메라 판정 없음',
                  background: Tone.pet.tint,
                  color: Tone.pet.ink),
              InfoChip('인덕션 잠금 중', background: Hs.bg),
            ],
            // 누를 수 없는 「지금 켜기」는 뺐다 — 조건이 맞으면 Core가 푼다 (정인 10/26).
            onClose: () => _dismiss(alert),
          ),
        ];
      case _Alert.absent:
        final away =
            _absentSince == null ? null : now.difference(_absentSince!);
        final temp = s.reasons.contains('TEMP_HIGH') ||
            s.reasons.contains('TEMP_DANGER');
        return [
          const Dim(alpha: 0.35),
          PanelHighlight(color: Tone.warning.main),
          SideAlert(
            tone: Tone.warning,
            icon: Icons.person_off_outlined,
            title: temp
                ? '자리를 비운 동안\n온도가 높아요'
                : s.reasons.contains('SMOKE')
                    ? '자리를 비운 동안\n연기가 많아요'
                    : '자리를 비운 동안\n경고가 났어요',
            body:
                '후드 ${levelText(s.fan.level)} · ${inductionText(s.induction)}으로 '
                '운전 중이에요.\n돌아오시면 불을 확인해 주세요.',
            chips: [
              InfoChip(away == null ? '자리 비움' : '자리 비움 ${duration(away)}',
                  background: Tone.warning.tint, color: Tone.warning.ink),
              InfoChip('온도차 ${number(s.thermal.metric)} °C',
                  background: Tone.warning.tint, color: Tone.warning.ink),
              hood,
            ],
            primary: HsButton(
              label: '인덕션 끄기',
              onPressed: s.induction.level == 0
                  ? null
                  : () => _send(UiCommand.indSet(0)),
            ),
            onClose: () => _dismiss(alert),
          ),
        ];
    }
  }
}

class _RangeSwitch extends StatelessWidget {
  const _RangeSwitch({required this.value, required this.onChanged});

  final TrendRange value;
  final ValueChanged<TrendRange> onChanged;

  @override
  Widget build(BuildContext context) => Container(
        padding: const EdgeInsets.all(6),
        decoration: BoxDecoration(
          color: Hs.card,
          borderRadius: BorderRadius.circular(26),
          boxShadow: Hs.shadow,
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            for (final range in TrendRange.values)
              Semantics(
                selected: range == value,
                child: SizedBox(
                  height: 40,
                  child: TextButton(
                    onPressed: () => onChanged(range),
                    style: TextButton.styleFrom(
                      backgroundColor:
                          range == value ? Hs.chip : Colors.transparent,
                      padding: const EdgeInsets.symmetric(horizontal: 20),
                      shape: const StadiumBorder(),
                    ),
                    child: Text(range.label,
                        style: hs(18,
                            weight: FontWeight.w500,
                            color: range == value ? Hs.t1 : Hs.t2)),
                  ),
                ),
              ),
          ],
        ),
      );
}

/// 데모 피드 전용 시나리오 칩. 시연 화면(1920×1080) 바깥 위쪽에 둔다.
class _RehearsalBar extends StatelessWidget {
  const _RehearsalBar({required this.selectedId, required this.onSelected});

  final String? selectedId;
  final ValueChanged<DemoScenario> onSelected;

  @override
  Widget build(BuildContext context) => Semantics(
        label: '시연 시나리오',
        child: Padding(
          padding: const EdgeInsets.fromLTRB(12, 8, 12, 0),
          child: Wrap(
            spacing: 6,
            runSpacing: 6,
            children: [
              for (final scenario in demoScenarios)
                ChoiceChip(
                  label: Text(scenario.title),
                  selected: selectedId == scenario.id,
                  onSelected: (_) => onSelected(scenario),
                ),
            ],
          ),
        ),
      );
}
