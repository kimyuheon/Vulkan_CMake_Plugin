# QtPlugin — Qt 위젯 대화상자를 띄우는 플러그인.
#
# Qt 가 없으면 조용히 건너뛴다. 나머지 예제(HelloPlugin)는 그대로 빌드된다 —
# Qt 하나 때문에 `cmake -B build` 가 실패하면 안 된다.
#
# Qt 를 못 찾으면 경로를 알려 준다:
#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64

find_package(QT NAMES Qt6 Qt5 QUIET COMPONENTS Widgets)
if(NOT QT_FOUND)
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
