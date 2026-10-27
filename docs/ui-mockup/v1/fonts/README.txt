LG EI 글꼴 — 저장소에는 넣지 않습니다 (LG 브랜드 서체, 재배포 조건 확인 전 · R-16 / P-010).

이 폴더에 아래 8개 woff2를 넣으면 모든 화면에 자동 적용됩니다. 파일은 PM(남현지)이 팀 채팅으로 전달한
`hoodseek-ui-html.zip`의 fonts/ 폴더에 있습니다 (원본: LG-EI-text-ttf.zip · LG-EI-Headline-ttf.zip).

  LGEIText-Light.woff2 / LGEIText-Regular.woff2 / LGEIText-SemiBold.woff2 / LGEIText-Bold.woff2
      — 본문·UI 전체 (한글 11,172자 포함). font-weight 300/400/500·600/700
  LGEIHeadline-Thin.woff2 / -Light.woff2 / -Regular.woff2 / -SemiBold.woff2
      — 핵심 수치 전용 (영문·숫자만, 한글 없음). class="hs-num" 요소가 먼저 쓰고 한글·단위는 Text로 떨어짐

없으면 Noto Sans KR(Google Fonts) → system-ui 순으로 대체되어 레이아웃은 유지됩니다.

Flutter (docs/2026-10-24-flutter-ui-design-v1.md §2.3, PM 결정 10/24):
  - pubspec.yaml에 등록하지 않습니다. 파일이 없는 CI·다른 PC에서 flutter test·flutter build web이
    "unable to locate asset entry in pubspec.yaml"로 멈춥니다 [실측 10/24 임재범, PR #113 리뷰].
  - 기기: 원본 TTF를 기기 폴더에 두고 --dart-define=FONT_DIR=<폴더>로 실행 → 앱이 FontLoader로 등록.
    심사 화면은 LG EI로만 실행: 기동 명령 앞에서 파일을 확인하고 없으면 기동하지 않습니다.
  - 실행 중 등록이 실패하면 앱에 포함한 Noto Sans KR(SIL OFL 1.1, 저장소 추적)로 그대로 그립니다. 경고 화면 없음.
  - CI·개발 PC: FONT_DIR 없음 → Noto Sans KR.
  재배포 허용이 확인되면 그때 저장소 추적 + pubspec 등록으로 바꿉니다.
