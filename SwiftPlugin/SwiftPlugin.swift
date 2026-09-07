// SwiftPlugin — Swift 로 쓴 VulkanCAD 플러그인. HelloPlugin 과 같은 것을 보여 준다.
//
// C#/WPF 와 달리 **새시(shim)가 없다.** WpfPlugin 은 네이티브 DLL 이 hostfxr 로 .NET 런타임을
// 띄우고 관리 어셈블리를 불러들여야 했다. Swift 는 그 층이 통째로 없다:
//
//   @_cdecl("CAD_PluginLoad")  →  심볼이 C 이름 그대로 나간다 (맹글링 없음)
//   swiftc -emit-library       →  평범한 .dylib/.so
//
// 엔진은 이게 HelloPlugin.dylib 인지 SwiftPlugin.dylib 인지 구분하지 못한다. 계약이
// 순수 C(문자열·함수포인터)라서 Swift 의 clang 임포터가 엔진 헤더를 **래퍼 없이** 읽는다 —
// lot_plugin_sdk.h 가 "C++ 인터페이스가 아니라 C API 만 쓴다" 고 정한 결정의 배당금이다.
//
// 보여주는 것:
//   · 명령 등록      swall / swallthick / swalldlg
//   · 메뉴·툴바·리본  '건축(Swift)' 아래
//   · 커스텀 엔티티   벽 = 평범한 메시 + 딱지. 정의는 Codable 로 JSON 왕복
//   · 특성창          길이·두께·높이 슬라이더 → 형상 재생성
//   · 이벤트 구독     void* user 로 Swift 객체를 실어 보내는 방법 (Unmanaged)
//   · 자기 창         플러그인이 자기 Cocoa 창을 띄운다 (WallWindow.swift)

import Foundation
import VulkanCAD

// ── 전역 상태 ────────────────────────────────────────────────────────────────
// @convention(c) 콜백은 **캡처를 못 한다** — C 함수 포인터에는 컨텍스트를 담을 자리가
// 없기 때문이다. 그래서 상태는 전역이거나, 아래 Counter 처럼 void* user 로 실어 보낸다.
// (Swift 전역은 첫 접근 때 swift_once 로 초기화된다 — 로드 시점 비용이 없다)

var pluginId: UInt32 = 0
let owner = "SwiftPlugin"   // 딱지 owner — 파일 이름과 같게 둔다

// ── 벽 정의 ─────────────────────────────────────────────────────────────────
// 정의가 원본이고 메시는 그로부터 만든 결과물이다. C++ 예제는 JSON 파서를 안 들이려고
// snprintf/sscanf 로 흉내 냈지만, Swift 는 Codable 이 표준이라 그럴 이유가 없다.
//
// ⭐ 키 이름(length/thick/height)이 CAD_AddEntityProperty 에 등록한 key 와 같아야 한다 —
//    엔진이 이 JSON 을 직접 읽고 쓴다(nlohmann::json). Float 은 0.2 를 0.2 로 쓴다(실측).

struct Wall: Codable {
    var length: Float = 2.0
    var thick:  Float = 0.2
    var height: Float = 2.4
}

func encode(_ w: Wall) -> String {
    guard let data = try? JSONEncoder().encode(w) else { return "{}" }
    return String(decoding: data, as: UTF8.self)
}

func decode(_ s: String) -> Wall? {
    try? JSONDecoder().decode(Wall.self, from: Data(s.utf8))
}

// 상자 8정점 12삼각형. 법선은 nil 로 넘겨 엔진이 삼각형별 flat 법선을 만들게 한다.
func buildWallMesh(_ w: Wall) -> (xyz: [Float], idx: [UInt32]) {
    let L = w.length, T = w.thick, H = w.height
    let xyz: [Float] = [
        0, 0, 0,  L, 0, 0,  L, T, 0,  0, T, 0,
        0, 0, H,  L, 0, H,  L, T, H,  0, T, H,
    ]
    let idx: [UInt32] = [
        0, 2, 1,  0, 3, 2,   // 바닥
        4, 5, 6,  4, 6, 7,   // 천장
        0, 1, 5,  0, 5, 4,   // 앞
        1, 2, 6,  1, 6, 5,   // 오른쪽
        2, 3, 7,  2, 7, 6,   // 뒤
        3, 0, 4,  3, 4, 7,   // 왼쪽
    ]
    return (xyz, idx)
}

// 벽 하나를 만든다: 형상 → 딱지.
@discardableResult
func makeWall(_ w: Wall) -> UInt32 {
    let m = buildWallMesh(w)
    // Swift 배열은 호출 동안 UnsafePointer 로 자동 브리징된다 — withUnsafeBufferPointer 불필요.
    let id = CAD_CreateMesh(m.xyz, UInt32(m.xyz.count / 3), m.idx, UInt32(m.idx.count), nil)
    if id != 0 { CAD_SetPluginTag(id, owner, "wall", encode(w)) }
    return id
}

// ── 엔진에서 문자열 받아오기 ─────────────────────────────────────────────────
// CAD_Get* 계열은 "buf 가 NULL 이면 필요한 길이를 반환한다" 는 규약이다. 두 번 부르면
// 길이를 넉넉히 잡을 필요가 없다 — 고정 버퍼로 자르는 C++ 예제보다 이쪽이 정확하다.
func readString(_ get: (UnsafeMutablePointer<CChar>?, Int32) -> Int32) -> String {
    let need = get(nil, 0)
    guard need > 1 else { return "" }
    var buf = [CChar](repeating: 0, count: Int(need))
    _ = get(&buf, need)
    return String(cString: buf)
}

// 선택된 객체들 중 우리 벽만.
func selectedWalls() -> [(id: UInt32, wall: Wall)] {
    var out: [(UInt32, Wall)] = []
    for i in 0 ..< CAD_GetSelectedCount() {
        var id: UInt32 = 0
        guard CAD_GetSelectedObjectId(i, &id), CAD_HasPluginTag(id) else { continue }
        guard readString({ CAD_GetPluginTagType(id, $0, $1) }) == "wall" else { continue }
        guard let w = decode(readString({ CAD_GetPluginTagData(id, $0, $1) })) else { continue }
        out.append((id, w))
    }
    return out
}

// 정의를 고쳐 형상만 갈아 끼운다. 위치·색·딱지는 그대로다.
@discardableResult
func rebuild(_ id: UInt32, _ w: Wall) -> Bool {
    let m = buildWallMesh(w)
    guard CAD_ReplaceMesh(id, m.xyz, UInt32(m.xyz.count / 3), m.idx, UInt32(m.idx.count), nil)
    else { return false }
    CAD_SetPluginTag(id, owner, "wall", encode(w))
    return true
}

// ── 명령 ────────────────────────────────────────────────────────────────────
// 전역 함수는 캡처가 없으므로 Swift 가 @convention(c) 함수 포인터로 알아서 변환한다.
// 클로저를 쓰더라도 캡처만 없으면 된다.

func onWall(_ user: UnsafeMutableRawPointer?) {
    makeWall(Wall())
    CAD_RequestZoomExtents()
}

func onWallThick(_ user: UnsafeMutableRawPointer?) {
    var changed = 0
    for (id, var w) in selectedWalls() {
        w.thick *= 2.0
        if rebuild(id, w) { changed += 1 }
    }
    print("[SwiftPlugin] 두께 두 배: \(changed)개")
}

// ── 이벤트 구독 — void* user 로 Swift 객체를 실어 보내기 ──────────────────────
// 여기가 Swift 플러그인의 유일한 진짜 함정이다. 콜백이 캡처를 못 하므로 인스턴스를
// **직접 참조할 수 없다.** Unmanaged 로 참조를 하나 붙잡아(passRetained) 그 주소를
// user 에 싣고, 콜백에서 되찾고, 언로드 때 놓아 준다(release). 안 놓으면 샌다.

final class Counter {
    var created = 0
    var removed = 0
}

var counterRef: Unmanaged<Counter>?

func onEngineEvent(_ kind: Int32, _ id: UInt32, _ user: UnsafeMutableRawPointer?) {
    guard let user else { return }
    let c = Unmanaged<Counter>.fromOpaque(user).takeUnretainedValue()
    if kind == 1 {
        c.created += 1
        print("[SwiftPlugin] 객체 생성 id=\(id) (누적 \(c.created))")
    } else if kind == 2 {
        c.removed += 1
        print("[SwiftPlugin] 객체 삭제 id=\(id) (누적 \(c.removed))")
    }
}

// ── 엔티티 콜백 ──────────────────────────────────────────────────────────────

// 특성창에서 값이 바뀌었을 때 — 정의는 엔진이 이미 갱신했다. 형상만 다시 만들면 된다.
// 두께 슬라이더를 끌면 벽이 실제로 두꺼워지는 것이 이 경로다.
func onEntityChanged(_ id: UInt32,
                     _ owner_: UnsafePointer<CChar>?,
                     _ type: UnsafePointer<CChar>?,
                     _ data: UnsafePointer<CChar>?) {
    guard let owner_, let type, let data,
          String(cString: owner_) == owner, String(cString: type) == "wall",
          let w = decode(String(cString: data)) else { return }
    let m = buildWallMesh(w)
    CAD_ReplaceMesh(id, m.xyz, UInt32(m.xyz.count / 3), m.idx, UInt32(m.idx.count), nil)
}

// .lot 을 열어 우리 딱지가 붙은 객체가 복원됐을 때. 형상은 이미 엔진이 올렸고,
// 여기서 정의를 읽어 살아 있는 벽으로 넘겨받는다.
func onEntityLoaded(_ id: UInt32,
                    _ owner_: UnsafePointer<CChar>?,
                    _ type: UnsafePointer<CChar>?,
                    _ data: UnsafePointer<CChar>?) {
    guard let owner_, let data, String(cString: owner_) == owner,
          let w = decode(String(cString: data)) else { return }
    print("[SwiftPlugin] 복원: id=\(id) 길이=\(w.length) 두께=\(w.thick) 높이=\(w.height)")
}

// ── 엔진이 부르는 심볼 셋 ────────────────────────────────────────────────────
// @_cdecl 이 이름 맹글링을 끄고 C 심볼로 내보낸다. 엔진 로더가 dlsym 으로 찾는 그 이름이다.

@_cdecl("CAD_PluginAbiVersion")
public func pluginAbiVersion() -> UInt32 {
    // 헤더의 매크로가 Swift 상수로 그대로 넘어온다 — 숫자를 손으로 베끼지 않는다.
    LOT_PLUGIN_ABI_VERSION
}

@_cdecl("CAD_PluginLoad")
public func pluginLoad(_ id: UInt32) -> Bool {
    pluginId = id

    // ⚠️ 등록할 때 owner 에 엔진이 준 pluginId 를 넣어야 언로드 때 묶여서 지워진다.
    //
    // ⚠️ Swift 의 String → const char* 브리징은 **호출 동안만 유효한 임시 포인터**다.
    //    엔진이 이 문자열을 복사하지 않고 보관했다면 댕글링이 된다 — C++ 예제는 전부
    //    문자열 리터럴(정적 수명)이라 이 차이가 드러나지 않는다. 확인해 보면 엔진은
    //    경계에서 전부 std::string 으로 복사한다(RegisteredCommand::name, UiItem::path …).
    //    C API 를 새로 쓸 때 이 규약을 깨면 Swift/C# 플러그인이 먼저 죽는다.
    guard CAD_RegisterCommand("swall", "벽 만들기(Swift)", onWall, nil, id),
          CAD_RegisterCommand("swallthick", "벽 두께 x2(Swift)", onWallThick, nil, id)
    else { return false }

    CAD_AddUiItem(0, "건축(Swift)", "벽 만들기", "swall", "", id)
    CAD_AddUiItem(0, "건축(Swift)", "벽 두께 x2", "swallthick", "", id)

    CAD_AddUiItem(2, "건축(Swift)/벽체", "벽", "swall", "", id)
    CAD_AddUiItem(3, "건축(Swift)/벽체", "벽", "swall", "", id)
    CAD_AddUiItem(3, "건축(Swift)/벽체", "두께x2", "swallthick", "", id)

    // 자기 창 — macOS 에서만. 등록도 거기서 한다(WallWindow.swift).
    registerWindowCommand(id)

    // 특성창: 플러그인은 UI 를 그리지 않고 "무엇이 있는지" 만 말한다.
    // key 는 Wall 의 Codable 키와 같아야 엔진이 JSON 에서 찾는다.
    CAD_AddEntityProperty(owner, "wall", "length", "길이", 0, 0.1, 20.0, id)
    CAD_AddEntityProperty(owner, "wall", "thick", "두께", 0, 0.05, 2.0, id)
    CAD_AddEntityProperty(owner, "wall", "height", "높이", 0, 0.1, 10.0, id)

    CAD_SetOnPluginEntityLoaded(onEntityLoaded)
    CAD_SetOnPluginEntityChanged(onEntityChanged)

    // 목록형 구독을 쓴다 — CAD_SetOnObjectCreated 같은 단일 슬롯은 호스트 것을 덮어쓴다.
    let counter = Unmanaged.passRetained(Counter())
    counterRef = counter
    CAD_AddEventListener(1 | 2, onEngineEvent, counter.toOpaque(), id)

    print("[SwiftPlugin] 로드됨 (id=\(id), Swift \(swiftVersionString()))")
    return true
}

@_cdecl("CAD_PluginUnload")
public func pluginUnload() {
    // 명령·UI·속성·이벤트는 엔진이 pluginId 로 묶어 지운다. 여기서는 우리 것만 치운다.
    CAD_SetOnPluginEntityLoaded(nil)
    CAD_SetOnPluginEntityChanged(nil)

    closeWindowIfOpen()

    // 콜백에 실어 보낸 참조를 놓아 준다 — passRetained 의 짝. 안 놓으면 샌다.
    counterRef?.release()
    counterRef = nil

    // ⚠️ 전역을 **손으로** 되돌린다. 여기가 C++ 플러그인과 갈리는 지점이다.
    //    macOS 에서 Swift 가 들어간 이미지는 dlclose 해도 실제로는 안 내려간다(실측:
    //    dlclose 는 0 을 돌려주지만 dladdr 로 코드가 여전히 매핑돼 있고, 다시 dlopen 하면
    //    **같은 핸들**이 온다). 순수 C 플러그인은 같은 시험에서 진짜로 언매핑된다.
    //    AppKit/ObjC 때문이 아니라 Swift 런타임 자체 때문이다 — AppKit 을 뺀 빌드도 같다.
    //
    //    죽지는 않는다. 다만 재로드해도 전역이 **이전 값 그대로**이고 let 전역의 초기화도
    //    다시 안 돈다(swift_once 는 이미 돌았다). 그러니 "언로드하면 깨끗해진다" 에
    //    기대면 안 되고, 되돌릴 것은 여기서 되돌려야 한다.
    pluginId = 0

    print("[SwiftPlugin] 언로드됨")
}

func swiftVersionString() -> String {
    #if swift(>=6.0)
        return "6.x"
    #else
        return "5.x"
    #endif
}
