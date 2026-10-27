import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/core_state.dart';
import 'package:hood_seek/data/hood_care.dart';
import 'package:hood_seek/data/hood_care_store.dart';
import 'package:hood_seek/data/kitchen_feed.dart';
import 'package:hood_seek/screens/hood_care_card.dart';

/// 데모 idle state에서 팬과 사유 코드만 바꾼다. 계약 파서를 그대로 통과시킨다.
CoreState _state(
    {int level = 2, bool ack = true, List<String> reasons = const []}) {
  final json = demoScenarios.first.state!.toJson();
  final p = json['p']! as Map<String, Object?>;
  p['fan'] = {'level': level, 'min': 0, 'requested': level, 'ack': ack};
  p['reasons'] = reasons;
  return CoreState.fromJson(json);
}

const _half = Duration(milliseconds: 500);
final _day1 = DateTime(2026, 10, 25, 19);
final _day2 = DateTime(2026, 10, 26, 19);

void _run(HoodCare care, CoreState? s, int samples, DateTime now) {
  for (var i = 0; i < samples; i++) {
    care.sample(s, _half, now);
  }
}

void main() {
  group('sample', () {
    test('팬 꺼짐·ack=false·연결 끊김(null) 동안은 누적하지 않는다', () {
      final care = HoodCare();
      _run(care, _state(level: 0), 10, _day1);
      _run(care, _state(level: 3, ack: false), 10, _day1);
      _run(care, _state(level: 3, ack: false, reasons: ['SMOKE']), 10, _day1);
      _run(care, null, 10, _day1);
      expect((care.levelSeconds, care.smokeSeconds), (0, 0));
      expect(care.daily, isEmpty);
    });

    test('가중 가동 = 경과 초 × 단수, 연기 중 가동은 단수 없이 따로 센다', () {
      final care = HoodCare();
      _run(care, _state(level: 3), 10, _day1); // 5 s × 3단 = 15
      _run(care, _state(level: 1, reasons: ['PM_WARN']), 4, _day1); // 2 s × 1단
      _run(care, _state(level: 3, reasons: ['SMOKE', 'TEMP_DANGER']), 2,
          _day1); // 1 s × 3단
      _run(care, _state(level: 2, reasons: ['TEMP_HIGH']), 2, _day1); // 연기 아님
      expect(care.levelSeconds, closeTo(15 + 2 + 3 + 2, 1e-9));
      expect(care.smokeSeconds, closeTo(2 + 1, 1e-9));
      expect(care.daily, {'2026-10-25': closeTo(22, 1e-9)});
    });

    test('간격은 state 만료(3 s)까지만, 0·음수 간격은 더하지 않는다', () {
      final care = HoodCare();
      care.sample(_state(level: 2), const Duration(hours: 5), _day1);
      care.sample(_state(level: 2), Duration.zero, _day1);
      care.sample(_state(level: 2), const Duration(seconds: -4), _day1);
      expect(care.levelSeconds, 6);
    });

    test('하루 기록은 마지막 기록일부터 1년 안의 날만 — 시계가 뒤로 가거나 1년 넘게 뛰면 누적만 센다', () {
      final care = HoodCare();
      _run(care, _state(level: 1), 2, _day1);
      _run(care, _state(level: 1), 2,
          DateTime(1970, 1, 1, 9)); // 배터리 없는 시계가 초기화됨
      expect(care.daily, {'2026-10-25': 1.0});
      expect(care.levelSeconds, 2.0); // 누적은 날짜와 무관하게 센다
      expect(care.clockSuspect, isTrue);
      _run(care, _state(level: 1), 2, _day1.add(const Duration(days: 367)));
      expect(care.daily.length, 1);
      expect(care.clockSuspect, isTrue);
      _run(care, _state(level: 2), 2, _day2); // 시계를 맞추면 다시 기록
      expect(care.daily, {'2026-10-25': 1.0, '2026-10-26': 2.0});
      expect(care.clockSuspect, isFalse);
      care.clean(_day2);
      _run(care, _state(level: 1), 2, DateTime(2030, 1, 1)); // 청소하면 기준이 지워진다
      expect(care.daily, {'2030-01-01': 1.0});
    });

    test('기기 로컬 날짜별로 나눠 담는다', () {
      final care = HoodCare();
      _run(care, _state(level: 1), 2, _day1);
      _run(care, _state(level: 2), 2, _day2);
      expect(care.daily, {'2026-10-25': 1.0, '2026-10-26': 2.0});
    });
  });

  test('clean: 가중·연기 중·날짜별을 0으로, 청소 시각을 남긴다', () {
    final care = HoodCare();
    _run(care, _state(level: 3, reasons: ['PM_WARN']), 4, _day1);
    care.clean(_day2);
    expect((care.levelSeconds, care.smokeSeconds), (0, 0));
    expect(care.daily, isEmpty);
    expect(care.cleanedAt, _day2);
    expect(care.daysLeft(), isNull);
  });

  group('daysLeft·progress', () {
    test('기록된 날이 2일 미만이면 null', () {
      final care = HoodCare(levelSeconds: 10800, daily: {'2026-10-25': 10800});
      expect(care.daysLeft(), isNull);
      expect(HoodCare().daysLeft(), isNull);
    });

    test('2일 이상이면 남은 양 ÷ 하루 평균을 올림', () {
      // 남은 194400 ÷ 평균 10800 = 18
      expect(
          HoodCare(
              levelSeconds: 21600,
              daily: {'2026-10-25': 10800, '2026-10-26': 10800}).daysLeft(),
          18);
      // 남은 196000 ÷ 평균 10000 = 19.6 → 20
      expect(
          HoodCare(
              levelSeconds: 20000,
              daily: {'2026-10-25': 5000, '2026-10-26': 15000}).daysLeft(),
          20);
    });

    test('기준을 넘으면 progress ≥ 1, 남은 날 0', () {
      final care = HoodCare(
          levelSeconds: cleanAfterLevelSeconds + 1,
          daily: {'2026-10-25': 1, '2026-10-26': cleanAfterLevelSeconds});
      expect(care.progress, greaterThanOrEqualTo(1));
      expect(care.daysLeft(), 0);
      expect(HoodCare(levelSeconds: cleanAfterLevelSeconds / 4).progress, .25);
    });
  });

  group('JSON', () {
    test('왕복하면 같은 값', () {
      final care = HoodCare();
      _run(care, _state(level: 2, reasons: ['SMOKE']), 3, _day1);
      care.cleanedAt = DateTime(2026, 10, 20, 8, 30);
      _run(care, _state(level: 1), 1, _day2);
      final json =
          jsonDecode(jsonEncode(care.toJson())) as Map<String, Object?>;
      final back = HoodCare.fromJson(json);
      expect(back.toJson(), care.toJson());
      expect(back.cleanedAt!.isAtSameMomentAs(care.cleanedAt!), isTrue);
      expect(back.cleanedAt!.isUtc, isFalse);
    });

    test('모르는 키·빠진 키·잘못된 타입은 FormatException', () {
      Map<String, Object?> ok() => HoodCare(levelSeconds: 1).toJson();
      final bad = <Map<String, Object?>>[
        {...ok(), 'extra': 1},
        ok()..remove('daily'),
        {...ok(), 'level_seconds': '12'},
        {...ok(), 'smoke_seconds': -1},
        {...ok(), 'cleaned_at': 12},
        {...ok(), 'cleaned_at': '어제'},
        {...ok(), 'daily': []},
        {
          ...ok(),
          'daily': {'10/26': 1}
        },
        {
          ...ok(),
          'daily': {'2026-10-26': null}
        },
      ];
      for (final json in bad) {
        expect(() => HoodCare.fromJson(json), throwsFormatException,
            reason: '$json');
      }
      expect(HoodCare.fromJson({...ok(), 'level_seconds': 7}).levelSeconds, 7);
    });
  });

  group('HoodCareStore', () {
    late Directory dir;
    setUp(() => dir = Directory.systemTemp.createTempSync('hood_care_'));
    tearDown(() => dir.deleteSync(recursive: true));

    test('저장한 값을 다시 읽고 임시 파일은 남지 않는다', () async {
      final path = '${dir.path}/sub/hood-care.json';
      final store = HoodCareStore(path);
      final care = HoodCare(
          levelSeconds: 42,
          smokeSeconds: 3,
          cleanedAt: DateTime(2026, 10, 1, 9),
          daily: {'2026-10-26': 42});
      expect(await store.save(care), isTrue);
      care.levelSeconds = 50; // 부른 뒤 바뀐 값은 저장되지 않는다
      final (back, notice) = await HoodCareStore(path).load();
      expect(notice, isNull);
      expect(back.levelSeconds, 42);
      expect(back.daily, {'2026-10-26': 42});
      expect(File('$path.tmp').existsSync(), isFalse);
    });

    test('예전 앱이 남긴 덮어쓸 수 없는 .tmp가 있어도 지우고 저장한다', () async {
      final path = '${dir.path}/hood-care.json';
      File('$path.tmp').writeAsStringSync('예전 앱');
      Process.runSync('chmod', ['444', '$path.tmp']); // 다른 계정 파일처럼 쓰기 불가
      expect(await HoodCareStore(path).save(HoodCare(levelSeconds: 7)), isTrue);
      expect((await HoodCareStore(path).load()).$1.levelSeconds, 7);
      expect(File('$path.tmp').existsSync(), isFalse);
    }, skip: Platform.isWindows);

    test('겹친 저장은 부른 순서대로 끝난다', () async {
      final path = '${dir.path}/hood-care.json';
      final store = HoodCareStore(path);
      final results = await Future.wait([
        store.save(HoodCare(levelSeconds: 1)),
        store.save(HoodCare(levelSeconds: 2)),
      ]);
      expect(results, [true, true]);
      expect((await store.load()).$1.levelSeconds, 2);
    });

    test('없는 파일은 0부터, 알림 없음', () async {
      final (care, notice) =
          await HoodCareStore('${dir.path}/none.json').load();
      expect(care.levelSeconds, 0);
      expect(care.daily, isEmpty);
      expect(notice, isNull);
    });

    test('깨진 파일은 0부터 + 알림, 원본은 .broken으로 남긴다', () async {
      final path = '${dir.path}/hood-care.json';
      for (final text in ['{"level_seconds": 3', '[1]', '{"x": 1}']) {
        File(path).writeAsStringSync(text);
        final (care, notice) = await HoodCareStore(path).load();
        expect(care.levelSeconds, 0, reason: text);
        expect(notice, hoodCareLoadFailed, reason: text);
        expect(File('$path.broken').readAsStringSync(), text);
      }
    });

    test('읽기·쓰기 실패(경로가 디렉터리)는 멈추지 않고 알림·false', () async {
      // 경로가 디렉터리라 파일로 읽거나 그 위로 rename할 수 없다.
      final path = '${dir.path}/d';
      Directory(path).createSync();
      final store = HoodCareStore(path);
      final (care, notice) = await store.load();
      expect((care.levelSeconds, notice), (0, hoodCareLoadFailed));
      expect(await store.save(HoodCare(levelSeconds: 1)), isFalse);
    });
  });

  group('HoodCareCard', () {
    Future<void> pump(WidgetTester tester, HoodCare care,
        {String? notice, VoidCallback? onCleaned}) async {
      tester.view.physicalSize = const Size(1920, 1080);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.reset);
      await tester.pumpWidget(MaterialApp(
        home: Scaffold(
          body: Center(
            child: SizedBox(
              width: 750,
              height: 162,
              child: HoodCareCard(
                  care: care, notice: notice, onCleaned: onCleaned ?? () {}),
            ),
          ),
        ),
      ));
    }

    testWidgets('「청소했어요」 → 확인 창 → 되돌리기에서만 onCleaned', (tester) async {
      var cleaned = 0;
      await pump(tester, HoodCare(), onCleaned: () => cleaned++);
      expect(find.textContaining('청소 기록 없음'), findsOneWidget);
      expect(find.text('기록이 쌓이면 알려 드릴게요'), findsOneWidget);
      expect(find.text('기준 60 단·시간 (제안)'), findsOneWidget);

      await tester.tap(find.text('청소했어요'));
      await tester.pumpAndSettle();
      expect(find.text('누적을 0으로 되돌릴까요?'), findsOneWidget);
      await tester.tap(find.text('취소'));
      await tester.pumpAndSettle();
      expect(find.text('누적을 0으로 되돌릴까요?'), findsNothing);
      expect(cleaned, 0);

      await tester.tap(find.text('청소했어요'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('되돌리기'));
      await tester.pumpAndSettle();
      expect(find.text('누적을 0으로 되돌릴까요?'), findsNothing);
      expect(cleaned, 1);
    });

    testWidgets('값·예측·청소 시각·알림을 750×162에 그린다 (#156 배치)', (tester) async {
      await pump(
        tester,
        HoodCare(
          levelSeconds: 21600,
          smokeSeconds: 3 * 3600 + 5 * 60,
          cleanedAt: DateTime(2026, 10, 20, 19, 42),
          daily: {'2026-10-25': 10800, '2026-10-26': 10800},
        ),
        notice: hoodCareSaveFailed,
      );
      expect(find.text('6.0'), findsOneWidget);
      expect(find.text('10%'), findsOneWidget);
      expect(find.textContaining('연기 중 가동 3시간 5분'), findsOneWidget);
      expect(find.text('이 속도면 약 18일 뒤 청소할 때예요'), findsOneWidget);
      expect(find.textContaining('마지막 청소 10월 20일'), findsOneWidget);
      expect(find.textContaining(hoodCareSaveFailed), findsOneWidget);
    });

    testWidgets('시계가 기간 밖이면 시계 알림, 저장 알림이 있으면 그쪽이 먼저', (tester) async {
      final care = HoodCare(daily: {'2026-10-26': 60});
      _run(care, _state(level: 1), 1, DateTime(1970, 1, 1));
      await pump(tester, care);
      expect(find.textContaining('기기 시계를 확인해 주세요'), findsOneWidget);
      await pump(tester, care, notice: hoodCareSaveFailed);
      expect(find.textContaining(hoodCareSaveFailed), findsOneWidget);
      expect(find.textContaining('기기 시계를 확인해 주세요'), findsNothing);
    });

    testWidgets('기준을 넘으면 「청소할 때가 됐어요」', (tester) async {
      await pump(tester, HoodCare(levelSeconds: 259200));
      expect(find.text('청소할 때가 됐어요'), findsOneWidget);
      expect(find.text('120%'), findsOneWidget);
    });
  });
}
