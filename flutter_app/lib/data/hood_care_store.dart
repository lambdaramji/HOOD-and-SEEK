import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

import 'hood_care.dart';

/// 후드 케어 누적을 남기는 Pi 5 파일. `--dart-define=HOOD_CARE_FILE=<경로>`로 바꾼다.
/// 앱 계정은 설치할 때마다 바뀌고(u0_a5006 → u0_a5007) `/data/hoodseek`는 root 전용이라
/// [실측 10/26 Pi 5 `ls -ld`·`ps`], root가 한 번 `mkdir -p /data/hoodseek/app && chmod 0777`로
/// 누구나 쓰는 폴더를 만들어 둔다. sticky bit(1777)는 두지 않는다 — 다음 설치의 새 계정이 옛 계정
/// 파일 위로 rename하지 못한다(#158 PM 리뷰). 없으면 「저장하지 못해요」를 띄우고 앱 메모리로만 센다.
const hoodCareFile = String.fromEnvironment('HOOD_CARE_FILE',
    defaultValue: '/data/hoodseek/app/hood-care.json');

/// 읽기 실패(깨짐·권한)로 0부터 셀 때 화면에 한 줄로 보이는 알림.
const hoodCareLoadFailed = '기록을 읽지 못해 0부터 다시 셉니다';

/// JSON 파일 하나에 [HoodCare]를 읽고 쓴다. 어떤 실패도 앱을 멈추지 않는다.
/// 주기 저장과 타이머는 부르는 쪽(대시보드)이 맡는다.
class HoodCareStore {
  HoodCareStore([this.path = hoodCareFile]);

  final String path;

  /// 앞 저장이 끝난 뒤 다음 저장을 쓴다. 주기 저장과 「청소했어요」 저장이 겹쳐도
  /// 임시 파일 하나를 두 번 동시에 쓰지 않게 한다.
  Future<bool> _last = Future.value(true);

  /// 두 번째 값은 화면 알림 문구이고 정상이면 null이다. 파일이 없으면(첫 실행) 알림 없이
  /// 0부터, 깨졌거나 권한이 없으면 0부터 + [hoodCareLoadFailed]. 깨진 파일은 다음 저장이
  /// 덮어쓰기 전에 `<path>.broken`으로 한 번 복사해 둔다.
  Future<(HoodCare, String?)> load() async {
    final file = File(path);
    final String text;
    try {
      text = await file.readAsString();
    } on PathNotFoundException {
      return (HoodCare(), null);
    } on FileSystemException catch (e) {
      debugPrint('hood care: 읽기 실패 $e');
      return (HoodCare(), hoodCareLoadFailed);
    }
    try {
      final json = jsonDecode(text);
      if (json is! Map<String, Object?>) {
        throw FormatException('hood care: JSON 객체 아님', text);
      }
      return (HoodCare.fromJson(json), null);
    } on FormatException catch (e) {
      debugPrint('hood care: 깨진 파일 $e');
      try {
        await file.copy('$path.broken');
      } on FileSystemException catch (e) {
        debugPrint('hood care: 깨진 파일 복사 실패 $e');
      }
      return (HoodCare(), hoodCareLoadFailed);
    }
  }

  /// 임시 파일(`<path>.tmp`)에 다 쓰고 디스크에 내린 뒤 rename한다. 쓰는 도중 전원이 꺼져도
  /// 이전 파일이 남는다. 실패하면 false — 부르는 쪽이 [hoodCareSaveFailed]를 보인다.
  Future<bool> save(HoodCare care) {
    final text = jsonEncode(care.toJson()); // 부른 순간의 값으로 쓴다
    return _last = _last.then((_) => _write(text));
  }

  Future<bool> _write(String text) async {
    final tmp = File('$path.tmp');
    try {
      await tmp.parent.create(recursive: true);
      // 앱 계정은 설치마다 바뀌어 예전 앱이 남긴 .tmp를 덮어쓰지 못한다. 0777 폴더라 지우는 것은 된다
      // [실측 10/26 정인 — 5005 → 5008, 덮어쓰기는 권한 없음, 지우고 쓰기는 됨].
      if (await tmp.exists()) await tmp.delete();
      await tmp.writeAsString(text, flush: true);
      await tmp.rename(path);
      return true;
    } on FileSystemException catch (e) {
      debugPrint('hood care: 저장 실패 $e');
      return false;
    }
  }
}
