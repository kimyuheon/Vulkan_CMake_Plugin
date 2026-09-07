cmake_minimum_required(VERSION 3.16)
project(VulkanCADPlugins CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 플러그인을 짓는 규칙(엔진 경로 확인 + add_vulkancad_plugin)은 모듈로 뺐다.
# QtPlugin/CMakeLists.txt 도 같은 모듈을 include 한다 — 그래야 Qt Creator 로 그 폴더만
# 열어도 똑같이 지어진다. 규칙이 두 곳에 갈라지지 않게 하려는 것이다.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VulkanCADPlugin)

add_subdirectory(HelloPlugin)

# QtPlugin 은 Qt 가 있을 때만 지어진다(하위 CMakeLists 가 스스로 판단해 빠진다).
# Qt 는 CMake 를 1급으로 지원하고 Qt Creator 도 CMakeLists.txt 를 그대로 연다 —
# MFC/Swift 와 달리 예제를 따로 뺄 이유가 없다. (qmake 로 열고 싶으면 QtPlugin/QtPlugin.pro)
add_subdirectory(QtPlugin)

# WallDialogPlugin(MFC 대화상자 예제)은 CMake 로 안 짓는다 — WallDialogPlugin/*.sln 참조.
# MFC 는 Windows 전용이라 크로스 플랫폼을 흉내 낼 이유가 없고, 대화상자를 리소스
# 편집기로 디자인하려면 vcxproj 가 자연스럽다(샘플 레포의 mfc_dlg_test 와 같은 방식).
#
# SwiftPlugin 도 CMake 로 안 짓는다 — SwiftPlugin/build.sh 참조.
# CMake 의 Swift 지원은 **Ninja/Xcode 제너레이터 전용**이라 여기에 넣으면 README 가
# 안내하는 `cmake -B build`(기본 Makefile 제너레이터)가 통째로 깨진다. swiftc 직접
# 호출이면 그 제약이 없고, 플래그도 세 줄이라 감쌀 이유가 없다.
