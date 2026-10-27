import 'package:flutter/widgets.dart';

import 'app.dart';
import 'audio/clip_player_stub.dart'
    if (dart.library.io) 'audio/gst_clip_player.dart';
import 'brand.dart';
import 'brand_fonts_stub.dart' if (dart.library.io) 'brand_fonts.dart';
import 'data/camera_video_stub.dart'
    if (dart.library.io) 'data/camera_video.dart';
import 'data/kitchen_feed.dart';
import 'data/tcp_feed_stub.dart' if (dart.library.io) 'data/tcp_feed.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  // flutter-atlas build ... --dart-define=CORE_HOST=<Pi4 IP> (설계서 §5.4)
  const coreHost = String.fromEnvironment('CORE_HOST');
  const corePort = int.fromEnvironment('CORE_PORT', defaultValue: 7420);
  // LG EI 글꼴 TTF와 LG 로고·배경 이미지가 있는 기기 폴더. 저장소에는 넣지 않는다.
  const fontDir = String.fromEnvironment('FONT_DIR');
  // 경고음 출력 요소. Pi 5 기본 출력은 HDMI라 ReSpeaker 스피커로 내려면
  // --dart-define=AUDIO_SINK='pulsesink device=<pactl list short sinks의 이름>'.
  const audioSink =
      String.fromEnvironment('AUDIO_SINK', defaultValue: 'pulsesink');
  // 카메라 카드 영상: vision-node가 Pi 5 루프백으로 내보내는 MJPEG(#144, UI 설계 v1 §3.2).
  // 빈 값이면 영상 없이 그림과 판정 칩만. 데모(CORE_HOST 없음)에서는 쓰지 않는다.
  const cameraUrl = String.fromEnvironment('CAMERA_URL',
      defaultValue: 'http://127.0.0.1:7431/');
  final feed = (coreHost.isEmpty ? null : coreFeed(coreHost, corePort)) ??
      DemoKitchenFeed();

  // 첫 화면 전에 등록한다. 실패해도 멈추지 않고 Noto Sans KR로 그린다 (설계 v1 §2.3).
  try {
    await loadBrandFonts(fontDir);
  } on Object catch (error) {
    debugPrint('LG EI 글꼴을 읽지 못해 Noto Sans KR로 그립니다: $error');
  }
  // LG 로고·시작 화면 배경도 같은 폴더에서 읽는다. 없으면 로고 없이 그린다 (설계 v1 §1).
  var brand = BrandImages.none;
  try {
    brand = await loadBrandImages(fontDir);
  } on Object catch (error) {
    debugPrint('LG 이미지를 읽지 못해 로고 없이 그립니다: $error');
  }
  runApp(HoodSeekApp(
    feed: feed,
    brand: brand,
    clips: deviceClipPlayer(audioSink),
    video: coreHost.isEmpty ? null : cameraVideo(cameraUrl),
  ));
}
