#!/usr/bin/env bash
# SwiftPlugin 빌드 — swiftc 한 번이면 끝난다.
#
# CMake 를 안 쓰는 이유: CMake 의 Swift 지원은 **Ninja/Xcode 제너레이터 전용**이라,
# 최상위에 넣으면 README 의 `cmake -B build` (기본 Makefile 제너레이터)가 깨진다.
# WallDialogPlugin/WpfPlugin 이 각자 vcxproj 를 쓰는 것과 같은 이유 — 예제마다 그
# 언어에 자연스러운 도구를 쓴다.
#
#   ./build.sh                 릴리스
#   ./build.sh debug           디버그
#   VULKANCAD_ENGINE=<경로> ./build.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENGINE="${VULKANCAD_ENGINE:-$HERE/../../3dEngine}"

if [[ ! -f "$ENGINE/plugin/lot_plugin_sdk.h" ]]; then
    echo "엔진 헤더를 찾을 수 없습니다: $ENGINE/plugin/lot_plugin_sdk.h" >&2
    echo "  VULKANCAD_ENGINE=<엔진 경로> ./build.sh" >&2
    exit 1
fi

case "$(uname -s)" in
    Darwin)
        OUT="$HERE/SwiftPlugin.dylib"
        # 미정의 CAD_* 심볼은 로드 시점에 호스트(엔진 실행 파일)에서 찾는다.
        # 최상위 CMakeLists 가 C++ 플러그인에 거는 것과 같은 옵션이다.
        LINK=(-Xlinker -undefined -Xlinker dynamic_lookup)
        ;;
    Linux)
        OUT="$HERE/SwiftPlugin.so"
        # ELF 공유 라이브러리는 미정의 심볼이 기본 허용이라 따로 줄 게 없다.
        # ⚠️ 대신 Swift 런타임이 OS 에 없다 — libswiftCore.so 를 같이 배포하거나
        #    -static-stdlib 로 박아야 한다(macOS 는 /usr/lib/swift 에 있어 필요 없다).
        LINK=()
        ;;
    *)
        echo "지원하지 않는 플랫폼: $(uname -s)" >&2
        exit 1
        ;;
esac

if [[ "${1:-release}" == "debug" ]]; then
    OPT=(-Onone -g)
else
    OPT=(-O)
fi

# -swift-version 5 를 못 박는다. Swift 6 언어 모드는 전역 가변 변수를 동시성 위반으로
# 막는데(pluginId, counterRef …), 플러그인은 계약상 **엔진 스레드 한 곳**에서만 불린다.
# 6 모드로 옮기려면 그 전역들을 격리하거나 nonisolated(unsafe) 를 붙여야 한다.
set -x
swiftc "${OPT[@]}" \
    -swift-version 5 \
    -emit-library \
    -module-name SwiftPlugin \
    -o "$OUT" \
    -I "$HERE/include" \
    -Xcc -I"$ENGINE" \
    "${LINK[@]}" \
    "$HERE/SwiftPlugin.swift" \
    "$HERE/WallWindow.swift"
set +x

echo
echo "빌드됨: $OUT"
echo "설치:   cp \"$OUT\" <엔진 실행 파일 옆>/plugins/"
