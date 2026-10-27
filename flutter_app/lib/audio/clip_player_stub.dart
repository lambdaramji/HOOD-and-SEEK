import 'clip_player.dart';

/// 웹(데모)에는 GStreamer가 없어 소리를 내지 않는다.
ClipPlayer deviceClipPlayer(String sink) => const SilentClipPlayer();
