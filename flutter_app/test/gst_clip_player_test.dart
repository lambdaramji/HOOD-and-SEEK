import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/audio/gst_clip_player.dart';

/// asset 대신 쓰는 묶음. 없는 이름은 Flutter처럼 예외를 던진다.
class _Bundle extends CachingAssetBundle {
  _Bundle(this.files, {this.hold = const {}});

  final Map<String, List<int>> files;

  /// 이 이름은 Future가 끝날 때까지 늦게 풀린다 (처음 내는 클립이 느린 경우).
  final Map<String, Future<void>> hold;

  @override
  Future<ByteData> load(String key) async {
    await hold[key];
    final bytes = files[key];
    if (bytes == null) throw FlutterError('Unable to load asset: $key');
    return ByteData.sublistView(Uint8List.fromList(bytes));
  }
}

/// 실제 gst 대신 띄운 것처럼 보이는 프로세스. [kill]하면 끝난다.
class _Process implements Process {
  final _exit = Completer<int>();
  bool killed = false;

  void finish() => _exit.complete(0);

  @override
  bool kill([ProcessSignal signal = ProcessSignal.sigterm]) {
    killed = true;
    if (!_exit.isCompleted) _exit.complete(-15);
    return true;
  }

  @override
  Future<int> get exitCode => _exit.future;

  @override
  Stream<List<int>> get stdout => const Stream.empty();

  @override
  Stream<List<int>> get stderr => const Stream.empty();

  @override
  IOSink get stdin => throw UnimplementedError();

  @override
  int get pid => 1;
}

void main() {
  late List<(String, List<String>)> started;
  late List<_Process> processes;
  late GstClipPlayer player;

  Future<Process> start(String exe, List<String> args) async {
    started.add((exe, args));
    final process = _Process();
    processes.add(process);
    return process;
  }

  final logs = <String>[];
  final originalPrint = debugPrint;
  setUp(() {
    logs.clear();
    debugPrint = (String? message, {int? wrapWidth}) => logs.add(message ?? '');
    started = [];
    processes = [];
    player = GstClipPlayer(
      sink: 'pulsesink device=speaker',
      bundle: _Bundle({
        'assets/audio/CAUTION.wav': [1, 2, 3],
        'assets/audio/DANGER.wav': [4, 5],
        'assets/audio/APP_START.wav': [6],
        'assets/audio/TOUCH_FEEDBACK.wav': [7],
        'assets/audio/results/RESULT_FAN_2.wav': [8],
      }),
      start: start,
    );
  });

  tearDown(() {
    debugPrint = originalPrint;
    for (final (_, args) in started) {
      final dir = File(args[2].substring('location='.length)).parent;
      if (dir.existsSync()) dir.deleteSync(recursive: true);
    }
  });

  test('asset을 임시 파일로 풀어 gst-launch-1.0으로 낸다', () async {
    await player.launch('CAUTION');

    expect(started, hasLength(1));
    final (exe, args) = started.single;
    expect(exe, 'gst-launch-1.0');
    expect(args.take(2), ['-q', 'filesrc']);
    expect(args.skip(3), [
      '!',
      'wavparse',
      '!',
      'audioconvert',
      '!',
      'audioresample',
      '!',
      'pulsesink',
      'device=speaker',
    ]);
    final file = File(args[2].substring('location='.length));
    expect(file.path, endsWith('/CAUTION.wav'));
    expect(file.readAsBytesSync(), [1, 2, 3]);
  });

  test('새 클립이 오면 앞 소리를 끊는다', () async {
    await player.launch('CAUTION');
    await player.launch('DANGER');

    expect(started.map((s) => s.$2[2].split('/').last),
        ['CAUTION.wav', 'DANGER.wav']);
    expect(processes.first.killed, isTrue);
    expect(processes.last.killed, isFalse);
    // 일부러 끊은 소리는 실패로 남기지 않는다.
    await pumpEventQueue();
    expect(logs, isNot(contains(startsWith('경고음 재생 실패'))));
  });

  test('클립 파일이 없으면 아무것도 띄우지 않고, 나던 소리도 그대로 둔다', () async {
    await player.launch('CAUTION');
    await player.launch('OFFLINE');

    expect(started, hasLength(1));
    expect(processes.single.killed, isFalse);
  });

  test('gst가 없어도 앱은 멈추지 않는다', () async {
    final failing = GstClipPlayer(
      bundle: _Bundle({
        'assets/audio/CAUTION.wav': [1],
      }),
      start: (exe, args) async {
        started.add((exe, args));
        throw const ProcessException('gst-launch-1.0', [], 'not found', 2);
      },
    );
    await failing.launch('CAUTION');
    expect(started, hasLength(1));
  });

  /// 실제 파일 쓰기가 끝날 때까지 기다린다.
  Future<void> until(bool Function() done) async {
    for (var i = 0; i < 200 && !done(); i++) {
      await Future<void>.delayed(const Duration(milliseconds: 5));
    }
  }

  test('gst가 뜨기 전에 다음 클립이 오면 나중 것 하나만 남는다', () async {
    final gates = <Completer<void>>[];
    final slow = GstClipPlayer(
      bundle: _Bundle({
        'assets/audio/CAUTION.wav': [1],
        'assets/audio/DANGER.wav': [2],
      }),
      start: (exe, args) async {
        final gate = Completer<void>();
        gates.add(gate);
        await gate.future;
        return start(exe, args);
      },
    );
    // 앱처럼 기다리지 않고 연달아 부른다.
    final first = slow.launch('CAUTION');
    final second = slow.launch('DANGER');
    await until(() => gates.length == 2);
    for (final gate in gates) {
      gate.complete();
    }
    await Future.wait([first, second]);

    final alive = [
      for (final (i, p) in processes.indexed)
        if (!p.killed) started[i].$2[2].split('/').last,
    ];
    expect(alive, ['DANGER.wav']);
  });

  test('늦게 풀린 옛 클립은 더 새 클립을 끊지 않는다', () async {
    final gate = Completer<void>();
    final player = GstClipPlayer(
      bundle: _Bundle({
        'assets/audio/CAUTION.wav': [1],
        'assets/audio/DANGER.wav': [2],
      }, hold: {
        'assets/audio/CAUTION.wav': gate.future
      }),
      start: start,
    );
    final first = player.launch('CAUTION');
    await player.launch('DANGER');
    gate.complete();
    await first;

    expect(started.map((s) => s.$2[2].split('/').last), ['DANGER.wav']);
    expect(processes.single.killed, isFalse);
  });

  test('PULSE_SERVER가 없고 시스템 소켓이 있을 때만 시스템 소켓을 넘긴다', () {
    bool yes(String path) => path == '/run/pulse/native';
    bool no(String path) => false;
    expect(
        pulseEnvironment({}, yes), {'PULSE_SERVER': 'unix:/run/pulse/native'});
    expect(pulseEnvironment({'PULSE_SERVER': 'unix:/other'}, yes), isNull);
    expect(pulseEnvironment({}, no), isNull);
  });

  test('기동음·조작음은 재생 중인 경고를 끊지 않고, 끝난 뒤 다시 낼 수 있다', () async {
    await player.launch('DANGER');
    await player.launch('APP_START');
    await player.launch('TOUCH_FEEDBACK');
    expect(started, hasLength(1));
    expect(processes.single.killed, isFalse);
    processes.single.finish();
    await pumpEventQueue();
    await player.launch('TOUCH_FEEDBACK');
    expect(started.last.$2[2], endsWith('/TOUCH_FEEDBACK.wav'));
  });

  test('음성 결과 안내(results/…)는 하위 폴더에서 풀어 내고, 경고를 끊지 않는다 (#173)', () async {
    await player.launch('results/RESULT_FAN_2');
    final file = File(started.single.$2[2].substring('location='.length));
    expect(file.path, endsWith('/results_RESULT_FAN_2.wav'));
    expect(file.readAsBytesSync(), [8]);

    await player.launch('DANGER'); // 경고는 안내를 끊는다
    expect(processes.first.killed, isTrue);
    await player.launch('results/RESULT_FAN_2'); // 경고 중 안내는 생략
    expect(started, hasLength(2));
    expect(processes.last.killed, isFalse);
  });

  test('경고 asset 로딩 중에도 조작음을 생략한다', () async {
    final gate = Completer<void>();
    final waiting = GstClipPlayer(
      bundle: _Bundle({
        'assets/audio/DANGER.wav': [1],
        'assets/audio/TOUCH_FEEDBACK.wav': [2],
      }, hold: {
        'assets/audio/DANGER.wav': gate.future
      }),
      start: start,
    );
    final danger = waiting.launch('DANGER');
    await waiting.launch('TOUCH_FEEDBACK');
    expect(started, isEmpty);
    gate.complete();
    await danger;
    expect(started.single.$2[2], endsWith('/DANGER.wav'));
  });

  test('경고 프로세스 시작 중에도 조작음을 생략한다', () async {
    final gate = Completer<void>();
    var launching = false;
    final waiting = GstClipPlayer(
      bundle: _Bundle({
        'assets/audio/DANGER.wav': [1],
        'assets/audio/TOUCH_FEEDBACK.wav': [2],
      }),
      start: (exe, args) async {
        launching = true;
        await gate.future;
        return start(exe, args);
      },
    );
    final danger = waiting.launch('DANGER');
    await until(() => launching);
    await waiting.launch('TOUCH_FEEDBACK');
    gate.complete();
    await danger;
    expect(started.single.$2[2], endsWith('/DANGER.wav'));
    expect(processes.single.killed, isFalse);
  });

  test('경고는 기동음을 즉시 중단한다', () async {
    await player.launch('APP_START');
    await player.launch('DANGER');
    expect(processes.first.killed, isTrue);
    expect(processes.last.killed, isFalse);
  });
}
