cmake_minimum_required(VERSION 3.16)
project(VulkanCADPlugins CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 엔진 SDK 위치 ──────────────────────────────────────────────────────────
# 플러그인은 엔진 **소스가 필요 없다.** 헤더 둘만 참조한다:
#   plugin/lot_plugin_sdk.h   내보낼 심볼 셋 + ABI 번호
#   api/VulkanCAD_API.h       엔진에게 시킬 것들
# 기본값은 옆에 나란히 클론했을 때의 경로. 다르면 -DVULKANCAD_ENGINE=<경로> 로 준다.
set(VULKANCAD_ENGINE "${CMAKE_CURRENT_SOURCE_DIR}/../3dEngine"
    CACHE PATH "엔진 레포(또는 SDK) 경로 — plugin/ 과 api/ 헤더를 여기서 찾는다")

if(NOT EXISTS "${VULKANCAD_ENGINE}/plugin/lot_plugin_sdk.h")
    message(FATAL_ERROR
        "엔진 헤더를 찾을 수 없습니다: ${VULKANCAD_ENGINE}/plugin/lot_plugin_sdk.h\n"
        "  -DVULKANCAD_ENGINE=<엔진 경로> 로 지정하세요.")
endif()

# ⚠️ 플러그인은 엔진 DLL 을 **링크하지 않는다.**
#    엔진이 이미 프로세스 안에 있고, 플러그인은 그 안으로 로드된다.
#    Windows 는 import library 가 필요하므로 예외적으로 링크한다(아래).
#    macOS/Linux 는 미정의 심볼을 로드 시점에 호스트에서 찾으므로 링크가 필요 없다.
set(VULKANCAD_IMPLIB "" CACHE FILEPATH
    "Windows 전용: VulkanCADCore.lib 경로. 비우면 엔진 빌드 폴더에서 찾는다")

function(add_vulkancad_plugin target)
    add_library(${target} MODULE ${ARGN})
    target_include_directories(${target} PRIVATE "${VULKANCAD_ENGINE}")
    # 플러그인은 접두사 없이 이름 그대로 (libFoo.so 가 아니라 Foo.so).
    # 엔진 로더가 확장자로만 거르므로 접두사는 혼란만 준다.
    set_target_properties(${target} PROPERTIES PREFIX "")

    # ⚠️ MSVC 는 소스가 UTF-8(BOM)이어도 **실행 문자집합**은 시스템 코드페이지(한글은 949)로
    #    굽는다. 그러면 플러그인이 넘긴 "건축" 이 엔진에선 깨져(???) 보인다 — 실측.
    #    엔진이 /utf-8 로 빌드되므로 플러그인도 맞춰야 한다. C API 문자열은 전부 UTF-8 약속.
    if(MSVC)
        target_compile_options(${target} PRIVATE "/utf-8")
    endif()

    if(WIN32)
        set(_lib "${VULKANCAD_IMPLIB}")
        if(NOT _lib)
            foreach(_cfg Debug Release RelWithDebInfo)
                if(EXISTS "${VULKANCAD_ENGINE}/build/${_cfg}/VulkanCADCore.lib")
                    set(_lib "${VULKANCAD_ENGINE}/build/${_cfg}/VulkanCADCore.lib")
                    break()
                endif()
            endforeach()
        endif()
        if(NOT _lib)
            message(FATAL_ERROR
                "VulkanCADCore.lib 를 찾을 수 없습니다. 엔진을 먼저 빌드하거나 "
                "-DVULKANCAD_IMPLIB=<경로> 로 지정하세요.")
        endif()
        target_link_libraries(${target} PRIVATE "${_lib}")
    elseif(APPLE)
        # 미정의 심볼은 로드 시점에 호스트(엔진)에서 찾는다.
        target_link_options(${target} PRIVATE "-undefined" "dynamic_lookup")
    endif()
endfunction()

add_subdirectory(HelloPlugin)
