import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/history_db.dart';
import 'package:hood_seek/data/kitchen_feed.dart';
import 'package:hood_seek/screens/dashboard_screen.dart';
import 'package:hood_seek/widgets/hs.dart';

void main() {
  group('HistoryDb', () {
    late Directory dir;
    setUp(() => dir = Directory.systemTemp.createTempSync('history_db'));
    tearDown(() => dir.deleteSync(recursive: true));

    test('타임라인·조리 요약을 쓰고, 닫았다 다시 열어도 최신부터 읽는다', () {
      final path = '${dir.path}/hoodseek.db';
      final db = HistoryDb.open(path)!;
      db.addEvent(StoredEvent(DateTime(2026, 10, 26, 16, 0), 'normal',
          '안전 장치 연결 — 모니터링 시작', '모니터링', 'safety'));
      db.addEvent(StoredEvent(
          DateTime(2026, 10, 26, 16, 1), 'pet', '반려동물 잠금 작동', '꺼짐', 'safety'));
      db.addSession(StoredSession(DateTime(2026, 10, 26, 16, 0),
          DateTime(2026, 10, 26, 16, 12), '약불 60 % · 중불 40 %'));
      db.close();

      final again = HistoryDb.open(path)!;
      expect(again.recentEvents(10).map((e) => e.text),
          ['반려동물 잠금 작동', '안전 장치 연결 — 모니터링 시작']);
      expect(again.recentEvents(1).single.tone, 'pet');
      expect(again.recentSessions(10).single.summary, '약불 60 % · 중불 40 %');
      expect(
          again.recentSessions(10).single.end, DateTime(2026, 10, 26, 16, 12));
      again.close();
    });

    test('처음 만들면 666, 남의 읽기 전용 파일이면 .old로 옮기고 새로 만든다 (#163 정인 리뷰)', () {
      final path = '${dir.path}/hoodseek.db';
      HistoryDb.open(path)!
        ..addEvent(
            StoredEvent(DateTime(2026, 10, 26), 'idle', '옛 기록', '-', 'other'))
        ..close();
      expect(File(path).statSync().modeString(), 'rw-rw-rw-');
      // 다른 계정이 만든 0644 파일처럼: 이 계정으로는 읽기만 된다.
      Process.runSync('chmod', ['444', path]);
      final again = HistoryDb.open(path)!;
      expect(File('$path.old').existsSync(), isTrue);
      expect(again.recentEvents(10), isEmpty); // 새 파일
      again.addEvent(
          StoredEvent(DateTime(2026, 10, 26), 'idle', '새 기록', '-', 'other'));
      expect(again.failed, isFalse);
      expect(File(path).statSync().modeString(), 'rw-rw-rw-');
      again.close();
    }, skip: Platform.isWindows);

    test('폴더가 없으면 null — 앱은 메모리로만 센다', () {
      expect(HistoryDb.open('${dir.path}/없는 폴더/hoodseek.db'), isNull);
    });

    test('쓰다가 실패하면 failed, 그 뒤로는 조용히 아무것도 안 한다', () {
      final db = HistoryDb.open(':memory:')!;
      db.close(); // 닫힌 DB에 쓰면 실패한다
      db.addEvent(StoredEvent(DateTime(2026), 'idle', 'x', 'y', 'other'));
      expect(db.failed, isTrue);
      expect(db.recentEvents(10), isEmpty);
    });
  });

  testWidgets('기록 탭: DB에 남은 지난 조리·타임라인을 켤 때 읽고, 새 기록도 DB에 쓴다', (tester) async {
    tester.view.physicalSize = const Size(1920, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final db = HistoryDb.open(':memory:')!;
    db.addEvent(StoredEvent(
        DateTime(2026, 10, 25, 19, 0), 'danger', '위험 잠금 작동', '꺼짐', 'safety'));
    db.addSession(StoredSession(DateTime(2026, 10, 25, 18, 40),
        DateTime(2026, 10, 25, 19, 1), '약불 50 % · 강불 50 % · 위험 차단으로 끝남'));
    final feed = DemoKitchenFeed(
        initial: demoScenarios.firstWhere((s) => s.id == 'idle').state!);
    await tester.pumpWidget(
        MaterialApp(home: DashboardScreen(feed: feed, historyDb: db)));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 1600));

    await tester.tap(find.text('기록'));
    await tester.pump();
    await tester.pump();
    expect(find.text('위험 잠금 작동'), findsOneWidget);
    expect(find.textContaining('10/25 오후 6:40 ~ 오후 7:01 · 약불 50 %'),
        findsOneWidget);
    expect(find.textContaining('기기에 저장'), findsWidgets);
    // 이번에 켜며 생긴 연결 기록이 DB에도 들어갔다.
    expect(
        db.recentEvents(10).map((e) => e.text), contains('안전 장치 연결 — 모니터링 시작'));
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('기록 탭: DB에 지난 조리가 20회여도 조리 리듬 카드(440)가 넘치지 않는다 (#170 PM 리뷰)',
      (tester) async {
    tester.view.physicalSize = const Size(1920, 1080);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final db = HistoryDb.open(':memory:')!;
    for (var i = 0; i < 20; i++) {
      db.addSession(StoredSession(DateTime(2026, 10, 25, 9, i),
          DateTime(2026, 10, 25, 9, i + 1), '약불 100 %'));
    }
    final feed = DemoKitchenFeed(
        initial: demoScenarios.firstWhere((s) => s.id == 'idle').state!);
    await tester.pumpWidget(
        MaterialApp(home: DashboardScreen(feed: feed, historyDb: db)));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 1600));
    await tester.tap(find.text('기록'));
    await tester.pump();
    await tester.pump();
    expect(tester.takeException(), isNull);
    expect(find.text('지난 조리 · 기기에 저장'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets('기록 탭: 조리 중 + 지난 조리 2줄이어도 리듬 띠·시간 줄이 칩과 겹치지 않는다 (정인 10/26)',
      (tester) async {
    tester.view.physicalSize = const Size(1920, 1080);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    // 시험 기본 글꼴(Ahem)은 줄 높이가 글자 크기 그대로라 실제보다 낮다. 줄이 높은 Noto Sans KR로 잰다.
    await tester.runAsync(() async {
      for (final family in Hs.fallback) {
        final loader = FontLoader(family);
        for (final w in ['Regular', 'Medium']) {
          loader.addFont(File('assets/fonts/NotoSansKR-$w.otf')
              .readAsBytes()
              .then(ByteData.sublistView));
        }
        await loader.load();
      }
    });
    final db = HistoryDb.open(':memory:')!;
    for (var i = 0; i < 2; i++) {
      db.addSession(StoredSession(
          DateTime(2026, 10, 25, 9, i),
          DateTime(2026, 10, 25, 9, i + 1),
          '약불 5 % · 중불 31 % · 강불 64 % · 자리 비움 2번'));
    }
    final feed = DemoKitchenFeed(
        initial: demoScenarios.firstWhere((s) => s.id == 'low').state!);
    await tester.pumpWidget(
        MaterialApp(home: DashboardScreen(feed: feed, historyDb: db)));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 1600));
    await tester.tap(find.text('기록'));
    await tester.pump();
    await tester.pump();
    expect(tester.takeException(), isNull);
    expect(find.text('화력'), findsOneWidget);
    // 시간 줄(시작 시각) 아래쪽이 첫 칩 위쪽보다 위에 있어야 한다.
    final axis = tester.getRect(find.text('지금'));
    final chip = tester.getRect(find.textContaining('조리 ').last);
    expect(axis.bottom, lessThanOrEqualTo(chip.top));
    await tester.pumpWidget(const SizedBox());
  });
}
