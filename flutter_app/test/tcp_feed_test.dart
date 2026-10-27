import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/core_state.dart';
import 'package:hood_seek/data/tcp_feed.dart';
import 'package:hood_seek/data/ui_command.dart';

final _examples = Directory('../contracts/examples');

Map<String, Object?> _example(String name) =>
    jsonDecode(File('${_examples.path}/$name').readAsStringSync())
        as Map<String, Object?>;

String _line(Map<String, Object?> json) => '${jsonEncode(json)}\n';

Map<String, Object?> _state(int seq) =>
    _example('state.idle.json')..['seq'] = seq;

/// 루프백 SafetyCore 흉내. 접속마다 받은 줄을 모은다.
class _FakeCore {
  _FakeCore._(this._server) {
    _server.listen((socket) {
      final received = StreamController<Map<String, Object?>>();
      socket
          .cast<List<int>>()
          .transform(utf8.decoder)
          .transform(const LineSplitter())
          .listen((l) => received.add(jsonDecode(l) as Map<String, Object?>));
      _clients.add((socket, StreamIterator(received.stream)));
    });
  }

  static Future<_FakeCore> start() async =>
      _FakeCore._(await ServerSocket.bind(InternetAddress.loopbackIPv4, 0));

  final ServerSocket _server;
  final StreamController<(Socket, StreamIterator<Map<String, Object?>>)>
      _clients = StreamController();
  late final StreamIterator<(Socket, StreamIterator<Map<String, Object?>>)>
      clients = StreamIterator(_clients.stream);

  int get port => _server.port;

  Future<(Socket, StreamIterator<Map<String, Object?>>)> nextClient() async {
    expect(
        await clients.moveNext().timeout(const Duration(seconds: 3)), isTrue);
    return clients.current;
  }

  Future<void> close() => _server.close();
}

Future<Map<String, Object?>> _next(
    StreamIterator<Map<String, Object?>> it) async {
  expect(await it.moveNext().timeout(const Duration(seconds: 3)), isTrue);
  return it.current;
}

void main() {
  late _FakeCore core;
  late TcpKitchenFeed feed;

  setUp(() async {
    core = await _FakeCore.start();
    feed = TcpKitchenFeed(
      '127.0.0.1',
      port: core.port,
      retryDelay: const Duration(milliseconds: 100),
    );
  });

  tearDown(() async {
    await feed.close();
    await core.close();
  });

  test('접속 직후 hello(src=ui, seq 0)를 보낸다', () async {
    await feed.connect();
    final (_, rx) = await core.nextClient();
    final hello = await _next(rx);
    final example = _example('hello.ui.json');
    expect(hello.keys, example.keys);
    expect((hello['v'], hello['type'], hello['src'], hello['seq']),
        (1, 'hello', 'ui', 0));
    expect(hello['p'],
        {'node': TcpKitchenFeed.node, 'ver': TcpKitchenFeed.version});
  });

  test('state를 받고, 형식 오류·역행 seq는 버리고 카운트한다', () async {
    final states = StreamIterator(feed.states);
    await feed.connect();
    final (socket, _) = await core.nextClient();

    socket.write(_line(_state(10)));
    socket.write('not json\n');
    socket.write(_line(_state(10))); // 중복
    socket.write(_line(_state(9))); // 역행
    socket
        .write(_line(_example('invalid/state.fan-level-4.json')..['seq'] = 11));
    socket.write(_line(_state(12)));

    expect(await states.moveNext(), isTrue);
    expect(states.current.seq, 10);
    expect(await states.moveNext(), isTrue);
    expect(states.current.seq, 12);
    expect(feed.droppedCount, 4);
    await states.cancel();
  });

  test('ui_cmd는 계약 예제와 같은 payload로, seq를 이어서 보낸다', () async {
    await feed.connect();
    final (_, rx) = await core.nextClient();
    await _next(rx); // hello

    await feed.send(UiCommand.fanSet(2));
    await feed.send(UiCommand.indSet(1));
    await feed.send(const UiCommand.ackAlert());
    await feed.send(const UiCommand.userLock(true));

    // 옛 세 명령은 v 1 그대로, USER_LOCK만 v 2 (D-058) — 새 앱을 먼저 올려도 옛 Core가 터치를 받는다.
    for (final (seq, name) in [
      (1, 'ui_cmd.json'),
      (2, 'ui_cmd.ind-set.json'),
      (3, 'ui_cmd.ack-alert.json'),
      (4, 'ui_cmd.user-lock.json'),
    ]) {
      final msg = await _next(rx);
      expect((msg['type'], msg['src'], msg['seq']), ('ui_cmd', 'ui', seq));
      expect(msg['v'], _example(name)['v']);
      expect(msg['p'], _example(name)['p']);
    }
  });

  test('끊기면 재접속하고 hello부터 seq 0으로 다시 시작한다', () async {
    final states = StreamIterator(feed.states);
    await feed.connect();
    final (first, rx1) = await core.nextClient();
    await _next(rx1);
    first.write(_line(_state(500)));
    expect(await states.moveNext(), isTrue);
    await first.close();

    final (second, rx2) = await core.nextClient();
    expect((await _next(rx2))['seq'], 0);
    // 새 연결에서는 seq 기준을 다시 잡으므로 작은 seq도 받는다.
    second.write(_line(_state(1)));
    expect(await states.moveNext(), isTrue);
    expect(states.current.seq, 1);
    expect(states.current.hazard, Hazard.low);
    await states.cancel();
  });

  test('state가 stateTimeout 동안 없으면 소켓을 버리고 재접속한다 (FIN 없는 단절, F3)', () async {
    await feed.close();
    feed = TcpKitchenFeed(
      '127.0.0.1',
      port: core.port,
      retryDelay: const Duration(milliseconds: 100),
      stateTimeout: const Duration(milliseconds: 300),
    );
    final states = StreamIterator(feed.states);
    await feed.connect();
    final (first, rx1) = await core.nextClient();
    await _next(rx1);
    first.write(_line(_state(7)));
    expect(await states.moveNext(), isTrue);

    // 서버는 소켓을 닫지 않고 침묵만 한다. 피드가 스스로 끊고 hello부터 다시 시작해야 한다.
    final (_, rx2) = await core.nextClient();
    final hello = await _next(rx2);
    expect((hello['type'], hello['seq']), ('hello', 0));
    await states.cancel();
  });

  test('연결 전 send는 StateError', () async {
    await core.close();
    expect(() => feed.send(UiCommand.fanSet(1)), throwsStateError);
  });
}
