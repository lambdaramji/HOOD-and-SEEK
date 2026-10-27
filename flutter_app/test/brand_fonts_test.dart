import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:hood_seek/brand_fonts.dart';

void main() {
  test('LG 이미지는 기기 폴더에 있는 것만 읽고, 없으면 null', () async {
    final dir = Directory.systemTemp.createTempSync('hs_brand');
    addTearDown(() => dir.deleteSync(recursive: true));

    final none = await loadBrandImages(dir.path);
    expect(none.logo, isNull);
    expect(none.splash, isNull);

    File('${dir.path}/$brandLogoFile').writeAsBytesSync([0]);
    final logoOnly = await loadBrandImages(dir.path);
    expect(logoOnly.logo, isNotNull);
    expect(logoOnly.splash, isNull);
  });

  test('FONT_DIR가 없으면 아무것도 읽지 않는다', () async {
    final none = await loadBrandImages('');
    expect(none.logo, isNull);
    expect(none.splash, isNull);
  });
}
