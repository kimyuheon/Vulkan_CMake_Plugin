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

# -swift-version 5 를 못 박는다. Swift 6 언어 모드는 전역 가변 변수를 동시성 위반으로
# 막는데(pluginId, counterRef …), 플러그인은 계약상 **엔진 스레드 한 곳**에서만 불린다.
# 6 모드로 옮기려면 그 전역들을 격리하거나 nonisolated(unsafe) 를 붙여야 한다.
COMMON=(-swift-version 5
        -module-name SwiftPlugin
        -I "$HERE/include"
        -Xcc -I"$ENGINE")
SOURCES=("$HERE/SwiftPlugin.swift" "$HERE/WallWindow.swift")

if [[ "${1:-release}" != "debug" ]]; then
    set -x
    swiftc -O "${COMMON[@]}" -emit-library -o "$OUT" "${LINK[@]}" "${SOURCES[@]}"
    set +x
elif [[ "$(uname -s)" == "Darwin" ]]; then
    # ⚠️ 디버그는 **두 단계**로 짓는다. 한 번에 -g -emit-library 로 지으면 lldb 가 줄을
    #    못 짚는다(실측: breakpoint 가 <compiler-generated> 로만 잡힌다).
    #    Mach-O 는 DWARF 를 실행물에 넣지 않고 "디버그맵"(OSO 항목)으로 .o 를 가리키는데,
    #    swiftc 는 그 .o 를 임시 폴더에 만들고 링크가 끝나면 **지워 버린다.** 가리키는
    #    곳이 없어지니 줄 정보도 같이 사라진다. (ELF/Linux 는 DWARF 가 .so 안에 들어가서
    #    이 문제가 없다 — 그래서 아래 else 는 한 단계다)
    #    그래서 .o 를 남기고, dsymutil 로 DWARF 를 .dSYM 에 모아 자립시킨다.
    #    -wmo 는 .o 를 하나로 만들려는 것뿐이다(-c 는 파일마다 -o 를 못 준다).
    mkdir -p "$HERE/.build-debug"
    set -x
    swiftc -Onone -g "${COMMON[@]}" -wmo -c -o "$HERE/.build-debug/SwiftPlugin.o" "${SOURCES[@]}"
    swiftc -emit-library -o "$OUT" "${LINK[@]}" "$HERE/.build-debug/SwiftPlugin.o"
    dsymutil "$OUT"
    set +x
else
    set -x
    swiftc -Onone -g "${COMMON[@]}" -emit-library -o "$OUT" "${LINK[@]}" "${SOURCES[@]}"
    set +x
fi

echo
echo "빌드됨: $OUT"
echo "설치:   cp \"$OUT\" <엔진 실행 파일 옆>/plugins/"
if [[ "${1:-release}" == "debug" ]]; then
    echo
    echo "디버깅: 플러그인은 실행 파일이 아니다 — 엔진을 띄우고 붙는다."
    echo "  lldb <엔진 실행 파일 옆>/VulkanApp"
    echo "  (lldb) breakpoint set --file SwiftPlugin.swift --line 1  # 로드 전이라 pending"
    echo "  (lldb) run"
    echo "⚠️ C 심볼(CAD_PluginLoad)이 아니라 Swift 이름(pluginLoad)이나 파일:줄로 잡는다 —"
    echo "   @_cdecl 이 만드는 건 썽크라 줄 정보가 없다."
fi
