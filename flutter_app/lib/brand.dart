import 'package:flutter/widgets.dart';

/// 기기 폴더에서 읽은 LG 브랜드 이미지(`brand_fonts.dart`의 [loadBrandImages]).
/// 글꼴처럼 저장소에 넣지 않는다(UI 설계 v1 §1 「로고·워드마크」, §2.3).
/// 파일이 없으면 null이고, 상단 바는 로고 없이, 시작 화면은 자체 마크와 대체 그라디언트로 그린다.
class BrandImages {
  const BrandImages({this.logo, this.splash});

  /// 「LG전자」 로고(헤리티지 레드 심벌 + 회색 글자). 상단 바와 시작 화면에만 쓴다.
  final ImageProvider? logo;

  /// 시작 화면 배경 LG Electronics Gradient 02.
  final ImageProvider? splash;

  static const none = BrandImages();
}
