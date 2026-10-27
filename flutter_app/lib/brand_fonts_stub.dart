import 'brand.dart';

/// 웹(데모)에는 기기 폴더가 없어 대체 글꼴로 그린다.
Future<void> loadBrandFonts(String dir) async {}

/// 웹(데모)에는 기기 폴더가 없어 LG 로고 없이 그린다.
Future<BrandImages> loadBrandImages(String dir) async => BrandImages.none;
