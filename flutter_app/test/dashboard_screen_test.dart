import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/app.dart';
import 'package:hood_seek/audio/clip_player.dart';
import 'package:hood_seek/brand.dart';
import 'package:hood_seek/data/core_state.dart';
import 'package:hood_seek/data/kitchen_feed.dart';
import 'package:hood_seek/data/ui_command.dart';
import 'package:hood_seek/screens/home_view.dart';
import 'package:hood_seek/screens/overlays.dart';
import 'package:hood_seek/widgets/hs.dart';

CoreState _scenario(String id) =>
    demoScenarios.firstWhere((s) => s.id == id).state!;

/// 시연 화면(1920×1080)과 리허설 칩이 다 들어가는 크기로 띄우고 시작 화면(1.5 s)을 넘긴다.
Future<DemoKitchenFeed> _start(WidgetTester tester, DemoKitchenFeed feed,
    {BrandImages brand = BrandImages.none,
    ClipPlayer clips = const SilentClipPlayer()}) async {
  tester.view.physicalSize = const Size(1920, 1200);
  tester.view.devicePixelRatio = 1;
  addTearDown(tester.view.reset);
  await tester.pumpWidget(HoodSeekApp(feed: feed, brand: brand, clips: clips));
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 1600));
  return feed;
}

Future<DemoKitchenFeed> _pump(WidgetTester tester, [String id = 'low']) =>
    _start(tester, DemoKitchenFeed(initial: _scenario(id)));

/// 시나리오 state의 JSON을 고쳐서 띄운다 (계약 파서를 그대로 통과시킨다).
Future<DemoKitchenFeed> _pumpEdited(WidgetTester tester, String id,
    void Function(Map<String, Object?> p) edit) {
  final json = _scenario(id).toJson();
  edit(json['p']! as Map<String, Object?>);
  return _start(tester, DemoKitchenFeed(initial: CoreState.fromJson(json)));
}

Future<void> _play(
    WidgetTester tester, DemoKitchenFeed feed, CoreState s) async {
  feed.play(s);
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 400));
}

Future<void> _tap(WidgetTester tester, Finder finder) async {
  await tester.tap(finder);
  await tester.pump();
  await tester.pump(const Duration(milliseconds: 400));
}

Iterable<(UiCmd, int)> _sent(DemoKitchenFeed feed) =>
    feed.sent.map((c) => (c.cmd, c.value));

/// 낸 경고음 이름을 순서대로 모은다.
class _Clips implements ClipPlayer {
  final played = <String>[];

  @override
  void play(String clip) => played.add(clip);
}

class _FailedSendFeed extends DemoKitchenFeed {
  _FailedSendFeed() : super(initial: _scenario('idle'));

  @override
  Future<void> send(UiCommand command) async => throw StateError('send failed');
}

/// LG 로고의 화면 읽기 이름. 의미 트리 묶는 방식이 Flutter 버전마다 달라서
/// (ATLAS 3.27.4와 최신 stable) 트리 대신 위젯 값을 본다.
String? _logoLabel(WidgetTester tester) => tester
    .widget<Semantics>(find
        .descendant(of: find.byType(LgLogo), matching: find.byType(Semantics))
        .first)
    .properties
    .label;

/// 기기 시트의 단수 버튼. [column] 0 = 인덕션, 1 = 후드.
LevelButton _level(WidgetTester tester, String label, int column) => tester
    .widgetList<LevelButton>(find.widgetWithText(LevelButton, label))
    .elementAt(column);

/// 옛 Core(state v1·v2)가 보내던 이름으로 되돌린다: mid → caution, TEMP_MID → TEMP_CAUTION, t_mid → t_caution 등.
void _legacyNames(Map<String, Object?> json) {
  final p = json['p']! as Map<String, Object?>;
  p['hazard'] = const {
        'low': 'normal',
        'mid': 'caution',
        'high': 'warning'
      }[p['hazard']] ??
      p['hazard'];
  p['reasons'] = [
    for (final r in p['reasons']! as List)
      const {'TEMP_MID': 'TEMP_CAUTION', 'TEMP_HIGH': 'TEMP_WARN'}[r] ?? r
  ];
  final t = p['thresholds']! as Map<String, Object?>;
  t['t_caution'] = t.remove('t_mid');
  t['t_warn'] = t.remove('t_high');
}

void main() {
  testWidgets('기동음은 앱을 켤 때1회이고 state 갱신·탭 전환에는 반복하지 않는다', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('idle')),
        clips: clips);
    await _play(tester, feed, _scenario('low'));
    await _tap(tester, find.text('센서'));
    expect(clips.played, ['APP_START']);
  });

  testWidgets('후드 조작음은 누름·전송이 아닌 요청 단수와 ACK 반영 뒤1회', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('idle')),
        clips: clips);
    await _tap(tester, find.text('기기'));
    await _tap(tester, find.widgetWithText(LevelButton, '2').last);
    expect(clips.played, ['APP_START']);
    final json = _scenario('idle').toJson();
    final p = json['p']! as Map<String, Object?>;
    p['fan'] = {'level': 2, 'min': 0, 'requested': 2, 'ack': false};
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START']);
    (p['fan']! as Map)['ack'] = true;
    await _play(tester, feed, CoreState.fromJson(json));
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'TOUCH_FEEDBACK']);
    // 이미 같은 단수면 새 확인음을 예약하지 않는다.
    await _tap(tester, find.widgetWithText(LevelButton, '2').last);
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'TOUCH_FEEDBACK']);
  });

  testWidgets('인덕션 요청과 다른 변화·안전 하한 보정에는 조작음을 내지 않는다', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('idle')),
        clips: clips);
    await _tap(tester, find.text('켜기'));
    final json = _scenario('idle').toJson();
    final p = json['p']! as Map<String, Object?>;
    p['induction'] = {'level': 2, 'allowed_on': true, 'ack': true};
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START']);
    p['induction'] = {'level': 1, 'allowed_on': true, 'ack': true};
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'TOUCH_FEEDBACK']);
    await _tap(tester, find.text('기기'));
    await _tap(tester, find.widgetWithText(LevelButton, '1').last);
    p['fan'] = {'level': 3, 'min': 3, 'requested': 1, 'ack': true};
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'TOUCH_FEEDBACK']);
  });

  testWidgets('거부·3초 뒤 늦은 상태에는 조작 확인음을 내지 않는다', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('idle')),
        clips: clips);
    await _tap(tester, find.text('켜기'));
    final rejected = _scenario('idle').toJson();
    (rejected['p']! as Map)['alert'] = 'CMD_REJECTED';
    await _play(tester, feed, CoreState.fromJson(rejected));
    final on = _scenario('low');
    await _play(tester, feed, on);
    expect(clips.played, ['APP_START', 'CMD_REJECTED']);
    await _tap(tester, find.text('기기'));
    await _tap(tester, find.widgetWithText(LevelButton, '3').last);
    await tester.pump(const Duration(seconds: 4));
    final later = on.toJson();
    (later['p']! as Map)['fan'] = {
      'level': 3,
      'min': 0,
      'requested': 3,
      'ack': true
    };
    await _play(tester, feed, CoreState.fromJson(later));
    expect(clips.played, ['APP_START', 'CMD_REJECTED']);
  });

  testWidgets('전송 실패 뒤 일치하는 state가 와도 확인음은 없다', (tester) async {
    final clips = _Clips();
    final feed = await _start(tester, _FailedSendFeed(), clips: clips);
    await _tap(tester, find.text('켜기'));
    expect(find.text('명령을 보내지 못했어요. 연결을 확인해 주세요.'), findsOneWidget);
    final on = _scenario('idle').toJson();
    (on['p']! as Map)['induction'] = {
      'level': 1,
      'allowed_on': true,
      'ack': true
    };
    await _play(tester, feed, CoreState.fromJson(on));
    expect(clips.played, ['APP_START']);
  });

  testWidgets('정상 조리: state 값과 배지·헤드라인 매핑을 보인다', (tester) async {
    await _pump(tester);

    expect(find.text('정상 · 모든 센서 작동 중'), findsOneWidget);
    expect(find.text('안전하게 조리 중입니다'), findsOneWidget);
    expect(find.text('14.6'), findsOneWidget);
    expect(find.text('9'), findsOneWidget);
    expect(find.text('인덕션'), findsOneWidget);
    expect(find.text('2단'), findsNWidgets(2));
    expect(find.text('사람 있음 · 반려동물 없음'), findsOneWidget);
    expect(find.text('위험 확인'), findsNothing);
    expect(find.text('자동 조절 중'), findsNothing);
  });

  testWidgets('대기: 인덕션·팬이 모두 꺼졌을 때만, 「켜기」는 IND_SET 1 요청만', (tester) async {
    final feed = await _pump(tester, 'idle');

    expect(find.text('대기 · 모든 센서 작동 중'), findsOneWidget);
    expect(find.text('조리를 시작해 볼까요?'), findsOneWidget);
    await _tap(tester, find.text('켜기'));
    expect(_sent(feed), [(UiCmd.indSet, 1)]);
    expect(find.text('꺼짐'), findsNWidgets(2));
  });

  testWidgets('대기 문구는 재실·allowed_on 그대로 (#162)', (tester) async {
    for (final (presence, allowed, title) in [
      ('present', false, '곧 조리를 시작할 수 있어요'),
      ('absent', false, '빈 주방을 안전하게 지키고 있어요'),
      ('unknown', false, '조리대 앞에 서면 시작할 수 있어요'),
    ]) {
      await tester.pumpWidget(const SizedBox()); // 앞 화면 상태를 버린다
      await _pumpEdited(tester, 'idle', (p) {
        p['presence'] = presence;
        (p['induction']! as Map)['allowed_on'] = allowed;
      });
      expect(find.text(title), findsOneWidget, reason: presence);
      expect(find.text('조리를 시작해 볼까요?'), findsNothing, reason: presence);
    }
  });

  testWidgets('홈 후드 카드는 후드만, 인덕션 카드는 인덕션만, 도크 「기기」는 둘 다 (#162)',
      (tester) async {
    await _pump(tester);
    Future<void> openAt(Type card) async {
      await tester
          .tapAt(tester.getTopLeft(find.byType(card)) + const Offset(24, 24));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 600));
    }

    await openAt(HoodCard);
    expect(find.text('기기 제어'), findsNothing);
    expect(find.byType(LevelButton), findsNWidgets(4));
    expect(find.text('잠그기'), findsNothing); // 사용자 잠금 버튼은 인덕션이 보이는 창에만
    await _tap(tester, find.byType(CloseCircle));

    await openAt(InductionCard);
    expect(find.byType(LevelButton), findsNWidgets(4));
    expect(find.text('잠그기'), findsOneWidget);
    await _tap(tester, find.byType(CloseCircle));

    await _tap(tester, find.text('기기'));
    expect(find.text('기기 제어'), findsOneWidget);
    expect(find.byType(LevelButton), findsNWidgets(8));
  });

  testWidgets('기기 시트: 후드 버튼은 FAN_SET 요청만 보내고 화면 값은 state를 따른다', (tester) async {
    final feed = await _pump(tester);
    await _tap(tester, find.text('기기'));
    expect(find.text('기기 제어'), findsOneWidget);

    await _tap(tester, find.widgetWithText(LevelButton, '3').last);
    expect(_sent(feed), [(UiCmd.fanSet, 3)]);
    expect(_level(tester, '2', 1).selected, isTrue);
  });

  testWidgets('기기 시트: 꺼져 있으면 굵은 「꺼짐」만 보이고 아래 연한 줄은 비운다 (정인 10/27)',
      (tester) async {
    await _pump(tester, 'idle');
    await _tap(tester, find.text('기기'));
    final hidden = find.ancestor(
        of: find.text('꺼짐'),
        matching: find.byWidgetPredicate((w) => w is Visibility && !w.visible));
    expect(hidden, findsNWidgets(2)); // 인덕션·후드 연한 줄
    expect(find.text('가열 중'), findsNothing);
    expect(find.text('환기 중'), findsNothing);
  });

  testWidgets('사용자 잠금: 기기 시트에서 USER_LOCK 1·0만 보내고, 화면은 state를 따른다 (D-058)',
      (tester) async {
    final feed = await _pump(tester, 'idle');
    await _tap(tester, find.text('기기'));
    await _tap(tester, find.text('잠그기'));
    expect(_sent(feed), [(UiCmd.userLock, 1)]);
    // 잠금 여부는 Core가 state로 돌려준 값만 따른다 — 보냈다고 화면이 먼저 바뀌지 않는다.
    expect(find.text('잠그기'), findsOneWidget);

    await _play(tester, feed, _scenario('user-lock'));
    expect(_level(tester, '1', 0).onPressed, isNull); // allowed_on=false라 켜기 막힘
    await _tap(tester, find.text('잠금 풀기'));
    expect(_sent(feed), [(UiCmd.userLock, 1), (UiCmd.userLock, 0)]);
  });

  testWidgets('사용자 잠금: 헤드라인과 인덕션 카드 (D-058)', (tester) async {
    await _pump(tester, 'user-lock');
    expect(find.text('사용자 잠금'), findsWidgets); // 배지 알약 + 리허설 칩
    expect(find.text('인덕션을 잠가 두었어요'), findsOneWidget);
    expect(find.text('기기 제어에서 잠금을 풀 수 있어요'), findsOneWidget);
  });

  testWidgets('위험·반려동물 잠금 중에는 사용자 잠금 버튼을 누를 수 없다 (불변식 ⑨)', (tester) async {
    final feed = await _pump(tester, 'cat-lock');
    await _tap(tester, find.widgetWithText(HsButton, '확인'));
    await _tap(tester, find.text('기기'));
    expect(
        tester
            .widget<TextButton>(find.widgetWithText(TextButton, '잠그기'))
            .onPressed,
        isNull);
    expect(feed.sent, isEmpty);
  });

  testWidgets('후드는 fan.min 아래 단수를 보낼 수 없다 (불변식 ②)', (tester) async {
    await _pump(tester, 'mid');
    await _tap(tester, find.text('기기'));

    expect(_level(tester, '끄기', 1).onPressed, isNull);
    expect(_level(tester, '1', 1).onPressed, isNull);
    expect(_level(tester, '2', 1).onPressed, isNotNull);
    expect(find.text('요청 꺼짐 → 안전 최소 2단 유지'), findsWidgets);
    // 안전 장치가 요청(0)보다 올렸을 때만 후드 카드에 「자동 조절 중」 (PM 결정 10/24).
    expect(find.text('자동 조절 중'), findsOneWidget);
  });

  testWidgets('allowed_on=false면 켜기·올리기만 비활성, 끄기·내리기는 보낸다 (F6)',
      (tester) async {
    final feed = await _pumpEdited(tester, 'low', (p) {
      p['induction'] = {'level': 2, 'allowed_on': false, 'ack': true};
    });
    await _tap(tester, find.text('기기'));

    expect(_level(tester, '3', 0).onPressed, isNull);
    expect(_level(tester, '2', 0).onPressed, isNotNull);
    await _tap(tester, find.widgetWithText(LevelButton, '1').first);
    await _tap(tester, find.widgetWithText(LevelButton, '끄기').first);
    expect(_sent(feed), [(UiCmd.indSet, 1), (UiCmd.indSet, 0)]);
  });

  testWidgets('고양이 잠금: 오른쪽 대화상자(누를 수 없는 버튼 없이 「확인」만), 인덕션 잠김', (tester) async {
    await _pump(tester, 'cat-lock');

    expect(find.text('경고 · 반려동물 감지'), findsOneWidget);
    expect(find.text('조리 공간에\n반려동물이 있어요'), findsOneWidget);
    // Core가 조건이 맞으면 푼다 — 「지금 켜기」를 두지 않는다 (정인 10/26).
    expect(find.text('지금 켜기'), findsNothing);
    expect(find.text('잠김'), findsOneWidget);
    await _tap(tester, find.widgetWithText(HsButton, '확인'));
    expect(find.text('조리 공간에\n반려동물이 있어요'), findsNothing);
    expect(find.text('반려동물이 조리 공간에 있어요'), findsOneWidget);
  });

  testWidgets('고양이 잠금 중 카메라 판정이 끊기면 「있어요」라고 하지 않는다', (tester) async {
    // Core는 vision 만료 시 CAT_NEAR를 빼고 presence=unknown, 잠금은 남긴다.
    await _pumpEdited(tester, 'cat-lock', (p) {
      p['presence'] = 'unknown';
      p['vision'] = {'fresh': false, 'person': false, 'cat': false};
      p['reasons'] = ['VISION_STALE'];
    });

    expect(find.text('경고 · 반려동물 잠금 중'), findsOneWidget);
    expect(find.text('반려동물\n잠금 중이에요'), findsOneWidget);
    expect(find.text('카메라 판정 없음'), findsWidgets);
    expect(find.text('조리 공간에\n반려동물이 있어요'), findsNothing);
    await _tap(tester, find.widgetWithText(HsButton, '확인'));
    expect(find.text('반려동물 잠금 중이에요'), findsOneWidget);
    expect(find.text('사람이 오면 풀려요'), findsOneWidget);
    expect(find.text('반려동물이 조리 공간에 있어요'), findsNothing);
  });

  testWidgets('vision.fresh=false면 cat=true여도 고양이를 그리지 않는다 (불변식 ③)',
      (tester) async {
    await _pumpEdited(tester, 'low', (p) {
      p['vision'] = {'fresh': false, 'person': false, 'cat': true};
    });

    expect(find.text('카메라 판정 없음'), findsOneWidget);
    expect(find.text('고양이'), findsNothing);
    expect(find.text('정상 · 카메라 정보 없음'), findsOneWidget);
  });

  testWidgets('화재 위험: 가운데 대화상자, 「확인했어요」는 닫기만 하고 명령을 보내지 않는다', (tester) async {
    final feed = await _pump(tester, 'fire');

    expect(find.text('위험 · 화재 위험'), findsOneWidget);
    // 헤드라인과 대화상자 제목이 같은 문장이다 (P-011 §5 안내 1줄).
    expect(find.text('화재 위험을 미리 감지해 인덕션을 껐어요'), findsNWidgets(2));
    expect(
        find.text('후드가 최대로 환기하고 있어요. 뜨거운 조리 기구는 그대로 두세요.\n'
            '연기가 걷히고 온도가 내려가면 확인 후 다시 쓸 수 있어요.'),
        findsOneWidget);
    // 위험 중에는 해제 조건 한 줄: ① 위험 진행 중, ② 위험이 내려간 뒤에만 확인 가능.
    expect(find.text('위험 진행 중'), findsOneWidget);
    expect(find.text('위험 확인 — 위험이 내려간 뒤'), findsOneWidget);
    await _tap(tester, find.text('확인했어요'));
    expect(find.byType(CenterAlert), findsNothing);
    expect(find.text('화재 위험을 미리 감지해 인덕션을 껐어요'), findsOneWidget);
    expect(feed.sent, isEmpty);
    expect(find.text('잠김'), findsOneWidget);
  });

  testWidgets('해제 대기: 3조건은 state 그대로, 「위험 확인」은 ACK_ALERT만 (불변식 ⑨)',
      (tester) async {
    final feed = await _pump(tester, 'ack-wait');

    expect(find.text('위험 해소 · 확인 필요'), findsOneWidget);
    expect(find.text('위험 상황이 끝났어요'), findsOneWidget);
    // ①은 Core가 보낸 남은 초(state v2 `lock_release_left_s`)를 그대로 옮긴다 (#116).
    expect(find.text('7초 남음 · 10초 중'), findsOneWidget);
    // 진행 원은 지난 비율 (10 − 7) / 10. 앱이 세는 시간이 아니다.
    final ring = tester.widget<CircularProgressIndicator>(find.descendant(
        of: find.byType(ConditionCard).first,
        matching: find.byType(CircularProgressIndicator)));
    expect(ring.value, closeTo(0.3, 1e-9));
    expect(find.text('아래 버튼을 눌러 주세요'), findsOneWidget);
    expect(find.text('주방에 계세요'), findsOneWidget);
    // LG 레드는 위험 대화상자의 1차 버튼에만 (설계 v1 §2.1).
    expect(
        tester
            .widget<HsButton>(find.widgetWithText(HsButton, '위험 확인'))
            .background,
        Hs.lgRed);

    // ACK는 요청만 보내고 대화상자를 닫지 않는다. 잠금은 그대로다.
    await _tap(tester, find.widgetWithText(HsButton, '위험 확인'));
    expect(_sent(feed), [(UiCmd.ackAlert, 0)]);
    expect(find.text('위험 상황이 끝났어요'), findsOneWidget);
    expect(find.text('잠김'), findsOneWidget);

    // Core가 UNLOCK_WAIT로 바꾸면 카드가 바뀌고 1차 버튼은 「닫기」.
    await _play(tester, feed, _scenario('unlock-wait'));
    expect(find.text('확인됨 · 해제 대기'), findsOneWidget);
    expect(find.text('안정 시간을 채웠어요'), findsOneWidget);
    expect(find.text('자리를 비우셨어요'), findsOneWidget);
    expect(find.widgetWithText(HsButton, '위험 확인'), findsNothing);
    await _tap(tester, find.widgetWithText(HsButton, '닫기'));
    expect(find.byType(ReleaseAlert), findsNothing);

    // 닫은 뒤 인덕션 카드는 충족 수를 보이고, 누르면 대화상자를 다시 연다.
    expect(find.text('잠금 해제 조건 2/3 충족'), findsOneWidget);
    await _tap(tester, find.text('잠금 해제 조건 2/3 충족'));
    expect(find.byType(ReleaseAlert), findsOneWidget);
    expect(_sent(feed), [(UiCmd.ackAlert, 0)]);

    await _play(tester, feed, _scenario('low'));
    expect(find.byType(ReleaseAlert), findsNothing);
    expect(find.text('위험 확인'), findsNothing);
  });

  testWidgets('「위험 확인」은 ACK_WAIT일 때만, 남은 조건이 하나면 그렇게 말한다', (tester) async {
    // ACK_WAIT도 UNLOCK_WAIT도 없으면 버튼 대신 「닫기」 (#95 시험 6·7).
    final feed = await _pumpEdited(tester, 'ack-wait', (p) {
      p['reasons'] = ['TEMP_MID', 'HOLD'];
    });
    expect(find.byType(ReleaseAlert), findsOneWidget);
    expect(find.widgetWithText(HsButton, '위험 확인'), findsNothing);
    expect(find.widgetWithText(HsButton, '닫기'), findsOneWidget);

    // 확인 뒤 재실 중이면 남은 것은 안전 시간 하나뿐이다.
    final json = _scenario('unlock-wait').toJson();
    final p = json['p']! as Map<String, Object?>;
    p['presence'] = 'present';
    p['vision'] = {'fresh': true, 'person': true, 'cat': false};
    p['reasons'] = ['TEMP_MID', 'UNLOCK_WAIT'];
    p['lock_release_left_s'] = 3;
    await _play(tester, feed, CoreState.fromJson(json));
    expect(find.text('남은 조건은 하나예요. 채워지면 인덕션 잠금이 자동으로 풀려요.'), findsOneWidget);
    expect(find.text('3초 남음 · 10초 중'), findsOneWidget);
    expect(feed.sent, isEmpty);
  });

  testWidgets('state v1이면 남은 초 없이 ①은 「지켜보는 중」 (Core를 나중에 올려도 됨)',
      (tester) async {
    final json = _scenario('ack-wait').toJson();
    final p = json['p']! as Map<String, Object?>;
    json['v'] = 1;
    _legacyNames(json);
    p.remove('lock_release_left_s');
    (p['thresholds']! as Map).remove('lock_release_s');
    await _start(tester, DemoKitchenFeed(initial: CoreState.fromJson(json)));
    expect(find.text('위험 상황이 끝났어요'), findsOneWidget);
    expect(find.text('안정될 때까지 지켜보는 중'), findsOneWidget);
    expect(find.textContaining('초 남음'), findsNothing);
  });

  testWidgets('해제 카운트다운(M10): Core가 보낸 남은 초가 줄고, 0이 되는 tick에 Core가 푼다',
      (tester) async {
    final release = demoScenarios.firstWhere((s) => s.id == 'release');
    final feed = await _start(tester, DemoKitchenFeed(initial: release.state));
    feed.play(release.state!, then: release.then);
    await tester.pump();
    expect(find.text('확인했어요'), findsWidgets);
    expect(find.text('10초 남음 · 10초 중'), findsOneWidget);

    // 데모 피드는 Core처럼 0.5 s마다 state를 보낸다. 올림이라 같은 숫자가 두 번 온다.
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.text('10초 남음 · 10초 중'), findsOneWidget);
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.text('9초 남음 · 10초 중'), findsOneWidget);

    // 대화상자를 닫으면 인덕션 카드에 충족 수와 남은 초가 남는다.
    await _tap(tester, find.widgetWithText(HsButton, '닫기'));
    expect(find.text('잠금 해제 조건 2/3 충족 · 9초 남음'), findsOneWidget);

    await tester.pump(const Duration(seconds: 8));
    expect(find.text('잠금 해제 조건 2/3 충족 · 1초 남음'), findsOneWidget);
    await tester.pump(const Duration(seconds: 1));
    expect(find.text('잠김'), findsNothing);
    expect(find.textContaining('초 남음'), findsNothing);
    expect(feed.sent, isEmpty);
  });

  testWidgets('위험 잠금이 state에서 풀리면 복구 토스트와 「잠금 풀림」', (tester) async {
    final feed = await _pump(tester, 'ack-wait');
    await _tap(tester, find.widgetWithText(HsButton, '위험 확인'));

    await _play(tester, feed, _scenario('idle'));
    expect(find.text('확인했어요 · 인덕션을 다시 쓸 수 있어요'), findsOneWidget);
    expect(find.text('정상 · 잠금 풀림'), findsOneWidget);
    await tester.pump(const Duration(seconds: 4));
    expect(find.text('확인했어요 · 인덕션을 다시 쓸 수 있어요'), findsNothing);
  });

  testWidgets('재실 중 강불(M3, high)은 대화상자 없이 카드만 바뀐다', (tester) async {
    await _pump(tester, 'high');

    expect(find.text('강불 조리 중'), findsOneWidget);
    expect(find.text('강불로 조리 중이에요'), findsOneWidget);
    expect(find.text('후드를 최대로 올렸어요. 인덕션은 그대로예요.'), findsOneWidget);
    expect(find.byType(CenterAlert), findsNothing);
    expect(find.byType(SideAlert), findsNothing);
  });

  testWidgets('자리 비움 경고: 「인덕션 끄기」는 IND_SET 0 요청', (tester) async {
    final feed = await _pump(tester, 'absent');

    expect(find.text('경고 · 자리 비움'), findsOneWidget);
    await _tap(tester, find.text('인덕션 끄기'));
    expect(_sent(feed), [(UiCmd.indSet, 0)]);
  });

  testWidgets('센서 고장: 값 대신 「—」, 점검 배지, 오른쪽 대화상자', (tester) async {
    await _pump(tester, 'fault');

    expect(find.text('점검 필요 · 온도 센서'), findsOneWidget);
    expect(find.text('—'), findsOneWidget);
    expect(find.text('온도 센서가\n응답하지 않아요'), findsOneWidget);
    expect(find.text('안전을 위해 후드를 최대로 올리고 인덕션을 껐어요.\n연결을 확인해 주세요.'),
        findsOneWidget);

    // 고장 중 locked_danger여도 고장 창 하나만 둔다.
    await _tap(tester, find.widgetWithText(HsButton, '확인'));
    expect(find.byType(SideAlert), findsNothing);
    expect(find.byType(CenterAlert), findsNothing);
  });

  testWidgets('ESP32 무응답은 hazard와 별개로 경고 배지 (IND_NOACK·FAN_NOACK)',
      (tester) async {
    await _pumpEdited(tester, 'low', (p) {
      p['reasons'] = ['IND_NOACK'];
      p['induction'] = {'level': 2, 'allowed_on': false, 'ack': false};
    });

    expect(find.text('경고 · 인덕션 응답 없음'), findsOneWidget);
    expect(find.text('인덕션이 응답하지 않아요'), findsOneWidget);
    expect(find.text('인덕션 응답 없음'), findsOneWidget);
  });

  testWidgets('무응답 코드는 위험 표시를 덮지 않는다 (danger + FAN_NOACK)', (tester) async {
    // Core는 FAN_NOACK·IND_NOACK을 reasons 맨 뒤에 붙인다 (policy.cpp 11단계).
    await _pumpEdited(tester, 'fire', (p) {
      p['reasons'] = ['TEMP_DANGER', 'SMOKE', 'FAN_NOACK'];
      p['fan'] = {'level': 3, 'min': 3, 'requested': 0, 'ack': false};
    });

    expect(find.text('위험 · 화재 위험'), findsOneWidget);
    expect(find.text('경고 · 후드 응답 없음'), findsNothing);
    await _tap(tester, find.text('확인했어요'));
    expect(find.text('화재 위험을 미리 감지해 인덕션을 껐어요'), findsOneWidget);
    expect(find.text('후드 응답 없음'), findsOneWidget);
  });

  testWidgets('state 3 s 무수신 → 연결 끊김, 다시 받으면 복귀 (M13)', (tester) async {
    final feed = await _pump(tester);

    // 데모 피드는 0.5 s마다 다시 보내므로 마지막 수신은 멈춘 시각 0.5 s 안쪽이다.
    feed.silence();
    await tester.pump(const Duration(seconds: 2));
    expect(find.text('연결 끊김 · 다시 연결 중'), findsNothing);
    await tester.pump(const Duration(milliseconds: 1100));
    expect(find.text('연결 끊김 · 다시 연결 중'), findsOneWidget);
    expect(find.text('안전 장치와 연결이 끊겼어요'), findsOneWidget);
    expect(find.text('14.6'), findsNothing);
    // 인덕션·후드 모두 마지막으로 받은 단수에 「마지막 확인」을 붙인다.
    expect(find.text('마지막 확인'), findsNWidgets(2));

    await _play(tester, feed, _scenario('mid'));
    expect(find.text('중불 조리 중'), findsOneWidget);
  });

  testWidgets('첫 state를 8 s 못 받으면 시작 화면에서 연결 끊김으로', (tester) async {
    final feed = DemoKitchenFeed()..silence();
    await _start(tester, feed);

    expect(find.text('안전 장치와 연결하는 중'), findsOneWidget);
    await tester.pump(const Duration(seconds: 6));
    expect(find.text('안전 장치와 연결하는 중'), findsOneWidget);
    await tester.pump(const Duration(milliseconds: 500));
    expect(find.text('안전 장치와 연결하는 중'), findsNothing);
    expect(find.text('연결 끊김 · 다시 연결 중'), findsOneWidget);
  });

  testWidgets('음성 수락: 「요청을 받았어요」와 지금 단수를 따로, 2 s 뒤 닫는다', (tester) async {
    final feed = await _pump(tester);
    expect(find.text('“하이 LG, 후드 약풍”'), findsOneWidget);

    await _play(tester, feed, _scenario('voice'));
    // 결과 창과 음성 카드 둘 다 「요청을 받았어요」. 실제 단수는 state의 fan.level.
    expect(find.text('요청을 받았어요'), findsNWidgets(2));
    expect(find.text('후드 중풍'), findsOneWidget);
    expect(find.text('지금 후드 2단 · 실제 단수는 안전 하한을 따라요'), findsOneWidget);
    expect(find.text('“후드 중풍” · 지금 후드 2단'), findsOneWidget);
    await tester.pump(const Duration(seconds: 2));
    expect(find.byType(VoiceResultPanel), findsNothing);
  });

  testWidgets('음성 수락 뒤 Core 단수·ACK가 맞으면 결과 안내 한 번, 경고가 있으면 생략 (#173)',
      (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('low')),
        clips: clips);
    await _play(tester, feed, _scenario('voice'));
    expect(clips.played, ['APP_START', 'results/RESULT_FAN_2']);
    // 같은 결과가 다시 와도 새 요청으로 보지 않는다(계약에 요청 ID 없음).
    await _play(tester, feed, _scenario('voice'));
    expect(clips.played, ['APP_START', 'results/RESULT_FAN_2']);

    // 경고와 같은 state에서는 결과 안내를 내지 않는다.
    final warned = _scenario('voice').toJson();
    final p = warned['p']! as Map<String, Object?>;
    p['voice'] = {
      'last_intent': 'FAN_SET',
      'value': 3,
      'accepted': true,
      'reason': null
    };
    p['fan'] = {'level': 3, 'min': 1, 'requested': 3, 'ack': true};
    p['alert'] = 'WARNING';
    await _play(tester, feed, CoreState.fromJson(warned));
    expect(clips.played, ['APP_START', 'results/RESULT_FAN_2', 'WARNING']);
  });

  testWidgets('NO_INTENT 새 결과에만 미지원 음성 한 번, 경고·다른 거부는 생략', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('low')),
        clips: clips);
    final json = _scenario('low').toJson();
    final p = json['p']! as Map<String, Object?>;
    p['voice'] = {
      'last_intent': 'NONE',
      'value': null,
      'accepted': false,
      'reason': 'NO_INTENT'
    };
    await _play(tester, feed, CoreState.fromJson(json));
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'results/RESULT_UNSUPPORTED']);
    p['voice'] = {
      'last_intent': 'IND_ON',
      'value': null,
      'accepted': false,
      'reason': 'LOCKED'
    };
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'results/RESULT_UNSUPPORTED']);
    p['voice'] = {
      'last_intent': 'NONE',
      'value': null,
      'accepted': false,
      'reason': 'NO_INTENT'
    };
    p['alert'] = 'WARNING';
    await _play(tester, feed, CoreState.fromJson(json));
    expect(
        clips.played, ['APP_START', 'results/RESULT_UNSUPPORTED', 'WARNING']);
  });

  testWidgets('인덕션 절대 단수 수락 뒤 목표·ACK를 확인하고 기존 결과 음원 재생', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('low')),
        clips: clips);
    final json = _scenario('low').toJson();
    final p = json['p']! as Map<String, Object?>;
    p['voice'] = {
      'last_intent': 'IND_SET',
      'value': 2,
      'accepted': true,
      'reason': null
    };
    p['induction'] = {'level': 1, 'allowed_on': true, 'ack': true};
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START']);
    p['induction'] = {'level': 2, 'allowed_on': true, 'ack': true};
    await _play(tester, feed, CoreState.fromJson(json));
    expect(clips.played, ['APP_START', 'results/RESULT_IND_2']);
  });

  testWidgets('음성 거부: 사유는 state 그대로 옮긴다', (tester) async {
    final feed = await _pump(tester);
    final json = _scenario('low').toJson();
    (json['p']! as Map<String, Object?>)['voice'] = {
      'last_intent': 'IND_ON',
      'value': null,
      'accepted': false,
      'reason': 'LOCKED',
    };

    await _play(tester, feed, CoreState.fromJson(json));
    expect(find.text('지금은 할 수 없어요'), findsOneWidget);
    expect(find.text('지금은 잠겨 있어요'), findsOneWidget);
    expect(find.text('인덕션'), findsOneWidget);
    expect(find.text('“인덕션 켜기” — 잠금 중이라 거부됨'), findsNWidgets(2));
    // 지금 말할 수 있는 지원 명령 예시 (설계 v1 §6 C3).
    expect(find.text('하이 LG, 인덕션 꺼 줘'), findsOneWidget);
    await tester.pump(const Duration(seconds: 2));
    expect(find.byType(VoiceResultPanel), findsOneWidget);
    await tester.pump(const Duration(seconds: 1));
    expect(find.byType(VoiceResultPanel), findsNothing);
  });

  testWidgets('센서 화면: 그래프와 모르는 reasons 코드를 원문 그대로 보인다', (tester) async {
    await _pumpEdited(tester, 'mid', (p) {
      p['reasons'] = ['TEMP_MID', 'NEW_CODE'];
    });

    await _tap(tester, find.text('센서'));
    expect(find.text('60초'), findsOneWidget);
    expect(find.text('현재 사유 · 중불 · NEW_CODE'), findsOneWidget);
    await _tap(tester, find.byTooltip('홈으로'));
    expect(find.text('주방'), findsOneWidget);
  });

  testWidgets('기록 탭: 앱을 켠 뒤 조리 리듬·지켜 준 기록, 필터는 기록 종류만 거른다', (tester) async {
    final feed = await _pump(tester, 'idle');
    await _tap(tester, find.text('기록'));
    // 조리 리듬 빈 상태는 한 줄. 지켜 준 기록 네 칸은 늘 보이고 없으면 0번 (정인 10/26).
    // 메모리 기록이라는 사실은 상단 바에 한 번.
    expect(find.text('아직 기록이 없어요'), findsOneWidget);
    expect(find.textContaining('부터 · 앱을 켠 뒤 기록'), findsOneWidget);
    expect(find.text('센서·연결 이상 없음'), findsOneWidget);
    expect(find.text('반려동물 잠금'), findsOneWidget);
    expect(find.text('0번'), findsNWidgets(3));
    expect(find.text('수락 0번 · 거부 0번'), findsOneWidget);

    await _play(tester, feed, _scenario('low'));
    await tester.pump(const Duration(seconds: 2));
    expect(find.textContaining('부터 조리 중'), findsOneWidget);
    expect(find.textContaining(RegExp(r'^조리 \d+초$')), findsOneWidget);
    expect(find.text('약불 100 %'), findsOneWidget);
    // 범례는 기록에 없던 단계·단수도 늘 같은 순서로, 후드는 1·2·3단 + 자동 빗금 (정인 10/26).
    for (final name in ['약불', '중불', '강불', '과열', '위험', '1단', '2단', '3단']) {
      expect(find.text(name), findsWidgets, reason: name);
    }
    expect(find.text('자동 상향 (빗금)'), findsOneWidget);
    expect(find.text('화력'), findsOneWidget);
    expect(find.textContaining('후드 자동 상향 0번'), findsNothing);

    // 정상으로 돌아온 것을 「복구」라 하지 않는다 — 중불·강불도 정상 조리다 (D-054, #158 PM 리뷰).
    await _play(tester, feed, _scenario('mid'));
    await _play(tester, feed, _scenario('low'));
    expect(find.text('정상 · 약불'), findsOneWidget);
    expect(find.textContaining('복구'), findsNothing);

    await _play(tester, feed, _scenario('voice'));
    await tester.pump(const Duration(seconds: 3));
    expect(find.textContaining('음성 “'), findsOneWidget);
    expect(find.textContaining('모니터링 시작'), findsOneWidget);
    await _tap(tester, find.widgetWithText(TextButton, '음성'));
    expect(find.textContaining('모니터링 시작'), findsNothing);
    expect(find.textContaining('음성 “'), findsOneWidget);
    await _tap(tester, find.widgetWithText(TextButton, '안전·잠금'));
    expect(find.textContaining('음성 “'), findsNothing);
    expect(find.textContaining('모니터링 시작'), findsOneWidget);
    expect(find.text('수락 1번 · 거부 0번'), findsOneWidget);
    // 「안전 점수」 같은 표현은 쓰지 않는다 (PM 설계 10/26).
    expect(find.textContaining('점수'), findsNothing);
    expect(find.textContaining('무사고'), findsNothing);
  });

  testWidgets('기록 탭: 켤 때 Core가 늦어 끊김이었으면 첫 state도 복구로 센다 (#158 리뷰)',
      (tester) async {
    final feed = await _start(tester, DemoKitchenFeed()..silence());
    await tester.pump(const Duration(seconds: 9));
    await _play(tester, feed, _scenario('idle'));
    await tester.pump(const Duration(seconds: 2));
    await _tap(tester, find.text('기록'));
    expect(find.text('연결 복구 1번'), findsOneWidget);
    expect(find.textContaining('연결 복구 0번'), findsNothing);
  });

  testWidgets('리허설 칩으로 시나리오를 바꾼다', (tester) async {
    await _pump(tester);

    await _tap(tester, find.widgetWithText(ChoiceChip, '화재 위험 차단'));
    expect(find.text('위험 · 화재 위험'), findsOneWidget);
    expect(find.byType(CenterAlert), findsOneWidget);
  });

  testWidgets('「동작 줄이기」면 반복 애니메이션을 멈춘다', (tester) async {
    tester.platformDispatcher.accessibilityFeaturesTestValue =
        const FakeAccessibilityFeatures(disableAnimations: true);
    addTearDown(tester.platformDispatcher.clearAccessibilityFeaturesTestValue);
    await _pump(tester);

    expect(find.byType(SpinningFan), findsOneWidget);
    expect(tester.hasRunningAnimations, isFalse);
  });

  // 1×1 투명 PNG. 기기에서는 brand_fonts.dart가 FONT_DIR의 LG 공식 파일을 읽어 넣는다.
  final pixel = MemoryImage(base64Decode(
      'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg=='));

  testWidgets('LG 로고 파일이 없으면 시작 화면·상단 바 어디에도 로고가 없다', (tester) async {
    tester.view.physicalSize = const Size(1920, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    await tester.pumpWidget(
        HoodSeekApp(feed: DemoKitchenFeed(initial: _scenario('low'))));
    await tester.pump();
    expect(find.text('안전 장치와 연결하는 중'), findsOneWidget);
    expect(find.byType(LgLogo), findsNothing);
    expect(find.byType(Image), findsNothing);

    await tester.pump(const Duration(milliseconds: 1600));
    expect(find.text('주방'), findsOneWidget);
    expect(find.byType(LgLogo), findsNothing);
  });

  testWidgets('LG 로고는 시작 화면과 상단 바의 같은 자리에, 배경은 LG 그라디언트', (tester) async {
    final background = MemoryImage(base64Decode(
        'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg=='));
    tester.view.physicalSize = const Size(1920, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    await tester.pumpWidget(HoodSeekApp(
        feed: DemoKitchenFeed(initial: _scenario('low')),
        brand: BrandImages(logo: pixel, splash: background)));
    await tester.pump();
    expect(find.text('안전 장치와 연결하는 중'), findsOneWidget);
    expect(_logoLabel(tester), 'LG전자');
    expect(find.byWidgetPredicate((w) => w is Image && w.image == background),
        findsOneWidget);
    final onSplash = tester.getRect(find.byType(LgLogo));

    await tester.pump(const Duration(milliseconds: 1600));
    expect(find.text('주방'), findsOneWidget);
    expect(_logoLabel(tester), 'LG전자');
    expect(tester.getRect(find.byType(LgLogo)), onSplash);
    expect(find.byWidgetPredicate((w) => w is Image && w.image == background),
        findsNothing);
  });

  testWidgets('위험 창에는 LG 로고를 넣지 않는다 (상단 바 하나만)', (tester) async {
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('low')),
        brand: BrandImages(logo: pixel));
    expect(find.byType(LgLogo), findsOneWidget);
    await _play(tester, feed, _scenario('fire'));
    expect(find.byType(CenterAlert), findsOneWidget);
    expect(find.byType(LgLogo), findsOneWidget);
  });

  testWidgets('경고음은 alert가 NONE 아닌 값으로 바뀔 때만 한 번 (#66)', (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('low')),
        clips: clips);
    expect(clips.played, ['APP_START']);

    // 정상 조리(중불·강불)는 Core가 경고음을 안 낸다 (D-059).
    await _play(tester, feed, _scenario('mid'));
    await _play(tester, feed, _scenario('high'));
    expect(clips.played, ['APP_START']);

    // Core가 2 Hz로 같은 alert를 다시 보내도 한 번만 낸다.
    await _play(tester, feed, _scenario('overheat'));
    await tester.pump(const Duration(seconds: 2));
    expect(clips.played, ['APP_START', 'WARNING']);

    // 1 s 뒤 NONE으로 돌아갔다가 다시 생기면 새 경고다.
    final quiet = _scenario('overheat').toJson();
    (quiet['p']! as Map)['alert'] = 'NONE';
    await _play(tester, feed, CoreState.fromJson(quiet));
    await _play(tester, feed, _scenario('overheat'));
    expect(clips.played, ['APP_START', 'WARNING', 'WARNING']);

    // 바로 다른 경고로 바뀌어도 낸다.
    await _play(tester, feed, _scenario('fire'));
    expect(clips.played, ['APP_START', 'WARNING', 'WARNING', 'DANGER']);
  });

  testWidgets('state가 3 s 끊기면 OFFLINE 한 번, 다시 받아도 alert가 없으면 조용히',
      (tester) async {
    final clips = _Clips();
    final feed = await _start(
        tester, DemoKitchenFeed(initial: _scenario('low')),
        clips: clips);
    feed.silence();
    await tester.pump(const Duration(seconds: 3));
    expect(find.text('연결 끊김 · 다시 연결 중'), findsOneWidget);
    expect(clips.played, ['APP_START', 'OFFLINE']);
    await tester.pump(const Duration(seconds: 10));
    expect(clips.played, ['APP_START', 'OFFLINE']);

    await _play(tester, feed, _scenario('low'));
    expect(clips.played, ['APP_START', 'OFFLINE']);
  });

  testWidgets('켜자마자 받은 첫 state가 경고면 그 경고음을 낸다', (tester) async {
    final clips = _Clips();
    await _start(tester, DemoKitchenFeed(initial: _scenario('fire')),
        clips: clips);
    await tester.pump(const Duration(seconds: 2));
    expect(clips.played, ['APP_START', 'DANGER']);
  });

  testWidgets('첫 state 바로 뒤에 끊겨도 3 s에 연결 끊김과 OFFLINE (8 s 아님)', (tester) async {
    final clips = _Clips();
    final feed = DemoKitchenFeed(initial: _scenario('low'));
    tester.view.physicalSize = const Size(1920, 1200);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    await tester.pumpWidget(HoodSeekApp(feed: feed, clips: clips));
    await tester.pump();
    feed.silence();

    await tester.pump(const Duration(milliseconds: 2900));
    expect(clips.played, ['APP_START']);
    await tester.pump(const Duration(milliseconds: 200));
    expect(find.text('연결 끊김 · 다시 연결 중'), findsOneWidget);
    expect(clips.played, ['APP_START', 'OFFLINE']);
  });

  testWidgets('켤 때부터 state가 없으면 8 s에 OFFLINE 한 번', (tester) async {
    final clips = _Clips();
    await _start(tester, DemoKitchenFeed()..silence(), clips: clips);
    await tester.pump(const Duration(seconds: 6));
    expect(clips.played, ['APP_START']);
    await tester.pump(const Duration(milliseconds: 500));
    expect(clips.played, ['APP_START', 'OFFLINE']);
    await tester.pump(const Duration(seconds: 20));
    expect(clips.played, ['APP_START', 'OFFLINE']);
  });
}
