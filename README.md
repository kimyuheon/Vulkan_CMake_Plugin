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

## 설치

빌드된 파일을 엔진 실행 파일 옆 `plugins/` 에 넣습니다. 엔진이 시작할 때 자동으로 훑습니다.

```
VulkanApp.exe
plugins/
  HelloPlugin.dll      ← Windows
  HelloPlugin.so       ← Linux
  HelloPlugin.dylib    ← macOS
```

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
