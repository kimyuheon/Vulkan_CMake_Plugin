# VulkanCAD 플러그인

[VulkanCAD 엔진](https://github.com/kimyuheon/Vulkan_CMake)에 붙이는 플러그인 예제입니다.
**엔진 소스는 필요 없습니다** — 헤더 둘만 참조합니다.

```
plugin/lot_plugin_sdk.h   내보낼 심볼 셋과 ABI 번호
api/VulkanCAD_API.h       엔진에게 시킬 것들 (명령 200개 이상)
```

## 플러그인이 하는 일

명령을 등록하면 **명령행이 곧 진입점**이 됩니다. 메뉴·툴바는 이름만 올리면 엔진이 그립니다.

```cpp
LOT_PLUGIN_ABI_EXPORT bool CAD_PluginLoad(unsigned int id) {
    CAD_RegisterCommand("wall", "벽 만들기", &onWall, nullptr, id);
    CAD_AddUiItem(0, "건축", "벽 만들기", "wall", "", id);   // 메뉴
    CAD_AddUiItem(2, "건축/벽체", "벽", "wall", "", id);      // 툴바
    return true;
}
```

`id` 는 엔진이 발급합니다. **등록할 때 `owner` 로 그 값을 써야** 언로드할 때 엔진이 묶어서
지웁니다. 안 쓰면 DLL 이 내려간 뒤에도 등록이 남아 사라진 코드를 부르게 됩니다.

## 내보내야 하는 심볼

셋 중 하나라도 없으면 엔진이 로드를 포기합니다.

| 심볼 | 하는 일 |
|---|---|
| `CAD_PluginAbiVersion` | `LOT_PLUGIN_ABI_VERSION` 을 그대로 반환 |
| `CAD_PluginLoad(id)` | 명령·UI 등록. `false` 면 로드 취소 |
| `CAD_PluginUnload()` | 자기 정리만. 등록물은 엔진이 지웁니다 |

## 빌드

엔진을 먼저 빌드해 두어야 합니다 (Windows 는 `VulkanCADCore.lib` 가 필요).

```bash
cmake -B build -DVULKANCAD_ENGINE=<엔진 경로>
cmake --build build --config Debug
```

엔진 레포를 옆에 나란히 클론했다면 `-DVULKANCAD_ENGINE` 은 생략해도 됩니다.

`QtPlugin` 은 **Qt 가 있을 때만** 함께 지어집니다. 못 찾으면 그 예제만 조용히 빠지고
나머지는 그대로 빌드됩니다 — Qt 하나 때문에 전체가 실패하면 안 되니까요.
시스템에 없는 Qt(공식 온라인 설치본 등)를 쓰려면 경로를 알려 주세요.

```bash
cmake -B build -DCMAKE_PREFIX_PATH=~/Qt/6.7.0/gcc_64
```

## 설치

빌드된 파일을 `plugins/` 에 넣습니다. 엔진이 시작할 때 자동으로 훑습니다.

```
VulkanApp.exe
plugins/
  HelloPlugin.dll      ← Windows
  HelloPlugin.so       ← Linux
  HelloPlugin.dylib    ← macOS
```

**⚠️ 기준은 실행 파일 위치가 아니라 "현재 작업 디렉터리" 입니다.** 엔진은
`loadPlugins("plugins")` 를 **상대 경로**로 부르고, `std::filesystem` 이 그걸 CWD 기준으로
풉니다. exe 를 더블클릭하면 둘이 우연히 같아서 티가 안 나지만, IDE 나 스크립트로 띄우면
갈립니다 — 다른 폴더에서 엔진을 띄우면 플러그인은커녕 모델·텍스처도 못 찾아
`CAD_CreateEngine` 부터 실패합니다(실측).

호스트가 `CAD_SetRuntimeAssetPath` 를 부르면 그 함수가 **진짜 chdir**(`current_path`) 이라,
그 뒤로는 그 폴더가 기준이 됩니다. 그래서 실제로 넣을 곳은 이렇게 갈립니다.

| 어떻게 띄우나 | plugins/ 를 둘 곳 |
|---|---|
| 엔진 단독(`VulkanApp`) | 엔진 빌드 폴더 — 거기서 실행하는 것이 전제 |
| 호스트 임베드(샘플 레포의 `qml_test` 등) | 호스트가 `CAD_SetRuntimeAssetPath` 로 넘긴 폴더 = `sdk/plugins` |

CMake 로 지으면 **찾을 수 있는 곳 전부에 자동으로 복사**합니다(`cmake -B ...` 출력에 경로가
찍힙니다). 끄려면 `-DVULKANCAD_PLUGINS_DIR=`, 직접 정하려면 그 변수에 폴더들을 `;` 로 줍니다.

## 주의할 것

**ABI 가 맞아야 붙습니다.** 엔진과 같은 컴파일러·런타임으로 빌드해야 합니다. Windows 라면
같은 Visual Studio 버전, 같은 구성(Debug/Release)입니다. 번호가 다르면 엔진이 아예 올리지
않고 이유를 알려줍니다 — 반쯤 붙어서 조용히 죽는 것보다 낫습니다. AutoCAD 가 릴리스마다
"이 VS 로 빌드하라" 를 명시하는 것과 같은 이유입니다.

**문자열은 전부 UTF-8 입니다.** MSVC 는 소스가 UTF-8(BOM)이어도 실행 문자집합은 시스템
코드페이지로 굽기 때문에, 한글 메뉴 이름이 엔진에서 `???` 로 깨집니다. SDK 의 CMake 함수가
`/utf-8` 을 붙여 주지만, 직접 빌드 스크립트를 쓴다면 잊지 마세요.

**기본 명령 이름은 못 씁니다.** `line`, `circle` 같은 엔진 명령과 겹치면 등록이 거부됩니다.
조회에서 기본 명령이 이기므로, 등록만 되고 안 불리는 것보다 거부가 낫습니다.

## 커스텀 엔티티 — 벽 예제

새 객체 종류를 만들지 않습니다. **엔진에는 평범한 메시**를 주고, 거기에 **딱지**를 붙입니다.

```cpp
id = CAD_CreateMesh(xyz, nVerts, idx, nIdx, NULL);      // 형상 (법선 NULL = flat)
CAD_SetPluginTag(id, "HelloPlugin", "wall", "{...}");   // 정의는 플러그인만 해석
```

객체가 메시라서 그리기·선택·이동·저장·내보내기가 전부 그대로 됩니다.

**값이 바뀌면 형상을 다시 만듭니다.** `wallthick` 명령이 그 예입니다 — 딱지의 두께를
두 배로 바꾸고 `CAD_ReplaceMesh` 로 형상만 교체합니다. 위치·색·딱지는 그대로입니다.

**`.lot` 을 열면 엔진이 알려줍니다.** `CAD_SetOnPluginEntityLoaded` 콜백으로 owner/type/data 가
오고, 자기 것이면 정의를 읽어 살아 있는 엔티티로 넘겨받습니다. 이 플러그인이 없는 PC 에서
열어도 형상은 보이고 딱지는 보존됩니다 — 다시 저장해도 안 사라집니다.

**사용자가 메시를 직접 고치면** (그립·불리언·밀당·분해) 엔진이 먼저 묻고, "계속" 이면
딱지를 뗍니다. 정의와 형상이 어긋난 채 남지 않게 하려는 것입니다. 호스트 임베드(MFC/WPF)는
ImGui 창이 없으니 `CAD_SetOnConfirm` 으로 대신 물어야 합니다.

## UI 종류

`CAD_AddUiItem(kind, ...)` 의 kind 로 갈립니다.

| kind | 무엇 | 어디에 |
|---|---|---|
| 0 / 1 | 메뉴 항목 / 구분선 | 메뉴바 오른쪽 |
| 2 | 툴바 버튼 | 좌측 세로 패널 |
| 3 | 리본 버튼 | 메뉴바 바로 아래, 탭/그룹 |
| 4 | 패널 | 도킹 창 |

메뉴·툴바·리본은 이름만 올리면 엔진이 그립니다. **패널만 예외**로 내용을 플러그인이
직접 그려야 하고, 그러려면 엔진과 같은 ImGui 를 컴파일해 넣고
`CAD_GetImGuiContext` / `CAD_GetImGuiAllocators` 로 컨텍스트와 할당자를 받아
`SetCurrentContext` + `SetAllocatorFunctions` 를 먼저 불러야 합니다. DLL 경계를 넘으면
ImGui 전역과 힙이 공유되지 않기 때문입니다. **호스트 임베드에선 패널이 안 뜹니다.**

값 편집만 필요하면 `CAD_AddEntityProperty`(특성창)가 이 제약을 전부 피합니다.
패널은 그래프·미리보기처럼 정말 직접 그려야 하는 것에만 쓰세요.

**대화상자는 이 표 밖입니다.** 플러그인이 직접 띄우므로 MFC 든 Win32 든 Qt 든 자유이고,
ImGui 도 호스트 종류도 상관없습니다 — `WallDialogPlugin`(MFC)과 `QtPlugin`(Qt)이 그 예입니다.

## 이벤트

```cpp
CAD_AddEventListener(1 | 2, &onEvent, nullptr, id);   // 1=생성 2=삭제 4=선택 8=문서변경
```

`CAD_SetOnObjectCreated` 같은 단일 슬롯은 **쓰지 마세요.** 호스트가 걸어 둔 것을
덮어씁니다. 플러그인은 여러 개가 동시에 올라오므로 목록형인 위쪽을 씁니다.

## 예제 셋

| 예제 | 빌드 | 보여주는 것 |
|---|---|---|
| `HelloPlugin` | CMake (3 OS) | 명령·메뉴·툴바·리본·엔티티·속성·이벤트 — ImGui 없이 |
| `WallDialogPlugin` | vcxproj (Windows) | 플러그인이 **자기 MFC 대화상자**를 띄운다 |
| `QtPlugin` | CMake 또는 qmake (3 OS) | **Qt 위젯 대화상자** — 같은 소스가 GLFW 엔진과 Qt 호스트 둘 다 |
| `WpfPlugin` | vcxproj + dotnet (Windows) | **C#/WPF** 플러그인 — 네이티브 새시가 .NET 런타임을 띄운다 |
| `SwiftPlugin` | swiftc (macOS/Linux) | **Swift** 플러그인 — 새시 없이 바로 붙는다 + 자기 Cocoa 창 |

## C# / WPF 플러그인

C# 은 두 갈래가 있습니다.

**NativeAOT** — C# 을 네이티브 DLL 로 컴파일하면 엔진이 그냥 로드합니다. 새시가 필요 없고
가볍지만 **WPF 를 못 씁니다**(NativeAOT 가 지원하지 않습니다).

**런타임 호스팅** — `WpfPlugin` 이 쓰는 방식입니다. 네이티브 새시(`WpfBridgePlugin`)가
`hostfxr` 로 .NET 런타임을 띄우고 관리 어셈블리를 불러들입니다. AutoCAD 의 `acmgd.dll` 과
같은 자리입니다. 엔진 입장에선 그냥 평범한 C++ 플러그인이라 **엔진은 .NET 을 전혀 모릅니다.**

```
plugins/
  WpfBridgePlugin.dll        ← 새시 (엔진이 로드)
  WpfPlugin/                 ← 관리 어셈블리는 하위 폴더에
    WpfPlugin.dll
    WpfPlugin.runtimeconfig.json
```

관리 어셈블리를 `plugins/` 최상위에 두면 엔진 로더가 그것도 네이티브 플러그인인 줄 알고
열어보다 실패 메시지를 찍습니다. 하위 폴더로 묶으세요.

**⚠️ 언로드가 안 됩니다.** .NET 런타임은 한 번 뜨면 프로세스가 끝날 때까지 못 내립니다.
AutoCAD 도 `NETLOAD` 한 어셈블리는 재시작해야 내려가니 같은 제약입니다.
C++ 플러그인은 실행 중 언로드가 됩니다.

**⚠️ WPF 창은 STA 스레드에서만 뜹니다.** 엔진의 주 스레드는 STA 도 아니고 Dispatcher 도
없어서, 전용 STA 스레드를 만들어 거기서 띄우고 닫힐 때까지 기다립니다(모달처럼).

## Qt 플러그인

`WallDialogPlugin`(MFC)과 **같은 일**을 하지만 Windows·macOS·Linux 에서 다 돕니다.
어려운 곳은 대화상자가 아니라 **QApplication 부트스트랩**입니다. MFC 는 `CWinApp` 전역
하나면 끝났지만, Qt 는 상황이 둘로 갈립니다.

| 엔진이 뜬 방식 | qApp | 플러그인이 하는 일 |
|---|---|---|
| GLFW 창 (기본) | 없다 | 우리가 `QApplication` 을 만들고 `exec()` 로 **중첩 루프**를 돈다 |
| Qt 호스트가 `CAD_AttachView` 로 품음 | 이미 있다 | 만들지 않는다. 루프가 이미 도니 **모덜리스**로 띄운다 |

**같은 소스가 둘 다 처리합니다.** `qApp` 이 있는지만 보면 됩니다.

```cpp
if (QCoreApplication* app = QCoreApplication::instance()) { ... }  // 얹어 탄다
else { new QApplication(g_argc, g_argv); }                         // 우리가 띄운다
```

### 빌드 — CMake 든 Qt Creator 든

```bash
cmake -B build && cmake --build build          # 최상위에서 같이 지어진다
```

**Qt Creator 로 열려면** `QtPlugin/QtPlugin.pro` 나 `QtPlugin/CMakeLists.txt` 를 엽니다.
둘 다 **단독으로** 열립니다 — CMake 쪽은 최상위와 같은 규칙 모듈(`cmake/VulkanCADPlugin.cmake`)을
가져다 쓰므로 최상위에서 지을 때와 결과가 같습니다.

처음 열면 *Configure Project* 화면이 나옵니다.

1. **Desktop** 킷만 체크합니다 (`Python 3.12.3` 같은 건 끕니다 — C++ 플러그인입니다)
2. Debug/Release 중 쓸 것만 남겨도 됩니다
3. **Configure Project**

엔진 레포를 옆에 나란히 클론했다면 그대로 빌드됩니다. 아니면 경로를 알려 줍니다.

| 여는 방식 | 어디서 고치나 |
|---|---|
| CMakeLists.txt | 프로젝트 → 빌드 → CMake → `VULKANCAD_ENGINE` |
| QtPlugin.pro | 프로젝트 → 빌드 → qmake → 추가 인자에 `VULKANCAD_ENGINE=<경로>` |

명령행이면 이렇습니다.

```bash
qmake6 VULKANCAD_ENGINE=<엔진 경로> && make      # qmake
cmake -B build -S QtPlugin -DVULKANCAD_ENGINE=<엔진 경로>   # CMake 단독
```

### Qt Creator 에서 바로 실행·디버깅

플러그인은 실행 파일이 아니라 라이브러리라, IDE 가 저절로 실행해 주지는 않습니다.
**엔진을 실행 파일로 지정**하면 됩니다 — ObjectARX 에서 디버그 대상을 `acad.exe` 로
잡는 것과 같습니다. 두 가지가 필요합니다.

**1. 빌드 결과가 엔진 옆으로 가야 한다** — 이건 자동입니다. 엔진 빌드 폴더를 찾으면
빌드할 때마다 거기 `plugins/` 로 복사합니다(`cmake -B ...` 출력에 경로가 찍힙니다).

```
-- 플러그인 설치 위치: <엔진>/build/plugins
-- IDE 실행 설정 — 실행 파일: <엔진>/build/VulkanApp
-- IDE 실행 설정 — 작업 디렉터리: <엔진>/build
```

끄려면 `-DVULKANCAD_PLUGINS_DIR=`(빈 값), 다른 곳에 넣으려면 그 경로를 줍니다.

**⚠️ 작업 디렉터리가 중요합니다.** 엔진은 `loadPlugins("plugins")` 를 **상대 경로**로
부릅니다. 기준이 실행 파일 위치가 아니라 **작업 디렉터리**라, 거기를 엔진 빌드 폴더로
맞춰야 플러그인이 보입니다.

**2. 실행 타겟이 이미 있다** — `run_engine`. CMake 가 엔진 실행 파일을 찾으면 자동으로
만듭니다. 설정할 것이 없습니다.

실행 대상에서 `run_engine` 을 고르고 **실행(Ctrl+R)** 이면 빌드 → 배치 → 엔진 실행까지
갑니다. **디버그(F5)** 도 그대로 됩니다 — `qt_plugin.cpp` 에 중단점을 걸면 플러그인이
아직 로드되기 전이라 pending 으로 잡혀 있다가, 엔진이 `plugins/` 를 훑어 `dlopen` 하는
순간 붙습니다(실측).

```
Thread 1 "VulkanApp" hit Breakpoint 1, CAD_PluginLoad (pluginId=2)
    at .../QtPlugin/qt_plugin.cpp:285
```

`run_engine` 은 15줄짜리 런처입니다(`cmake/run_engine.cpp`). 엔진 폴더로 `chdir` 한 뒤
엔진으로 **`exec`** 합니다. 자식 프로세스를 새로 띄우면 디버거가 런처에 붙어 있고 엔진을
놓치지만, `exec` 는 같은 프로세스를 갈아치우므로 디버거가 그대로 따라옵니다.
작업 디렉터리 함정도 자기가 `chdir` 해서 없앱니다.

**⚠️ 실행 타겟을 소스로 두는 이유**가 이것입니다. IDE 의 실행 설정은 `.user` 파일에
들어가는데 그건 **PC 마다 다른 파일**이라 버전 관리로 공유되지 않습니다(Qt Creator 의
의도된 설계입니다). 타겟으로 두면 클론해서 여는 누구나 설정 없이 F5 가 됩니다.

| 플랫폼 | 실행 | 디버깅 |
|---|---|---|
| Linux / macOS | `run_engine` | 됨 — 위 실측 |
| Windows + Visual Studio | — | 됨 — CMake 가 `VS_DEBUGGER_COMMAND` 를 채운다 |
| Windows + Qt Creator | `run_engine` | Custom Executable 을 한 번 만들어야 한다 |

Windows 에 `exec` 가 없어서 생기는 차이입니다 — `_execv` 는 부모를 끝내고 새 프로세스를
만들어 디버거가 따라오지 못합니다. 그래서 거기서는 "디버깅 시 실행할 명령" 을 IDE 에
알려 주는 쪽을 씁니다(실행 파일 `<엔진>/VulkanApp.exe`, 작업 디렉터리 `<엔진 폴더>`).
ObjectARX 에서 디버그 대상을 `acad.exe` 로 잡는 그 설정과 같습니다.

`.pro` 는 `no_plugin_name_prefix` 로 `lib` 접두사를 뗍니다 — 엔진 로더는 확장자로만
거르므로 `libQtPlugin.so` 가 아니라 `QtPlugin.so` 여야 자연스럽습니다(CMake 쪽과 같은 규칙).

### 실측

Qt 6.4.2 / Linux(Wayland). 두 가지로 쟀습니다 — **진짜 엔진**을 띄워서, 그리고 엔진 계약과
같은 모양의 하네스로 `dlopen(RTLD_NOW|RTLD_LOCAL)` 해서(엔진 로더와 같은 플래그).

진짜 엔진에서:

| 확인한 것 | 결과 |
|---|---|
| 엔진이 `plugins/` 를 훑어 로드 | `[plugin] 로드: QtPlugin (id=2)` — HelloPlugin 과 나란히 붙는다 |
| 명령 실행 뒤 프로세스 상태 | 대화상자를 닫을 때까지 **반환하지 않는다**(모달, 설계대로) |
| 그때 프로세스가 올린 것 | `libQt6Widgets` + `libQt6WaylandClient` + EGL 통합 → **진짜 네이티브 창**이다 |

즉 GLFW/Vulkan 이 이미 도는 프로세스 안에서 Qt 가 자기 플랫폼을 따로 올려 창을 띄웁니다.
둘은 각자의 연결로 컴포지터와 이야기하므로 서로의 이벤트를 훔치지 않습니다.

하네스에서(경계 조건들):

| 확인한 것 | 결과 |
|---|---|
| Qt 호스트 위에서 명령 실행 | 20ms 만에 반환, 창은 떠 있음 → **호스트 루프가 안 막힌다** |
| 중첩 루프 안에서 큐 이벤트 | 돈다 (`exec()` 가 진짜 이벤트 루프다) |
| 언로드 뒤 `dlclose` | 코드가 **실제로 언매핑된다** — Qt 를 띄운 뒤에도 |
| 언로드 → 재로드 | 명령이 다시 모달로 동작 |

**언로드가 진짜로 됩니다.** .NET 은 아예 못 내려가고 Swift 는 `dlclose` 가 0 을 돌려주면서도
매핑이 남았지만, Qt 플러그인은 C++ 플러그인이라 평범하게 내려갑니다. 대신 **조건이 있습니다**
— 아래 두 가지를 지켜야 합니다.

### 걸리는 것

**⚠️ 우리 위젯은 언로드 때 `delete` 로 지웁니다.** `close()` 만으로는 부족합니다.
`WA_DeleteOnClose` 의 실제 삭제는 `deleteLater` 라 **이벤트 루프가 한 바퀴 더 돌아야**
일어나는데, 엔진은 `CAD_PluginUnload` 직후 `dlclose` 를 부르므로 그 한 바퀴가 오지 않습니다.
남은 창을 Qt 가 나중에 건드리면 사라진 코드(우리 `QDialog` 의 vtable)를 부릅니다.
실측에서 `close()` 는 언로드 뒤에도 창을 1개 남겼고, `delete` 로 바꿔 0개가 됐습니다.

**⚠️ `QApplication` 은 언로드해도 지우지 않습니다.** 한 프로세스에서 한 번만 만드는 게
안전합니다. 그래서 **재로드하면 함정이 하나 생깁니다** — 새로 올라온 플러그인이 `qApp` 이
있는 것만 보고 "호스트가 Qt 구나" 로 오판해, 돌지도 않는 이벤트 루프를 믿고 모덜리스로
띄우면 창이 그대로 얼어붙습니다. 그래서 "이건 플러그인이 만든 것" 이라는 표식을
`QApplication` 객체의 동적 속성에 남깁니다. 표식이 앱과 함께 남으므로 재로드해도 사실이
유지됩니다(위 실측표의 마지막 줄).

**⚠️ 플랫폼 플러그인 경로.** Qt 는 `qwindows.dll` / `libqxcb.so` 를 **실행 파일 옆**에서
찾습니다. 플러그인은 `plugins/` 안에 있으므로, `QApplication` 을 만들기 **전에**
자기 폴더를 `QCoreApplication::addLibraryPath` 로 넣어 줍니다. 안 하면
"could not load the Qt platform plugin" 으로 그 자리에서 abort 합니다.

**⚠️ Qt 초기화는 명령을 처음 부를 때 합니다.** `CAD_PluginLoad` 에서 하지 않습니다 —
로드는 엔진 시작 중에 일어나고, 쓰지도 않을 Qt 를 그때 올리면 시작이 느려질 뿐 아니라
플랫폼 플러그인이 없는 환경에서는 **엔진 자체가** 못 뜹니다.

**⚠️ 호스트가 Qt 라면 버전이 같아야 합니다.** 한 프로세스에 Qt5 와 Qt6 이 같이 올라오면
심볼이 겹쳐 죽습니다. ABI 규칙(같은 컴파일러·같은 구성)이 Qt 에도 그대로 적용됩니다.

## Swift 플러그인

**새시가 없습니다.** C# 은 네이티브 DLL 이 .NET 런타임을 띄워야 했지만, Swift 는 그 층이
통째로 없습니다. `@_cdecl` 이 심볼을 C 이름 그대로 내보내고 `swiftc -emit-library` 가 평범한
`.dylib` 을 만듭니다 — 엔진은 이게 `HelloPlugin` 인지 `SwiftPlugin` 인지 구분하지 못합니다.

```swift
@_cdecl("CAD_PluginAbiVersion")
public func pluginAbiVersion() -> UInt32 { LOT_PLUGIN_ABI_VERSION }

@_cdecl("CAD_PluginLoad")
public func pluginLoad(_ id: UInt32) -> Bool {
    CAD_RegisterCommand("swall", "벽 만들기", onWall, nil, id)
    CAD_AddUiItem(0, "건축(Swift)", "벽 만들기", "swall", "", id)
    return true
}
```

엔진 헤더는 **래퍼 한 줄 없이** 그대로 씁니다. modulemap 한 장이면 `import VulkanCAD` 로
명령 200개가 전부 열립니다. 계약이 C++ 인터페이스가 아니라 **순수 C** 라서 되는 일입니다 —
`lot_plugin_sdk.h` 가 "지금은 C API 만 쓴다" 고 정해 둔 결정의 배당금입니다.

```bash
cd SwiftPlugin && ./build.sh
cp SwiftPlugin.dylib <엔진 실행 파일 옆>/plugins/
```

빌드가 CMake 가 아닌 이유: CMake 의 Swift 지원은 **Ninja/Xcode 제너레이터 전용**이라,
최상위에 넣으면 위의 `cmake -B build`(기본 Makefile 제너레이터)가 깨집니다.

### 디버깅

플러그인은 `.dylib` 이라 혼자 못 뜹니다. **디버기는 엔진이고**, 플러그인은 엔진이 `dlopen`
할 때 붙습니다 — MFC/WPF 예제가 vcxproj 의 "디버깅 명령" 에 엔진 exe 를 걸어 두는 것과
같은 얼개입니다.

VS Code 는 `.vscode/launch.json` 이 들어 있어 **F5 하나면** 됩니다(CodeLLDB 확장 필요).
짓고 → `plugins/` 에 설치하고 → 엔진을 띄웁니다. lldb 를 직접 쓰면:

```
./build.sh debug
lldb <엔진 실행 파일 옆>/VulkanApp
(lldb) breakpoint set --file SwiftPlugin.swift --line 232   # 아직 로드 전이라 pending
(lldb) run
```

브레이크포인트를 **로드 전에** 걸어도 됩니다. 없는 모듈이라 pending 으로 남았다가 엔진이
`dlopen` 하는 순간 풀립니다.

```
* frame #0: SwiftPlugin.dylib`pluginLoad(id=1) at SwiftPlugin.swift:232:11
  frame #1: SwiftPlugin.dylib`CAD_PluginLoad at <compiler-generated>:0
  frame #2: libVulkanCADCore.dylib`lot::PluginManager::load(...)
(lldb) frame variable id     → (UInt32) id = 1
(lldb) expression pluginId   → (UInt32) $R0 = 1
```

**⚠️ 디버그 빌드는 두 단계로 짓습니다.** 한 번에 `-g -emit-library` 로 지으면 lldb 가 줄을
못 짚습니다(`<compiler-generated>` 로만 잡힙니다). Mach-O 는 DWARF 를 실행물에 넣지 않고
"디버그맵" 으로 `.o` 를 가리키는데, swiftc 가 그 `.o` 를 임시 폴더에 만들고 링크가 끝나면
**지워 버리기** 때문입니다. `build.sh debug` 는 `.o` 를 남기고 `dsymutil` 로 `.dSYM` 을 만들어
자립시킵니다 — 엔진 옆으로 옮길 때 **`.dSYM` 도 같이** 가야 합니다.
(Linux/ELF 는 DWARF 가 `.so` 안에 들어가서 이 문제가 없습니다)

**⚠️ C 심볼에 직접 걸면 줄이 안 나옵니다.** `@_cdecl` 이 만드는 `CAD_PluginLoad` 는 썽크라
`<compiler-generated>` 입니다. Swift 이름(`pluginLoad`)이나 파일:줄로 잡으세요.

### macOS 는 창 띄우기가 WPF 보다 쉽습니다

WPF 는 엔진 주 스레드가 STA 도 Dispatcher 도 아니라 **전용 STA 스레드**를 파야 했습니다.
macOS 에는 그 우회가 필요 없습니다. 엔진 루프의 `glfwPollEvents` 가

```objc
while (e = [NSApp nextEventMatchingMask:NSEventMaskAny ... dequeue:YES])
    [NSApp sendEvent:e];
```

즉 **프로세스의 NSApp 큐를 통째로 비워 각 창으로 라우팅**하기 때문에, 플러그인이 만든
NSWindow 는 엔진 루프를 그냥 얻어 탑니다. 스레드도 모달 루프도 우리가 만들지 않습니다.

GLFW 와 같은 모양의 논블로킹 펌프를 재현해 실측한 결과입니다.

| 확인한 것 | 결과 |
|---|---|
| 명령에서 연 NSWindow | 뜬다 |
| 펌프 1초 뒤 `viewsNeedDisplay` | 예 → **아니오** (표시 사이클이 실제로 돌았다) |
| NSApp 큐에 넣은 진짜 클릭 | 펌프 → 창 → 버튼 → `CAD_CreateMesh` 까지 도달 |

다만 AppKit 은 **주 스레드 전용**입니다. 명령 콜백은 엔진 스레드에서 오고 macOS 에서 그건
주 스레드지만, 예제는 그 가정을 `Thread.isMainThread` 로 명시적으로 확인합니다.
모달(`NSApp.runModal`)은 일부러 안 씁니다 — 중첩 런루프가 엔진 프레임을 멈춰 세웁니다.

### 걸리는 것

**⚠️ 언로드해도 실제로는 안 내려갑니다.** `dlclose` 는 0 을 돌려주는데 `dladdr` 로 보면 코드가
여전히 매핑돼 있고, 다시 `dlopen` 하면 **같은 핸들**이 옵니다. 순수 C 플러그인은 같은
시험에서 진짜로 언매핑됩니다 — 차이는 **Swift 런타임 자체**입니다(AppKit 을 뺀 빌드도
같았습니다. ObjC 때문이 아닙니다).

죽지는 않습니다. 다만 재로드해도 전역이 **이전 값 그대로**이고 `let` 전역의 초기화도 다시
돌지 않습니다(`swift_once` 는 이미 돌았습니다). 되돌릴 것은 `CAD_PluginUnload` 에서 손으로
되돌려야 합니다. .NET 이 아예 못 내려가는 것보다는 낫고, C++ 플러그인이 진짜로 내려가는
것보다는 못합니다.

**⚠️ 문자열 수명이 C++ 예제와 다릅니다.** Swift 의 `String` → `const char*` 브리징은 **호출
동안만 유효한 임시 포인터**입니다. C++ 예제는 전부 문자열 리터럴(정적 수명)이라 이 차이가
드러나지 않습니다. 엔진은 경계에서 전부 `std::string` 으로 복사하므로 지금은 안전하지만,
C API 를 새로 쓸 때 이 규약을 깨면 Swift/C# 플러그인이 먼저 죽습니다.

**⚠️ 콜백은 캡처를 못 합니다.** `@convention(c)` 함수 포인터에는 컨텍스트를 담을 자리가
없습니다. 상태는 전역이거나 `void* user` 에 실어 보냅니다 — 예제가
`Unmanaged.passRetained` / `release` 짝으로 보여줍니다.

**⚠️ 런타임 배포는 macOS 만 공짜입니다.** Swift 5 ABI 안정화 이후 macOS 는 런타임이 OS 에
들어 있어(`/usr/lib/swift`) 재배포가 0 입니다. Linux 는 `libswiftCore.so` 를 같이 깔거나
`-static-stdlib` 로 박아야 합니다.

## 안 되는 것

도킹 패널 내용은 ImGui 가 필요합니다(위 UI 종류 표 참조).
C# 이 **호스트**가 되어 엔진을 품는 것은 별개로 이미 됩니다
([샘플 레포](https://github.com/kimyuheon/Vulkan_CMake_Sample)의 WPF 예제).
