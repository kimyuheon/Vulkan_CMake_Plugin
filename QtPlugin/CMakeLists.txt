# QtPlugin — Qt 위젯 대화상자를 띄우는 플러그인.
#
# Qt 가 없으면 조용히 건너뛴다. 나머지 예제(HelloPlugin)는 그대로 빌드된다 —
# Qt 하나 때문에 `cmake -B build` 가 실패하면 안 된다.
#
# Qt 를 못 찾으면 경로를 알려 준다:
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64

# 이 폴더만 단독으로 열어도(Qt Creator 로 QtPlugin/CMakeLists.txt 를 여는 경우) 지어진다.
# 최상위에서 add_subdirectory 로 들어올 때는 이 블록을 건너뛴다 — project() 가 두 번
# 불리면 안 되고, 규칙(add_vulkancad_plugin)은 이미 위에서 들어와 있다.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.16)
    project(QtPlugin CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
    include(VulkanCADPlugin)
endif()

find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)
if(NOT QT_FOUND)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        # 단독으로 열었으면 지을 게 이것뿐이다 — 조용히 빠지면 안 된다.
        message(FATAL_ERROR "Qt 를 찾을 수 없습니다 (-DCMAKE_PREFIX_PATH=<Qt 경로>)")
    endif()
    message(STATUS "QtPlugin: Qt 를 못 찾아 건너뜁니다 (-DCMAKE_PREFIX_PATH=<Qt 경로>)")
    return()
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
message(STATUS "QtPlugin: Qt ${QT_VERSION} (${Qt${QT_VERSION_MAJOR}_DIR})")

# 이 예제는 Q_OBJECT 를 안 쓰지만(qt_plugin.cpp 의 주석 참조), 시그널을 하나라도
# 추가하는 순간 필요해지므로 미리 켜 둔다. 켜 두는 비용은 거의 없다.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)   # .ui 파일을 Qt Designer 로 그리고 싶을 때
set(CMAKE_AUTORCC ON)

add_vulkancad_plugin(QtPlugin qt_plugin.cpp)
target_link_libraries(QtPlugin PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
