import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'core_state.dart';
import 'kitchen_feed.dart';
import 'line_framer.dart';
import 'ui_command.dart';

/// `main.dart`의 conditional import 진입점. web 빌드는 `tcp_feed_stub.dart`를 쓴다.
KitchenFeed? coreFeed(String host, int port) =>
    TcpKitchenFeed(host, port: port);

/// SafetyCore TCP 클라이언트 (계약 v1 §2.1).
///
/// 접속 → `hello` → `state` 수신. 끊기면 [retryDelay] 뒤 무한 재접속한다.
/// 형식 오류·역행/중복 seq·상한 초과 줄은 버리고 [droppedCount]만 올린다(상태 불변).
/// "연결 끊김" 판정(state 3 s 무수신)은 소켓 상태가 아니라 화면이 state 도착 간격으로 한다.
/// 피드도 [stateTimeout] 동안 state가 없으면 소켓을 버리고 재접속한다 — LAN이 FIN 없이
/// 끊기면(F3) 소켓이 수 분간 「연결됨」으로 남기 때문이다.
class TcpKitchenFeed implements KitchenFeed {
  TcpKitchenFeed(
    this.host, {
    this.port = 7420,
    this.retryDelay = const Duration(seconds: 1),
    this.connectTimeout = const Duration(seconds: 2),
    this.stateTimeout = const Duration(seconds: 3),
  });

  static const node = 'flutter-dashboard';
  static const version = '0.3.0';

  final String host;
  final int port;
  final Duration retryDelay;
  final Duration connectTimeout;
  final Duration stateTimeout;

  final StreamController<CoreState> _states = StreamController.broadcast();
  final Stopwatch _clock = Stopwatch()..start();
  Socket? _socket;
  Timer? _retry;
  Timer? _watchdog;
  bool _closed = false;
  int _txSeq = 0;
  int? _rxSeq;
  int _dropped = 0;

  /// 버린 수신 메시지 수 (검증 실패 카운터).
  int get droppedCount => _dropped;

  @override
  Stream<CoreState> get states => _states.stream;

  @override
  Future<void> connect() => _dial();

  Future<void> _dial() async {
    if (_closed) return;
    try {
      final socket = await Socket.connect(host, port, timeout: connectTimeout);
      if (_closed) {
        socket.destroy();
        return;
      }
      socket.setOption(SocketOption.tcpNoDelay, true);
      if (Platform.isLinux) {
        // ACK 없는 송신 데이터가 2 s를 넘으면 커널이 연결을 버린다. 케이블이 빠진 사이 보낸
        // ui_cmd가 버퍼에 남았다가 다시 꽂은 뒤 「새 명령」으로 집행되지 않게 (IPPROTO_TCP 6,
        // TCP_USER_TIMEOUT 18 — vision_node #77과 같은 처리).
        socket.setRawOption(RawSocketOption.fromInt(6, 18, 2000));
      }
      _socket = socket;
      _armWatchdog(socket);
      // seq는 연결마다 새로 센다 (계약 README 「전송 규칙」).
      _txSeq = 0;
      _rxSeq = null;
      final framer = LineFramer(onLine: _onLine, onOversize: _drop);
      socket.listen(
        framer.add,
        onError: (Object _) => _lost(socket),
        onDone: () => _lost(socket),
        cancelOnError: true,
      );
      _write(socket, 'hello', {'node': node, 'ver': version});
    } on Object {
      _scheduleRetry();
    }
  }

  void _armWatchdog(Socket socket) {
    _watchdog?.cancel();
    _watchdog = Timer(stateTimeout, () => _lost(socket));
  }

  void _lost(Socket socket) {
    if (!identical(_socket, socket)) return;
    _socket = null;
    _watchdog?.cancel();
    socket.destroy();
    _scheduleRetry();
  }

  void _scheduleRetry() {
    if (_closed || _retry != null) return;
    _retry = Timer(retryDelay, () {
      _retry = null;
      unawaited(_dial());
    });
  }

  void _onLine(Uint8List line) {
    final CoreState state;
    try {
      state = CoreState.fromJson(jsonDecode(utf8.decode(line)));
    } on FormatException {
      _drop();
      return;
    }
    final last = _rxSeq;
    if (last != null && state.seq <= last) {
      _drop();
      return;
    }
    _rxSeq = state.seq;
    final socket = _socket;
    if (socket != null) _armWatchdog(socket);
    _states.add(state);
  }

  void _drop() => _dropped++;

  void _write(Socket socket, String type, Map<String, Object?> payload,
      {int version = 1}) {
    final message = {
      'v': version,
      'type': type,
      'src': 'ui',
      'seq': _txSeq++,
      't_ms': _clock.elapsedMilliseconds,
      'p': payload,
    };
    socket.add(utf8.encode('${jsonEncode(message)}\n'));
  }

  @override
  Future<void> send(UiCommand command) async {
    final socket = _socket;
    if (socket == null) {
      throw StateError('SafetyCore에 연결되어 있지 않습니다');
    }
    _write(socket, 'ui_cmd', command.toPayload(), version: command.version);
  }

  @override
  Future<void> close() async {
    _closed = true;
    _retry?.cancel();
    _watchdog?.cancel();
    _socket?.destroy();
    _socket = null;
    await _states.close();
  }
}
