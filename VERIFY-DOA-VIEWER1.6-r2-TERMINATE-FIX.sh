#!/usr/bin/env bash
set -euo pipefail
ROOT="${1:-$(pwd)}"
pass=0; fail=0
check(){
  local name="$1"; shift
  if "$@"; then echo "PASS - $name"; pass=$((pass+1)); else echo "FAIL - $name"; fail=$((fail+1)); fi
}
contains(){ grep -q -- "$1" "$2"; }
not_contains(){ ! grep -q -- "$1" "$2"; }
check "ViewerPage root no anchors.fill in first 16 lines" bash -c "! sed -n '1,16p' \"$ROOT/DoaViewer/ViewerPage.qml\" | grep -q 'anchors.fill: parent'"
check "ViewerPage StackView note" contains "StackView sizes pages itself" "$ROOT/DoaViewer/ViewerPage.qml"
check "ViewerPage width binding" contains "width: parent ? parent.width : 1920" "$ROOT/DoaViewer/ViewerPage.qml"
check "ViewerPage height binding" contains "height: parent ? parent.height : 1080" "$ROOT/DoaViewer/ViewerPage.qml"
check "Waterfall destructor declaration" contains "~FftWaterfallTextureItem() override" "$ROOT/FftWaterfallTextureItem.h"
check "Waterfall releaseResources declaration" contains "void releaseResources() override" "$ROOT/FftWaterfallTextureItem.h"
check "Waterfall destructor implementation" contains "FftWaterfallTextureItem::~FftWaterfallTextureItem" "$ROOT/FftWaterfallTextureItem.cpp"
check "Waterfall releaseResources implementation" contains "FftWaterfallTextureItem::releaseResources" "$ROOT/FftWaterfallTextureItem.cpp"
check "Waterfall conservative node replacement" contains "delete oldNode;" "$ROOT/FftWaterfallTextureItem.cpp"
check "Waterfall no manual old texture delete" not_contains "delete node->texture" "$ROOT/FftWaterfallTextureItem.cpp"
check "Waterfall no static_cast reuse" not_contains "static_cast<QSGSimpleTextureNode" "$ROOT/FftWaterfallTextureItem.cpp"
check "LineGraph destructor declaration" contains "~FftLineGraphItem() override" "$ROOT/FftLineGraphItem.h"
check "LineGraph releaseResources declaration" contains "void releaseResources() override" "$ROOT/FftLineGraphItem.h"
check "LineGraph destructor implementation" contains "FftLineGraphItem::~FftLineGraphItem" "$ROOT/FftLineGraphItem.cpp"
check "LineGraph releaseResources implementation" contains "FftLineGraphItem::releaseResources" "$ROOT/FftLineGraphItem.cpp"
check "Engineering note present" test -f "$ROOT/DOA-VIEWER1.6-r2-TERMINATE-FIX.md"
echo "VERIFY: $pass PASS / $fail FAIL"
test "$fail" -eq 0
