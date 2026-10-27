import 'dart:typed_data';

/// TCP 바이트 스트림을 `\n` 단위 한 줄로 자른다 (계약 §2.1: 한 줄 = 한 메시지, `\r` 무시).
///
/// `LineSplitter` 대신 바이트에서 자르는 이유: 상한 4096 byte(개행 포함)를 문자 수가 아니라
/// 바이트로 재고, 개행 없이 길어지는 입력을 버퍼에 무한정 쌓지 않기 위해서다.
/// 상한을 넘은 줄은 다음 `\n`까지 통째로 버리고 [onOversize]를 부른다.
class LineFramer {
  LineFramer({
    required this.onLine,
    this.onOversize,
    this.maxLineBytes = 4096,
  });

  final void Function(Uint8List line) onLine;
  final void Function()? onOversize;
  final int maxLineBytes;

  final BytesBuilder _buffer = BytesBuilder(copy: true);
  bool _discarding = false;

  void add(List<int> chunk) {
    var start = 0;
    for (var i = 0; i < chunk.length; i++) {
      if (chunk[i] != 0x0A) continue;
      _append(chunk, start, i);
      _endLine();
      start = i + 1;
    }
    _append(chunk, start, chunk.length);
  }

  void _append(List<int> chunk, int start, int end) {
    if (_discarding || start == end) return;
    _buffer.add(chunk.sublist(start, end));
    // 개행 1 byte 자리를 남긴다.
    if (_buffer.length > maxLineBytes - 1) {
      _buffer.clear();
      _discarding = true;
      onOversize?.call();
    }
  }

  void _endLine() {
    if (_discarding) {
      _discarding = false;
      return;
    }
    final bytes = _buffer.takeBytes();
    final line = Uint8List.fromList([
      for (final b in bytes)
        if (b != 0x0D) b,
    ]);
    if (line.isNotEmpty) onLine(line);
  }
}
