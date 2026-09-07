# QtPlugin — Qt Creator 로 열려면 이 파일을 연다. (CMakeLists.txt 를 열어도 된다)
#
#   Qt Creator → 파일 → 파일/프로젝트 열기 → QtPlugin/QtPlugin.pro
#
# 엔진 경로는 옆에 나란히 클론했을 때가 기본값이다. 다르면 프로젝트 설정의
# "빌드 단계 → qmake → 추가 인자" 에 넣거나, 명령행에서:
#   qmake6 VULKANCAD_ENGINE=/path/to/3dEngine && make

TEMPLATE = lib
TARGET   = QtPlugin
QT      += widgets
CONFIG  += c++17 plugin no_plugin_name_prefix
CONFIG  -= debug_and_release

# 플러그인은 접두사 없이 이름 그대로 (libQtPlugin.so 가 아니라 QtPlugin.so).
# 엔진 로더가 확장자로만 거르므로 접두사는 혼란만 준다 — CMakeLists 와 같은 규칙이다.
# no_plugin_name_prefix 가 그 일을 한다. 버전 접미사(.so.1.0.0)도 안 붙는다.

isEmpty(VULKANCAD_ENGINE): VULKANCAD_ENGINE = $$PWD/../../3dEngine
!exists($$VULKANCAD_ENGINE/plugin/lot_plugin_sdk.h) {
    error("엔진 헤더를 찾을 수 없습니다: $$VULKANCAD_ENGINE/plugin/lot_plugin_sdk.h — qmake VULKANCAD_ENGINE=<엔진 경로>")
}
INCLUDEPATH += $$VULKANCAD_ENGINE

SOURCES += qt_plugin.cpp

# ⚠️ 플러그인은 엔진을 **링크하지 않는다.** 엔진이 이미 프로세스 안에 있고
#    플러그인은 그 안으로 로드된다. 미정의 CAD_* 심볼은 로드 시점에 호스트에서 찾는다.
#    Windows 만 예외 — import library 가 없으면 링크가 안 된다.
win32-msvc*: QMAKE_CXXFLAGS += /utf-8   # 안 주면 실행 문자집합이 시스템 코드페이지(949)라 한글이 ??? 로 깨진다

win32 {
    VULKANCAD_CFGS = Debug Release RelWithDebInfo
    isEmpty(VULKANCAD_IMPLIB) {
        for(cfg, VULKANCAD_CFGS) {
            exists($$VULKANCAD_ENGINE/build/$$cfg/VulkanCADCore.lib) {
                VULKANCAD_IMPLIB = $$VULKANCAD_ENGINE/build/$$cfg/VulkanCADCore.lib
                break()
            }
        }
    }
    isEmpty(VULKANCAD_IMPLIB) {
        error("VulkanCADCore.lib 를 찾을 수 없습니다 — 엔진을 먼저 빌드하거나 qmake VULKANCAD_IMPLIB=<경로>")
    }
    LIBS += $$VULKANCAD_IMPLIB
}
macx: QMAKE_LFLAGS += -undefined dynamic_lookup
# Linux 는 ELF 가 미정의 심볼을 기본 허용이라 따로 줄 게 없다.

# 빌드 결과를 소스 폴더에 둔다 — 엔진 plugins/ 로 복사하기 편하게.
DESTDIR = $$PWD
