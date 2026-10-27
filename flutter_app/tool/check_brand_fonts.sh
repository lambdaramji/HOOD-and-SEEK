#!/bin/sh
# Pi 5에서 앱을 띄우기 전에 LG EI 글꼴과 LG 로고·시작 화면 배경 파일이 다 있는지 본다
# (UI 설계 v1 §1 「로고·워드마크」, §2.3). 앱은 못 읽어도 Noto Sans KR·로고 없이 그리므로,
# 그 상태로 심사를 치르지 않게 기동 전에 막는다.
#
#   PI5=<Pi 5 주소> FONT_DIR=<빌드 때 --dart-define에 넣은 폴더> flutter_app/tool/check_brand_fonts.sh \
#     && <앱 기동 명령>
#
# 파일 목록은 lib/brand_fonts.dart의 brandFontFiles·brandLogoFile·brandSplashFile에서 읽는다.
# 하나라도 없으면 이름을 찍고 1로 끝난다.
set -eu
: "${PI5:?PI5를 지정하세요}" "${FONT_DIR:?FONT_DIR를 지정하세요}"
files=$(grep -oE "'(LGEI[A-Za-z]+-[A-Za-z]+\.ttf|lge_logo_[a-z_]+\.png|LG_Electronics_Gradient_[0-9]+_RGB\.jpg)'" \
  "$(dirname "$0")/../lib/brand_fonts.dart" | tr -d "'" | tr '\n' ' ')
ssh "root@$PI5" "cd '$FONT_DIR' && for f in $files; do test -r \"\$f\" || { echo \"missing \$f\"; exit 1; }; done && echo 'LG files ok'"
