// 플러그인이 자기 창을 띄운다 — WallDialogPlugin(MFC) / WpfPlugin(WPF) 의 macOS 판.
//
// ⭐ 여기가 WPF 보다 훨씬 간단하다. WPF 는 엔진 주 스레드가 STA 도 아니고 Dispatcher 도
//    없어서 **전용 STA 스레드**를 파고 거기서 창을 띄운 뒤 닫힐 때까지 기다려야 했다.
//    macOS 에는 그 우회가 아예 필요 없다:
//
//    엔진의 프레임 루프는 glfwPollEvents 를 돈다. GLFW 의 macOS 구현은
//
//        while (event = [NSApp nextEventMatchingMask:NSEventMaskAny ... dequeue:YES])
//            [NSApp sendEvent:event];
//
//    즉 **프로세스의 NSApp 이벤트 큐를 통째로 비우고 각 창으로 라우팅한다.** 엔진 창만이
//    아니라 그 프로세스의 모든 NSWindow 가 대상이다. 그래서 플러그인이 만든 창은
//    엔진 루프를 그냥 얻어 탄다 — 스레드도, 모달 루프도, 펌프도 우리가 안 만든다.
//
// ⚠️ 대신 지켜야 할 것: AppKit 은 **주 스레드 전용**이다. 명령 콜백은 엔진과 같은
//    스레드에서 불리고 macOS 에서 그건 주 스레드다(GLFW 가 그렇게 요구한다). 그래서
//    직접 만들어도 되지만, 가정이 깨지면 조용히 어긋나므로 아래에서 명시적으로 확인한다.
//
// ⚠️ 모달(NSApp.runModal)은 일부러 안 쓴다. 모달은 중첩 런루프를 돌려 엔진 프레임 루프를
//    멈춰 세운다 — MFC 의 DoModal 이 그랬듯이. 모드리스면 벽을 만들면서 뷰를 계속 돌릴 수
//    있고, 이게 CAD 도구에 맞는 모양이다.

#if canImport(AppKit)

import AppKit
import VulkanCAD

final class WallWindowController: NSObject, NSWindowDelegate {

    static var shared: WallWindowController?

    private let window: NSWindow
    private var wall = Wall()
    private let lengthSlider = NSSlider()
    private let thickSlider = NSSlider()
    private let heightSlider = NSSlider()
    private let readout = NSTextField(labelWithString: "")

    override init() {
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 260, height: 190),
                          styleMask: [.titled, .closable, .utilityWindow],
                          backing: .buffered,
                          defer: false)
        super.init()

        window.title = "벽 만들기 (Swift)"
        window.isReleasedWhenClosed = false   // 우리가 shared 로 붙잡고 있다
        window.delegate = self
        window.level = .floating              // 엔진 창 위에 남는다

        let stack = NSStackView()
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 6
        stack.edgeInsets = NSEdgeInsets(top: 12, left: 12, bottom: 12, right: 12)
        stack.translatesAutoresizingMaskIntoConstraints = false

        stack.addArrangedSubview(row("길이", lengthSlider, 0.1, 20.0, Double(wall.length)))
        stack.addArrangedSubview(row("두께", thickSlider, 0.05, 2.0, Double(wall.thick)))
        stack.addArrangedSubview(row("높이", heightSlider, 0.1, 10.0, Double(wall.height)))

        readout.font = .monospacedDigitSystemFont(ofSize: 11, weight: .regular)
        readout.textColor = .secondaryLabelColor
        stack.addArrangedSubview(readout)

        let make = NSButton(title: "벽 만들기", target: self, action: #selector(onMake))
        make.keyEquivalent = "\r"
        stack.addArrangedSubview(make)

        window.contentView = stack
        NSLayoutConstraint.activate([
            stack.widthAnchor.constraint(greaterThanOrEqualToConstant: 236),
        ])
        window.center()
        updateReadout()
    }

    private func row(_ title: String, _ slider: NSSlider,
                     _ minV: Double, _ maxV: Double, _ value: Double) -> NSView {
        slider.minValue = minV
        slider.maxValue = maxV
        slider.doubleValue = value
        slider.target = self
        slider.action = #selector(onSlide)
        slider.isContinuous = true

        let label = NSTextField(labelWithString: title)
        label.widthAnchor.constraint(equalToConstant: 36).isActive = true

        let h = NSStackView(views: [label, slider])
        h.orientation = .horizontal
        slider.widthAnchor.constraint(equalToConstant: 180).isActive = true
        return h
    }

    @objc private func onSlide() {
        wall.length = Float(lengthSlider.doubleValue)
        wall.thick = Float(thickSlider.doubleValue)
        wall.height = Float(heightSlider.doubleValue)
        updateReadout()

        // 선택된 우리 벽이 있으면 끌면서 바로 반영한다. 특성창 슬라이더와 같은 경로다 —
        // 정의를 고치고 형상을 다시 만든다.
        for (id, _) in selectedWalls() { rebuild(id, wall) }
    }

    @objc private func onMake() {
        // ⭐ AppKit 콜백에서 CAD_* 를 바로 부른다. 이 콜백은 [NSApp sendEvent:] 안에서
        //    불리고, 그건 엔진 루프의 glfwPollEvents 안이다 — 즉 **엔진 스레드**다.
        //    별도 마샬링이 필요 없는 이유이자, 여기서 스레드를 옮기면 안 되는 이유다.
        makeWall(wall)
        CAD_RequestZoomExtents()
    }

    private func updateReadout() {
        readout.stringValue = String(format: "길이 %.2f · 두께 %.2f · 높이 %.2f",
                                     wall.length, wall.thick, wall.height)
    }

    func show() {
        window.makeKeyAndOrderFront(nil)
    }

    func close() {
        window.delegate = nil
        window.close()
    }

    func windowWillClose(_ notification: Notification) {
        WallWindowController.shared = nil   // 다음에 명령을 부르면 새로 만든다
    }
}

private func onWallDialog(_ user: UnsafeMutableRawPointer?) {
    // AppKit 은 주 스레드 전용이다. 엔진 콜백이 주 스레드라는 가정이 깨지면 여기서 잡는다.
    guard Thread.isMainThread else {
        print("[SwiftPlugin] 창을 열 수 없습니다 — 주 스레드가 아닙니다")
        return
    }
    if WallWindowController.shared == nil {
        WallWindowController.shared = WallWindowController()
    }
    WallWindowController.shared?.show()
}

func registerWindowCommand(_ id: UInt32) {
    CAD_RegisterCommand("swalldlg", "벽 대화상자(Swift)", onWallDialog, nil, id)
    CAD_AddUiItem(0, "건축(Swift)", "벽 대화상자…", "swalldlg", "", id)
    CAD_AddUiItem(3, "건축(Swift)/벽체", "대화상자", "swalldlg", "", id)
}

func closeWindowIfOpen() {
    // 언로드 뒤에도 창이 남아 있으면, 버튼을 누르는 순간 사라진 코드로 뛴다.
    WallWindowController.shared?.close()
    WallWindowController.shared = nil
}

#else

// AppKit 이 없는 곳(Linux) — 나머지는 그대로 동작한다.
func registerWindowCommand(_ id: UInt32) {}
func closeWindowIfOpen() {}

#endif
