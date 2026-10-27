import 'dart:convert';
import 'dart:typed_data';

/// vision-node 카메라 카드 영상(`multipart/x-mixed-replace`)을 JPEG 한 장씩 자른다
/// (`vision_node/README.md` 「카메라 카드 영상」, UI 설계 v1 §3.2).
///
/// 파트마다 `--boundary` 줄, `Content-Length: N`이 있는 머리, 빈 줄, JPEG N바이트가 온다.
/// 길이만큼 읽으므로 JPEG 안을 뒤지지 않는다. 길이가 없거나 너무 크면 그 파트를 버리고
/// 다음 머리에서 다시 맞춘다. 영상은 표시 전용이라 틀린 파트는 조용히 건너뛴다.
class MjpegParser {
  MjpegParser({this.maxFrameBytes = 2 * 1024 * 1024});

  /// 이보다 큰 파트는 깨진 것으로 본다. 640×480 품질 70 JPEG는 수십 KB다.
  final int maxFrameBytes;

  final _buffer = BytesBuilder(copy: false);
  Uint8List _pending = Uint8List(0);
  int? _need;

  /// [chunk]를 더하고, 그 덕에 끝난 JPEG들을 순서대로 돌려준다.
  List<Uint8List> add(List<int> chunk) {
    _buffer
      ..add(_pending)
      ..add(chunk);
    var data = _buffer.takeBytes();
    final frames = <Uint8List>[];
    var start = 0;
    while (true) {
      final need = _need;
      if (need != null) {
        if (data.length - start < need) break;
        frames.add(Uint8List.sublistView(data, start, start + need));
        start += need;
        _need = null;
        continue;
      }
      final end = _indexOf(data, _headerEnd, start);
      if (end < 0) {
        // 머리가 아직 덜 왔다. 끝없이 쌓이지 않게 머리 길이는 4 KB로 막는다.
        if (data.length - start > 4096) start = data.length;
        break;
      }
      final header = latin1.decode(Uint8List.sublistView(data, start, end));
      start = end + _headerEnd.length;
      final length = _contentLength(header);
      if (length != null && length > 0 && length <= maxFrameBytes) {
        _need = length;
      }
    }
    _pending = Uint8List.fromList(Uint8List.sublistView(data, start));
    return frames;
  }

  static final _headerEnd = ascii.encode('\r\n\r\n');

  static int? _contentLength(String header) {
    for (final line in header.split('\r\n')) {
      final colon = line.indexOf(':');
      if (colon < 0) continue;
      if (line.substring(0, colon).trim().toLowerCase() == 'content-length') {
        return int.tryParse(line.substring(colon + 1).trim());
      }
    }
    return null;
  }

  static int _indexOf(Uint8List data, List<int> pattern, int from) {
    for (var i = from; i <= data.length - pattern.length; i++) {
      var j = 0;
      while (j < pattern.length && data[i + j] == pattern[j]) {
        j++;
      }
      if (j == pattern.length) return i;
    }
    return -1;
  }
}
