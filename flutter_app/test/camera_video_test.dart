import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/camera_video.dart';
import 'package:hood_seek/data/kitchen_feed.dart';
import 'package:hood_seek/data/mjpeg.dart';
import 'package:hood_seek/screens/home_view.dart';

// vision-node 카메라 카드 영상 형식 그대로 (#144 `vision_node/README.md` 「카메라 카드 영상」).
List<int> _part(List<int> jpeg) => [
      ...ascii.encode('--hoodseekframe\r\nContent-Type: image/jpeg\r\n'
          'Content-Length: ${jpeg.length}\r\n\r\n'),
      ...jpeg,
      ...ascii.encode('\r\n'),
    ];

void main() {
  group('MjpegParser', () {
    test('파트 하나를 Content-Length만큼 잘라 JPEG 한 장으로', () {
      final frames = MjpegParser().add(_part([0xFF, 0xD8, 1, 2, 0xFF, 0xD9]));
      expect(frames, [
        [0xFF, 0xD8, 1, 2, 0xFF, 0xD9]
      ]);
    });

    test('한 바이트씩 와도, 두 장이 한 덩어리로 와도 같은 결과', () {
      final stream = [
        ..._part([1, 2, 3]),
        ..._part([4, 5])
      ];
      final parser = MjpegParser();
      final byByte = [
        for (final b in stream) ...parser.add([b])
      ];
      expect(byByte, [
        [1, 2, 3],
        [4, 5]
      ]);
      expect(MjpegParser().add(stream), [
        [1, 2, 3],
        [4, 5]
      ]);
    });

    test('JPEG 안에 빈 줄 모양(\\r\\n\\r\\n)이 있어도 길이로 자른다', () {
      final jpeg = [9, ...ascii.encode('\r\n\r\n'), 9];
      expect(
          MjpegParser().add([
            ..._part(jpeg),
            ..._part([7])
          ]),
          [
            jpeg,
            [7]
          ]);
    });

    test('길이가 없거나 너무 큰 파트는 버리고 다음 파트에서 다시 맞춘다', () {
      final parser = MjpegParser(maxFrameBytes: 4);
      final noLength =
          ascii.encode('--hoodseekframe\r\nContent-Type: image/jpeg\r\n\r\n');
      expect(
          parser.add([
            ...noLength,
            ..._part([1, 2])
          ]),
          [
            [1, 2]
          ]);
      expect(parser.add(_part([1, 2, 3, 4, 5])), isEmpty);
    });
  });

  test('루프백에서 받고, 서버가 끊으면 다시 붙는다', () async {
    // preview.cpp처럼 요청을 읽지 않고 HTTP/1.0 머리 + 파트를 보낸다.
    final server = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
    var connections = 0;
    server.listen((socket) async {
      connections++;
      socket.add(ascii.encode('HTTP/1.0 200 OK\r\nContent-Type: '
          'multipart/x-mixed-replace; boundary=hoodseekframe\r\n\r\n'));
      socket.add(_part([connections]));
      await socket.flush();
      await socket.close(); // 한 장 보내고 끊음
    });
    final video = CameraVideo(Uri.parse('http://127.0.0.1:${server.port}/'),
        retryDelay: const Duration(milliseconds: 50));
    final got =
        await video.frames.take(2).toList().timeout(const Duration(seconds: 5));
    expect(got, [
      [1],
      [2]
    ]);
    await server.close();
  });

  test('200이 아니면 프레임을 내지 않고 다시 시도한다', () async {
    final server = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
    var connections = 0;
    server.listen((socket) async {
      connections++;
      socket.add(ascii.encode('HTTP/1.0 404 Not Found\r\n\r\n'));
      socket.add(_part([9]));
      await socket.close();
    });
    final video = CameraVideo(Uri.parse('http://127.0.0.1:${server.port}/'),
        retryDelay: const Duration(milliseconds: 20));
    final got = <List<int>>[];
    final sub = video.frames.listen(got.add);
    await Future<void>.delayed(const Duration(milliseconds: 300));
    await sub.cancel();
    expect(got, isEmpty);
    expect(connections, greaterThan(1));
    await server.close();
  });

  testWidgets('카메라 카드: 첫 프레임 전엔 그림, 받으면 프레임 시각, 2 s 멈추면 「영상 일시 중지」',
      (tester) async {
    final frames = StreamController<Uint8List>.broadcast();
    final state = demoScenarios.firstWhere((s) => s.id == 'low').state!;
    await tester.pumpWidget(MaterialApp(
      home: Center(
        child: SizedBox(
          width: 1050,
          height: 560,
          child: CameraPanel(
            state: state,
            offline: false,
            lastAt: DateTime(2026, 10, 25, 22, 0, 1),
            sinceLast: Duration.zero,
            absentFor: null,
            video: frames.stream,
          ),
        ),
      ),
    ));
    // 영상이 아직 없으면 지금처럼 state 시각 표식과 그림.
    expect(find.text('22:00:01'), findsOneWidget);
    expect(find.textContaining('실시간 '), findsNothing);

    frames.add(Uint8List.fromList([0xFF, 0xD8, 0xFF, 0xD9]));
    await tester.pump();
    await tester.pump();
    expect(find.textContaining('실시간 '), findsOneWidget);
    expect(find.text('22:00:01'), findsNothing);
    expect(find.text('영상 일시 중지'), findsNothing);
    // 프레임은 자르지 않고 전체를 보인다.
    expect(tester.widget<Image>(find.byType(Image)).fit, BoxFit.contain);
    // 판정 칩은 영상과 상관없이 state 값 그대로.
    expect(find.text('사람 있음 · 반려동물 없음'), findsOneWidget);

    await tester.pump(const Duration(milliseconds: 1900));
    expect(find.text('영상 일시 중지'), findsNothing);
    await tester.pump(const Duration(milliseconds: 200));
    expect(find.text('영상 일시 중지'), findsOneWidget);

    frames.add(Uint8List.fromList([0xFF, 0xD8, 0xFF, 0xD9]));
    await tester.pump();
    await tester.pump();
    expect(find.text('영상 일시 중지'), findsNothing);
    await frames.close();
  });

  testWidgets('연기 그림·「연기 있음」 태그는 영상이 없을 때만 — 실시간 영상 위에는 없다 (수현 10/26)',
      (tester) async {
    final frames = StreamController<Uint8List>.broadcast();
    final state = demoScenarios.firstWhere((s) => s.id == 'pm-warn').state!;
    await tester.pumpWidget(MaterialApp(
      home: Center(
        child: SizedBox(
          width: 1050,
          height: 590,
          child: CameraPanel(
            state: state,
            offline: false,
            lastAt: DateTime(2026, 10, 26, 20, 0),
            sinceLast: Duration.zero,
            absentFor: null,
            video: frames.stream,
          ),
        ),
      ),
    ));
    expect(find.text('연기 있음 · 미세먼지 센서'), findsOneWidget);
    frames.add(Uint8List.fromList([0xFF, 0xD8, 0xFF, 0xD9]));
    await tester.pump();
    await tester.pump();
    expect(find.text('연기 있음 · 미세먼지 센서'), findsNothing);
    await frames.close();
  });
}
