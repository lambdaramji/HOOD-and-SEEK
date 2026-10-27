import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'clip_player.dart';

ClipPlayer deviceClipPlayer(String sink) {
  final env =
      pulseEnvironment(Platform.environment, (path) => File(path).existsSync());
  return GstClipPlayer(
    sink: sink,
    start: (exe, args) => Process.start(exe, args, environment: env),
  );
}

const _systemPulseSocket = '/run/pulse/native';
const _feedbackClips = {'APP_START', 'TOUCH_FEEDBACK'};

/// 경고가 아닌 안내 소리: 기동음·조작음과 음성 명령 결과 안내(`results/…`, #173).
/// 경고를 끊지 않고, 경고를 불러오거나 내는 동안에는 생략한다.
bool _isFeedback(String clip) =>
    _feedbackClips.contains(clip) || clip.startsWith('results/');

/// ATLAS의 PulseAudio는 시스템 모드다. 10/25 ReSpeaker 출력 확인(#134)은 셸에서
/// `PULSE_SERVER=unix:/run/pulse/native`를 붙여 했는데, 앱은 AppManager가 띄워 그 변수가
/// 없을 수 있다. 변수가 없고 시스템 소켓이 있으면 그 소켓을 쓰게 한다. 이미 있으면 건드리지 않는다.
/// 출력 장치는 ATLAS AudioManager의 활성 출력이 정한다(`deploy/README.md`).
Map<String, String>? pulseEnvironment(
        Map<String, String> env, bool Function(String path) exists) =>
    env.containsKey('PULSE_SERVER') || !exists(_systemPulseSocket)
        ? null
        : const {'PULSE_SERVER': 'unix:$_systemPulseSocket'};

/// Pi 5에서 `gst-launch-1.0`으로 클립을 낸다. [sink]는 GStreamer 출력 요소다
/// (`--dart-define=AUDIO_SINK`). ATLAS의 ActiveOutputDevice도 USB로 선택해야 하며,
/// 장치 지정만으로 ReSpeaker 출력을 보장하지 않는다(10/25 실측, deploy/README.md).
///
/// 클립은 앱 asset이라 파일 경로가 없어서, 처음 낼 때 임시 폴더에 한 번 풀어 둔다.
/// 새 경고는 앞 소리를 끊는다. 기동음·조작음은 로딩/재생 중인 경고를 끊지 않는다.
/// 클립 파일이나 gst가 없으면 화면은 그대로 두고 로그만 남긴다.
class GstClipPlayer implements ClipPlayer {
  GstClipPlayer({
    this.sink = 'pulsesink',
    AssetBundle? bundle,
    this.start = Process.start,
  }) : _bundle = bundle ?? rootBundle;

  /// GStreamer 출력 요소. 공백으로 나눠 그대로 붙인다(예: `alsasink device=hw:0`).
  final String sink;
  final Future<Process> Function(String, List<String>) start;
  final AssetBundle _bundle;

  final Map<String, Future<String?>> _files = {};
  Future<Directory>? _dir;
  Process? _playing;
  String? _playingClip;
  int _alertLoads = 0;

  bool get _alertActive =>
      _alertLoads > 0 || (_playingClip != null && !_isFeedback(_playingClip!));

  /// 겹쳐 들어온 요청 가운데 나중 것만 살리려고 매기는 순번. [_current]는 지금 소리를 낼 권리가 있는 순번.
  int _seq = 0;
  int _current = 0;

  @override
  void play(String clip) => unawaited(launch(clip));

  /// [play]와 같고, gst를 띄우고 나면(또는 낼 수 없다고 판단하면) 끝난다.
  Future<void> launch(String clip) async {
    final feedback = _isFeedback(clip);
    if (feedback && _alertActive) return;
    final ticket = ++_seq;
    if (!feedback) _alertLoads++;
    final String? path;
    try {
      path = await (_files[clip] ??= _extract(clip));
    } finally {
      if (!feedback) _alertLoads--;
    }
    // 클립이 없으면 나던 소리를 그대로 둔다. 늦게 풀린 옛 클립은 더 새 클립을 끊지 않는다.
    if (path == null || ticket < _current || (feedback && _alertActive)) return;
    _current = ticket;
    _playingClip = clip;
    final previous = _playing;
    _playing = null;
    previous?.kill();
    final Process process;
    try {
      process = await start('gst-launch-1.0', [
        '-q',
        'filesrc',
        'location=$path',
        '!',
        'wavparse',
        '!',
        'audioconvert',
        '!',
        'audioresample',
        '!',
        ...sink.split(' ').where((s) => s.isNotEmpty),
      ]);
    } on ProcessException catch (error) {
      if (ticket == _current) _playingClip = null;
      debugPrint('경고음을 내지 못했습니다 ($clip): $error');
      return;
    }
    // gst를 띄우는 사이 더 새 클립이 왔으면 방금 띄운 것을 바로 끈다.
    if (ticket != _current) {
      process.kill();
      return;
    }
    _playing = process;
    unawaited(process.stdout.drain<void>());
    final errors = process.stderr.transform(utf8.decoder).join();
    unawaited(process.exitCode.then((code) async {
      // 다음 클립 때문에 끊은 것은 실패가 아니다.
      if (!identical(_playing, process)) return;
      _playing = null;
      _playingClip = null;
      if (code != 0) {
        debugPrint('경고음 재생 실패 ($clip, exit $code): ${await errors}');
      }
    }));
  }

  Future<String?> _extract(String clip) async {
    try {
      final data = await _bundle.load('assets/audio/$clip.wav');
      final dir =
          await (_dir ??= Directory.systemTemp.createTemp('hoodseek-audio'));
      // 하위 폴더 클립(`results/…`)도 임시 폴더 한 곳에 푼다.
      final file = File('${dir.path}/${clip.replaceAll('/', '_')}.wav');
      await file.writeAsBytes(
          data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes));
      return file.path;
    } on Object catch (error) {
      debugPrint('경고음 클립이 없습니다: assets/audio/$clip.wav ($error)');
      return null;
    }
  }
}
