/// 경고음 클립 재생 (설계서 §5.4·D-14, `contracts/README.md` 「`state.p.alert` — 클립 ID」).
///
/// 언제 낼지는 화면이 정한다(경고·끊김, 앱 기동, 사용자 요청 뒤 상태 변경).
/// 기동음·조작 확인음은 경고를 끊지 않고 생략한다.
/// 이 인터페이스는 소리만 낸다.
abstract interface class ClipPlayer {
  /// [clip]은 `assets/audio/<clip>.wav`의 이름이다 (`CAUTION`, `OFFLINE` 등).
  void play(String clip);
}

/// 소리를 내지 않는다. 웹 데모와 위젯 시험의 기본값.
class SilentClipPlayer implements ClipPlayer {
  const SilentClipPlayer();

  @override
  void play(String clip) {}
}
