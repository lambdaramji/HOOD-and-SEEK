import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import 'brand.dart';
import 'widgets/hs.dart';

/// 기기 폴더에 둘 LG EI 원본 TTF 파일 이름. LG전자 브랜드 자산 페이지
/// (https://www.lge.co.kr/company/info/brandAsset)의 폰트 묶음 `LG-EI_font_all.zip`
/// `TTF/` 폴더 이름 그대로다. 기동 전 확인(`tool/check_brand_fonts.sh`)도 이 목록을 쓴다.
const brandFontFiles = {
  Hs.text: [
    'LGEITextTTF-Light.ttf',
    'LGEITextTTF-Regular.ttf',
    'LGEITextTTF-SemiBold.ttf',
    'LGEITextTTF-Bold.ttf',
  ],
  Hs.number: [
    'LGEIHeadlineTTF-Thin.ttf',
    'LGEIHeadlineTTF-Light.ttf',
    'LGEIHeadlineTTF-Regular.ttf',
    'LGEIHeadlineTTF-Semibold.ttf',
  ],
};

/// 같은 폴더에 둘 LG 이미지. 같은 브랜드 자산 페이지의 로고 묶음
/// `lge_logo_kr_heritagered_grey_all/` 폴더와 그라디언트 묶음 `LG_Electronics_Gradient_all.zip`에
/// 든 파일 이름 그대로다. 시작 화면 배경은 02(핑크, 목업 `hs-bg-2`). 기동 전 확인도 이 두 이름을 쓴다.
const brandLogoFile = 'lge_logo_kr_heritagered_grey_rgb.png';
const brandSplashFile = 'LG_Electronics_Gradient_02_RGB.jpg';

/// LG EI 글꼴을 기기 폴더에서 읽어 등록한다. 글꼴 파일은 LG 브랜드 서체라 저장소에
/// 넣지 않는다(R-16·P-010). `--dart-define=FONT_DIR=<폴더>`에 원본 TTF가 있으면 쓰고,
/// 없거나 읽지 못하면 앱에 넣은 Noto Sans KR로 그린다. 화면 경고 없이 로그 한 줄만 남긴다.
Future<void> loadBrandFonts(String dir) async {
  if (dir.isEmpty) return;
  final missing = <String>[];
  for (final MapEntry(key: family, value: files) in brandFontFiles.entries) {
    final loader = FontLoader(family);
    var found = false;
    for (final name in files) {
      final file = File('$dir/$name');
      if (!file.existsSync()) {
        missing.add(name);
        continue;
      }
      found = true;
      loader.addFont(
          file.readAsBytes().then((bytes) => ByteData.sublistView(bytes)));
    }
    if (found) await loader.load();
  }
  if (missing.isNotEmpty) {
    debugPrint('LG EI 글꼴 일부가 없어 Noto Sans KR로 그립니다: ${missing.join(', ')}');
  }
}

/// LG 로고와 시작 화면 배경을 기기 폴더에서 읽는다(UI 설계 v1 §1 「로고·워드마크」).
/// 없는 파일은 null로 두고 로그 한 줄만 남긴다. 원본이 커서(로고 3211×1127, 배경
/// 2000×2000) 화면에 쓰는 크기로 줄여 푼다.
Future<BrandImages> loadBrandImages(String dir) async {
  if (dir.isEmpty) return BrandImages.none;
  Future<Uint8List?> read(String name) async {
    final file = File('$dir/$name');
    if (file.existsSync()) return file.readAsBytes();
    debugPrint('LG 이미지 파일이 없어 쓰지 않습니다: $name');
    return null;
  }

  final logo = await read(brandLogoFile);
  final splash = await read(brandSplashFile);
  return BrandImages(
    logo: logo == null ? null : ResizeImage(MemoryImage(logo), height: 128),
    splash:
        splash == null ? null : ResizeImage(MemoryImage(splash), width: 1920),
  );
}
