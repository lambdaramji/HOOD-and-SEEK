import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/data/line_framer.dart';

void main() {
  late List<String> lines;
  late int oversize;
  late LineFramer framer;

  setUp(() {
    lines = [];
    oversize = 0;
    framer = LineFramer(
      onLine: (bytes) => lines.add(utf8.decode(bytes)),
      onOversize: () => oversize++,
      maxLineBytes: 16,
    );
  });

  test('청크 경계와 무관하게 줄 단위로 자른다', () {
    framer.add(utf8.encode('{"a":1}\n{"b"'));
    framer.add(utf8.encode(':2}\n{"c":3}\n'));
    expect(lines, ['{"a":1}', '{"b":2}', '{"c":3}']);
  });

  test('\\r은 무시하고 빈 줄은 내보내지 않는다', () {
    framer.add(utf8.encode('ab\r\n\r\n\ncd\n'));
    expect(lines, ['ab', 'cd']);
  });

  test('UTF-8 한 글자가 청크 사이에서 갈라져도 복원된다', () {
    final bytes = utf8.encode('강풍\n');
    framer.add(bytes.sublist(0, 2));
    framer.add(bytes.sublist(2));
    expect(lines, ['강풍']);
  });

  test('상한(개행 포함 16 byte)을 넘는 줄은 다음 개행까지 버리고 회복한다', () {
    framer.add(utf8.encode('${'x' * 15}\n')); // 15 + \n = 16 → 통과
    framer.add(utf8.encode('y' * 10));
    framer.add(utf8.encode('${'y' * 10}\nok\n')); // 20 byte → 버림
    expect(lines, ['x' * 15, 'ok']);
    expect(oversize, 1);
  });
}
