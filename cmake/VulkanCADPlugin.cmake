# 플러그인 하나를 짓는 규칙. 최상위 CMakeLists 와 QtPlugin/CMakeLists 둘 다 이 파일을
# include 한다 — 그래서 Qt Creator 로 QtPlugin/CMakeLists.txt 만 열어도 그대로 지어진다.

# ── 엔진 SDK 위치 ──────────────────────────────────────────────────────────
# 플러그인은 엔진 **소스가 필요 없다.** 헤더 둘만 참조한다:
#   plugin/lot_plugin_sdk.h   내보낼 심볼 셋 + ABI 번호
#   api/VulkanCAD_API.h       엔진에게 시킬 것들
# 기본값은 옆에 나란히 클론했을 때의 경로. 이 파일은 <레포>/cmake/ 에 있으므로
# 누가 include 하든(최상위든 QtPlugin 이든) 같은 곳을 가리킨다.
# 다르면 -DVULKANCAD_ENGINE=<경로> 로 준다.
get_filename_component(_vc_default_engine "${CMAKE_CURRENT_LIST_DIR}/../../3dEngine" ABSOLUTE)
set(VULKANCAD_ENGINE "${_vc_default_engine}"
    CACHE PATH "엔진 레포(또는 SDK) 경로 — plugin/ 과 api/ 헤더를 여기서 찾는다")

if(NOT EXISTS "${VULKANCAD_ENGINE}/plugin/lot_plugin_sdk.h")
    message(FATAL_ERROR
        "엔진 헤더를 찾을 수 없습니다: ${VULKANCAD_ENGINE}/plugin/lot_plugin_sdk.h\n"
        "  -DVULKANCAD_ENGINE=<엔진 경로> 로 지정하세요.\n"
        "  (Qt Creator: 프로젝트 → 빌드 → CMake 에서 VULKANCAD_ENGINE 을 고칩니다)")
endif()

# ⚠️ 플러그인은 엔진 DLL 을 **링크하지 않는다.**
#    엔진이 이미 프로세스 안에 있고, 플러그인은 그 안으로 로드된다.
#    Windows 는 import library 가 필요하므로 예외적으로 링크한다(아래).
#    macOS/Linux 는 미정의 심볼을 로드 시점에 호스트에서 찾으므로 링크가 필요 없다.
set(VULKANCAD_IMPLIB "" CACHE FILEPATH
    "Windows 전용: VulkanCADCore.lib 경로. 비우면 엔진 빌드 폴더에서 찾는다")

# ── 빌드하면 엔진 옆 plugins/ 로 바로 배치한다 ────────────────────────────
#
# 개발자는 Qt Creator(또는 IDE)에서 빌드 → 실행만 하고 싶다. 그래서 빌드 결과를
# 손으로 복사하지 않게 POST_BUILD 로 넣어 준다.
#
# ⚠️ 어디에 넣나: 엔진은 `loadPlugins("plugins")` 를 **상대 경로**로 부른다.
#    즉 기준은 실행 파일 위치가 아니라 **작업 디렉터리**다. 개발 중에는 그 둘이 같은
#    곳(엔진 빌드 폴더)이라 거기로 넣는다. 배포할 때는 실행 파일 옆이 맞다.
#
# 끄려면 -DVULKANCAD_PLUGINS_DIR= (빈 값). 다른 곳에 넣으려면 그 경로를 준다.
# ⚠️ 호스트마다 그 "작업 디렉터리" 가 다르다. 그래서 후보가 **여럿**이다:
#
#   · 엔진 단독 실행(VulkanApp)  → 엔진 빌드 폴더. 거기서 실행하는 것이 전제다
#     (에셋도 전부 상대 경로라, 다른 곳에서 띄우면 엔진 생성부터 실패한다 — 실측)
#   · 호스트 임베드(샘플 레포의 qml_test 등) → 그 호스트가 CAD_SetRuntimeAssetPath 로
#     넘긴 폴더. 그 함수는 std::filesystem::current_path 를 부르는 **진짜 chdir** 이라
#     이후의 "plugins" 가 그 폴더 기준이 된다. 샘플들은 sdk/ 를 넘기므로 sdk/plugins 다.
#
# 그래서 목록으로 둔다. 있는 곳 전부에 넣는다.
set(_vc_plugins "")
set(_vc_engine_exe "")
foreach(_dir "" "Debug" "Release" "RelWithDebInfo")
    foreach(_exe "VulkanApp" "VulkanApp.exe")
        if(EXISTS "${VULKANCAD_ENGINE}/build/${_dir}/${_exe}")
            get_filename_component(_p "${VULKANCAD_ENGINE}/build/${_dir}/plugins" ABSOLUTE)
            list(APPEND _vc_plugins "${_p}")
            if(NOT _vc_engine_exe)
                get_filename_component(_vc_engine_exe
                    "${VULKANCAD_ENGINE}/build/${_dir}/${_exe}" ABSOLUTE)
            endif()
        endif()
    endforeach()
endforeach()
# 샘플 레포(호스트 임베드 예제들)가 옆에 있으면 그 sdk/ 도 후보다.
foreach(_lib "libVulkanCADCore.so" "libVulkanCADCore.dylib" "VulkanCADCore.dll")
    if(EXISTS "${VULKANCAD_ENGINE}/../3dEngine_Sample/sdk/${_lib}")
        get_filename_component(_p "${VULKANCAD_ENGINE}/../3dEngine_Sample/sdk/plugins" ABSOLUTE)
        list(APPEND _vc_plugins "${_p}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _vc_plugins)

set(VULKANCAD_PLUGINS_DIR "${_vc_plugins}" CACHE STRING
    "빌드한 플러그인을 복사해 둘 폴더들(;로 구분). 비우면 복사하지 않는다")

if(VULKANCAD_PLUGINS_DIR)
    foreach(_p IN LISTS VULKANCAD_PLUGINS_DIR)
        get_filename_component(_vc_run_dir "${_p}" DIRECTORY)
        message(STATUS "플러그인 설치 위치: ${_p}")
        message(STATUS "  └ 호스트의 작업 디렉터리는 여기여야 한다: ${_vc_run_dir}")
    endforeach()
else()
    message(STATUS "엔진 빌드 폴더를 못 찾아 플러그인을 복사하지 않습니다 "
                   "(-DVULKANCAD_PLUGINS_DIR=<경로> 로 지정 가능)")
endif()

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

    # Visual Studio 에서 이 플러그인을 시작 프로젝트로 두고 F5 를 누르면 엔진이 뜨게 한다.
    # ⚠️ Windows 에는 exec 가 없어서 run_engine 런처로는 디버깅이 안 된다(_execv 는 부모를
    #    끝내고 새 프로세스를 만들어 디버거가 따라오지 못한다). 대신 VS 는 "디버깅 시 실행할
    #    명령" 을 프로젝트 속성으로 받으므로, 그것을 CMake 가 채워 준다 — ObjectARX 에서
    #    디버그 대상을 acad.exe 로 잡는 그 설정이다. (Linux/macOS 는 run_engine 이 한다)
    if(MSVC AND VULKANCAD_ENGINE_EXE)
        get_filename_component(_vc_dbg_dir "${VULKANCAD_ENGINE_EXE}" DIRECTORY)
        set_target_properties(${target} PROPERTIES
            VS_DEBUGGER_COMMAND           "${VULKANCAD_ENGINE_EXE}"
            VS_DEBUGGER_WORKING_DIRECTORY "${_vc_dbg_dir}")
    endif()

    # run_engine 으로 실행하기 전에 이 플러그인이 지어져 배치되도록 묶는다.
    if(TARGET run_engine)
        add_dependencies(run_engine ${target})
    endif()

    foreach(_p IN LISTS VULKANCAD_PLUGINS_DIR)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${_p}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:${target}>" "${_p}/"
            COMMENT "설치: ${_p}/$<TARGET_FILE_NAME:${target}>"
            VERBATIM)
    endforeach()
endfunction()

# ── IDE 의 Run 버튼이 누를 것 ──────────────────────────────────────────────
#
# 플러그인은 라이브러리라 실행 타겟이 없다. 그래서 IDE 마다 "Custom Executable" 을
# 손으로 만들어야 하는데, 그 설정이 들어가는 .user 파일은 **PC 마다 다른 파일**이라
# 버전 관리로 공유되지 않는다(Qt Creator 의 의도된 설계다).
#
# 실행 타겟을 **소스로** 두면 그 문제가 사라진다 — 클론해서 열면 어느 PC 에서든
# 설정 없이 Run/F5 가 된다. run_engine 은 엔진 폴더로 chdir 한 뒤 엔진으로 exec 한다.
# 작업 디렉터리 함정(에셋·plugins 가 CWD 기준)도 스스로 해결한다. cmake/run_engine.cpp 참조.
set(VULKANCAD_ENGINE_EXE "${_vc_engine_exe}" CACHE FILEPATH
    "엔진 실행 파일. 있으면 run_engine 실행 타겟을 만든다")

if(VULKANCAD_ENGINE_EXE AND NOT TARGET run_engine)
    get_filename_component(_vc_run_dir "${VULKANCAD_ENGINE_EXE}" DIRECTORY)
    add_executable(run_engine "${CMAKE_CURRENT_LIST_DIR}/run_engine.cpp")
    target_compile_definitions(run_engine PRIVATE
        VULKANCAD_ENGINE_EXE="${VULKANCAD_ENGINE_EXE}"
        VULKANCAD_RUN_DIR="${_vc_run_dir}")
    message(STATUS "실행 타겟 run_engine → ${VULKANCAD_ENGINE_EXE}")
endif()
