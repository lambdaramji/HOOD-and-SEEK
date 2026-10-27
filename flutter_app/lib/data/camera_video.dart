import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

import 'mjpeg.dart';

/// Pi 5 루프백의 vision-node 카메라 카드 영상을 JPEG 한 장씩 흘린다(UI 설계 v1 §3.2, #144).
/// 보는 쪽이 있을 때만 연결하고, 끊기면 1 s 뒤 다시 붙는다. 영상은 표시 전용이라 실패해도
/// 로그만 남기고 화면 판정에는 아무 영향이 없다.
Stream<Uint8List>? cameraVideo(String url) {
  final uri = Uri.tryParse(url);
  if (url.isEmpty || uri == null || uri.host.isEmpty) return null;
  return CameraVideo(uri).frames;
}

/// 서버(`vision_node/src/preview.cpp`)는 요청을 읽지 않고 HTTP/1.0 머리 뒤에 파트를 계속 보낸다.
/// HttpClient 대신 소켓 하나로 받는다 — 끝없는 응답이라 HTTP 기능이 필요 없고, 연결을 끊을 때도
/// 소켓만 닫으면 된다.
class CameraVideo {
  CameraVideo(
    this.uri, {
    this.retryDelay = const Duration(seconds: 1),
    this.connectTimeout = const Duration(seconds: 2),
  }) {
    _controller = StreamController<Uint8List>.broadcast(
      onListen: _run,
      onCancel: _stop,
    );
  }

  final Uri uri;
  final Duration retryDelay;
  final Duration connectTimeout;
  late final StreamController<Uint8List> _controller;
  Socket? _socket;
  bool _listening = false;
  int _generation = 0; // 다시 들으면 앞 루프가 끝나도록

  Stream<Uint8List> get frames => _controller.stream;

  Future<void> _run() async {
    _listening = true;
    final generation = ++_generation;
    var warned = false;
    while (_listening && generation == _generation) {
      try {
        final socket = _socket = await Socket.connect(
            uri.host, uri.hasPort ? uri.port : 80,
            timeout: connectTimeout);
        socket.write('GET ${uri.path.isEmpty ? '/' : uri.path} HTTP/1.0\r\n'
            'Host: ${uri.host}\r\n\r\n');
        var status = true; // 첫 줄은 「HTTP/1.x 200 …」이어야 한다
        var line = '';
        final parser = MjpegParser();
        await for (final chunk in socket) {
          if (generation != _generation) break;
          if (status) {
            line += latin1.decode(chunk, allowInvalid: true);
            final end = line.indexOf('\r\n');
            if (end < 0 && line.length < 256) continue;
            final first = end < 0 ? line : line.substring(0, end);
            final parts = first.split(' ');
            if (!first.startsWith('HTTP/') ||
                parts.length < 2 ||
                parts[1] != '200') {
              throw HttpException('응답 「$first」', uri: uri);
            }
            status = false;
            warned = false;
          }
          // HTTP 머리는 Content-Length가 없는 파트처럼 건너뛴다.
          for (final frame in parser.add(chunk)) {
            _controller.add(frame);
          }
        }
      } on Object catch (error) {
        // vision-node가 영상 없이 돌거나 재시작 중이면 계속 실패한다. 한 번만 남긴다.
        if (!warned) debugPrint('카메라 영상을 받지 못했습니다 ($uri): $error');
        warned = true;
      } finally {
        _socket?.destroy();
        _socket = null;
      }
      if (_listening && generation == _generation) {
        await Future<void>.delayed(retryDelay);
      }
    }
  }

  void _stop() {
    _listening = false;
    _generation++;
    _socket?.destroy();
  }
}
