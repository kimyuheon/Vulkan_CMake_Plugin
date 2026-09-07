// QtPlugin — 플러그인이 **자기 Qt 대화상자**를 띄우는 예제.
//
// ⭐ WallDialogPlugin(MFC)과 같은 일을 하지만 **3 OS 전부**에서 돈다.
//    엔진 UI 가 ImGui 든 MFC 호스트든 상관없다 — 대화상자는 플러그인이 직접 띄운다.
//
// ⭐ 어려운 곳은 대화상자가 아니라 **QApplication 부트스트랩**이다.
//    MFC 는 CWinApp 전역 하나면 끝났지만, Qt 는 상황이 둘로 갈린다:
//
//      · 엔진이 Qt 앱이 아님 (GLFW 창)  → 우리가 QApplication 을 만든다.
//        돌고 있는 이벤트 루프가 없으므로 exec() 로 **중첩 루프**를 돌린다.
//        그동안 엔진 프레임은 멈춘다 — MFC 의 DoModal() 과 같은 값이다.
//
//      · 호스트가 이미 Qt (CAD_AttachView 로 엔진을 품은 Qt 앱)
//        → QApplication 이 이미 있고 이벤트 루프도 돈다. 만들면 안 되고(둘째는 assert),
//          모달로 막을 이유도 없다 → **모덜리스**로 띄운다. 엔진은 계속 그린다.
//
//    같은 소스가 둘 다 처리한다. qApp 이 있는지만 보면 된다.

#include "plugin/lot_plugin_sdk.h"
#include "api/VulkanCAD_API.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QPointer>
#include <QString>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

    unsigned int g_id = 0;
    const char*  kOwner = "QtPlugin";

    // 엔진이 CAD_PluginLoad 를 부른 스레드. Qt 위젯은 이 스레드에서만 만질 수 있다.
    // (SwiftPlugin 이 Thread.isMainThread 로 확인하는 것과 같은 자리)
    std::thread::id g_engineThread;

    struct Wall { double length = 2.0, thick = 0.2, height = 2.4; };
    Wall g_last;   // 마지막 값 — 벽을 여러 개 세울 때 매번 다시 치지 않게

    std::string encode(const Wall& w) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "{\"length\":%g,\"thick\":%g,\"height\":%g}",
                      w.length, w.thick, w.height);
        return buf;
    }
    bool decode(const char* s, Wall& w) {
        if (!s) return false;
        return std::sscanf(s, "{\"length\":%lf,\"thick\":%lf,\"height\":%lf}",
                           &w.length, &w.thick, &w.height) == 3;
    }

    // 상자 8정점 12삼각형. 법선은 NULL 로 넘겨 엔진이 삼각형별 flat 법선을 만들게 한다.
    void buildWallMesh(const Wall& w, std::vector<float>& xyz, std::vector<unsigned int>& idx) {
        const float L = static_cast<float>(w.length);
        const float T = static_cast<float>(w.thick);
        const float H = static_cast<float>(w.height);
        const float v[8][3] = { {0,0,0},{L,0,0},{L,T,0},{0,T,0}, {0,0,H},{L,0,H},{L,T,H},{0,T,H} };
        xyz.assign(&v[0][0], &v[0][0] + 24);
        static const unsigned int tri[36] = {
            0,2,1, 0,3,2,   4,5,6, 4,6,7,
            0,1,5, 0,5,4,   1,2,6, 1,6,5,
            2,3,7, 2,7,6,   3,0,4, 3,4,7 };
        idx.assign(tri, tri + 36);
    }

    void createWall(const Wall& w) {
        std::vector<float> xyz; std::vector<unsigned int> idx;
        buildWallMesh(w, xyz, idx);
        const unsigned int id = CAD_CreateMesh(
            xyz.data(), static_cast<unsigned int>(xyz.size() / 3),
            idx.data(), static_cast<unsigned int>(idx.size()), nullptr);
        if (!id) return;
        // 딱지를 붙여 두면 저장/열기 뒤에도 벽으로 남고, 특성창에서 값을 고칠 수 있다.
        CAD_SetPluginTag(id, kOwner, "wall", encode(w).c_str());
        CAD_RequestZoomExtents();
    }

    // ── Qt 부트스트랩 ────────────────────────────────────────────────────
    //
    // ⚠️ argv 는 QApplication 보다 오래 살아야 한다 — Qt 는 이 포인터를 복사하지 않고
    //    그대로 들고 있다가 arguments() 등에서 다시 읽는다. 지역 배열로 주면 나중에
    //    죽은 스택을 읽는다. 그래서 정적 수명으로 둔다.
    int   g_argc     = 1;
    char  g_arg0[]   = "QtPlugin";
    char* g_argv[]   = { g_arg0, nullptr };

    // "이 QApplication 은 플러그인이 만든 것" 이라는 표식을 QApplication 객체 자체에 남긴다.
    //
    // ⚠️ 왜 지역 bool 로 충분하지 않은가: 언로드해도 QApplication 은 프로세스에 남는다
    //    (아래 CAD_PluginUnload 참조). 그 상태에서 플러그인을 **다시 로드**하면 새 인스턴스는
    //    qApp 이 있는 것만 보고 "호스트가 Qt 구나" 로 오판한다 → 돌지도 않는 이벤트 루프를
    //    믿고 모덜리스로 띄워 창이 그대로 얼어붙는다(실측으로 잡았다).
    //    표식은 QApplication 과 함께 남으므로 재로드해도 사실이 유지된다.
    const char* kOwnedProp = "VulkanCAD.QtPlugin.ownsApp";
    bool g_weOwnApp = false;   // 우리(또는 이전의 우리)가 만든 QApplication 인가

    // 이 플러그인 파일이 놓인 폴더. 아래에서 Qt 라이브러리 경로로 쓴다.
    QString thisModuleDir() {
#if defined(_WIN32)
        HMODULE h = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&thisModuleDir), &h))
            return QString();
        wchar_t buf[MAX_PATH] = {};
        if (!GetModuleFileNameW(h, buf, MAX_PATH)) return QString();
        return QFileInfo(QString::fromWCharArray(buf)).absolutePath();
#else
        Dl_info info{};
        if (!dladdr(reinterpret_cast<void*>(&thisModuleDir), &info) || !info.dli_fname)
            return QString();
        return QFileInfo(QString::fromUtf8(info.dli_fname)).absolutePath();
#endif
    }

    // 성공하면 qApp 이 있다. g_weOwnApp 이 참이면 **엔진은 Qt 앱이 아니다**.
    bool ensureQtApp() {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            g_weOwnApp = app->property(kOwnedProp).toBool();
            return true;   // 호스트가 Qt 면 그 위에 얹어 탄다
        }

        // ⚠️ Qt 는 플랫폼 플러그인(qwindows.dll / libqxcb.so)을 **실행 파일 옆**에서 찾는다.
        //    플러그인은 plugins/ 안에 있으므로, 우리 폴더도 후보에 넣어 준다.
        //    안 하면 "could not load the Qt platform plugin" 으로 그 자리에서 abort 한다 —
        //    라이브러리 경로는 QApplication 을 만들기 **전에** 넣어야 읽힌다.
        const QString dir = thisModuleDir();
        if (!dir.isEmpty()) QCoreApplication::addLibraryPath(dir);

        QApplication* app = new QApplication(g_argc, g_argv);
        app->setProperty(kOwnedProp, true);
        g_weOwnApp = true;
        return QCoreApplication::instance() != nullptr;
    }

    // ── 대화상자 ─────────────────────────────────────────────────────────
    //
    // Q_OBJECT 를 안 붙였다 — 자기 시그널/슬롯을 만들지 않고, 연결은 전부 새 문법
    // (함수 포인터·람다)이라 moc 이 필요 없다. 시그널을 하나라도 선언하면 그때
    // Q_OBJECT + AUTOMOC 이 필요해진다(CMakeLists 는 이미 켜 두었다).
    class WallDialog : public QDialog {
    public:
        explicit WallDialog(const Wall& init, QWidget* parent = nullptr) : QDialog(parent) {
            setWindowTitle(QStringLiteral("벽 만들기 (Qt)"));

            length_ = makeSpin(0.1,  100.0, init.length);
            thick_  = makeSpin(0.01,   5.0, init.thick);
            height_ = makeSpin(0.1,   50.0, init.height);

            auto* form = new QFormLayout(this);
            form->addRow(QStringLiteral("길이 (m)"), length_);
            form->addRow(QStringLiteral("두께 (m)"), thick_);
            form->addRow(QStringLiteral("높이 (m)"), height_);

            auto* buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
            form->addRow(buttons);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        }

        Wall value() const {
            Wall w;
            w.length = length_->value();
            w.thick  = thick_->value();
            w.height = height_->value();
            return w;
        }

    private:
        QDoubleSpinBox* makeSpin(double lo, double hi, double v) {
            auto* s = new QDoubleSpinBox(this);
            s->setRange(lo, hi);
            s->setDecimals(3);
            s->setSingleStep(0.1);
            s->setValue(v);
            return s;
        }

        QDoubleSpinBox* length_ = nullptr;
        QDoubleSpinBox* thick_  = nullptr;
        QDoubleSpinBox* height_ = nullptr;
    };

    // 모덜리스로 띄웠을 때의 그 창. QPointer 라 닫히면 저절로 null 이 된다.
    QPointer<WallDialog> g_modeless;

    // ── 명령 ─────────────────────────────────────────────────────────────
    void onWallDialog(void*) {
        // Qt 위젯은 GUI 스레드 전용이다. 명령 콜백은 엔진 스레드에서 오고 그게 GUI
        // 스레드지만, 가정을 말없이 믿지 않고 확인한다.
        if (std::this_thread::get_id() != g_engineThread) {
            std::printf("[QtPlugin] 엔진 스레드가 아닙니다 — 대화상자를 열지 않습니다\n");
            return;
        }
        if (!ensureQtApp()) {
            std::printf("[QtPlugin] QApplication 생성 실패\n");
            return;
        }

        if (g_weOwnApp) {
            // 우리가 Qt 를 띄웠다 = 엔진이 Qt 앱이 아니다. 돌고 있는 이벤트 루프가
            // 없으므로 exec() 가 중첩 루프를 연다. 닫을 때까지 엔진은 멈춘다.
            WallDialog dlg(g_last);
            if (dlg.exec() != QDialog::Accepted) return;   // 취소 — 아무 일도 안 한다
            g_last = dlg.value();
            createWall(g_last);
            return;
        }

        // 호스트가 Qt = 이벤트 루프가 이미 돈다. 막을 이유가 없으니 모덜리스로.
        if (g_modeless) {
            g_modeless->raise();
            g_modeless->activateWindow();
            return;
        }
        auto* dlg = new WallDialog(g_last);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dlg, &QDialog::accepted, dlg, [dlg]() {
            g_last = dlg->value();
            createWall(g_last);
        });
        g_modeless = dlg;
        dlg->show();
    }

    // 특성창에서 값이 바뀌면 형상을 다시 만든다(재생성). 여기엔 Qt 가 안 나온다 —
    // 값 편집은 엔진이 그리므로 대화상자와 무관하다.
    void onEntityChanged(unsigned int id, const char* owner, const char* type, const char* data) {
        if (std::strcmp(owner, kOwner) != 0 || std::strcmp(type, "wall") != 0) return;
        Wall w;
        if (!decode(data, w)) return;
        std::vector<float> xyz; std::vector<unsigned int> idx;
        buildWallMesh(w, xyz, idx);
        CAD_ReplaceMesh(id, xyz.data(), static_cast<unsigned int>(xyz.size() / 3),
                        idx.data(), static_cast<unsigned int>(idx.size()), nullptr);
    }

}  // namespace

extern "C" {

LOT_PLUGIN_ABI_EXPORT unsigned int CAD_PluginAbiVersion(void) {
    return LOT_PLUGIN_ABI_VERSION;
}

LOT_PLUGIN_ABI_EXPORT bool CAD_PluginLoad(unsigned int pluginId) {
    g_id = pluginId;
    g_engineThread = std::this_thread::get_id();

    // ⚠️ 여기서 QApplication 을 만들지 않는다. 로드는 엔진 시작 중에 일어나고,
    //    쓰지도 않을 Qt 를 그때 올리면 시작이 느려지고 플랫폼 플러그인이 없는
    //    환경에서는 **엔진 자체가** 못 뜬다. 명령을 처음 부를 때 만든다(지연 초기화).

    if (!CAD_RegisterCommand("qwall", "벽 만들기(Qt)", &onWallDialog, nullptr, g_id))
        return false;

    // UI 는 이름만 등록한다 — 여기엔 ImGui 도 Qt 도 안 쓴다.
    CAD_AddUiItem(0, "건축(Qt)", "벽 만들기...", "qwall", "", g_id);
    CAD_AddUiItem(2, "건축(Qt)/벽체", "벽...", "qwall", "", g_id);
    CAD_AddUiItem(3, "건축(Qt)/벽체", "벽...", "qwall", "", g_id);

    // 특성창에서 값을 고칠 수 있게 — 이것도 Qt 없이 된다.
    CAD_AddEntityProperty(kOwner, "wall", "length", "길이", 0, 0.1f, 100.0f, g_id);
    CAD_AddEntityProperty(kOwner, "wall", "thick",  "두께", 0, 0.01f, 5.0f,  g_id);
    CAD_AddEntityProperty(kOwner, "wall", "height", "높이", 0, 0.1f, 50.0f,  g_id);
    CAD_SetOnPluginEntityChanged(&onEntityChanged);

    std::printf("[QtPlugin] 로드됨 (id=%u, Qt %s)\n", g_id, qVersion());
    return true;
}

LOT_PLUGIN_ABI_EXPORT void CAD_PluginUnload(void) {
    // 콜백은 등록부와 달리 엔진이 안 지운다 — 직접 뗀다.
    CAD_SetOnPluginEntityChanged(nullptr);

    // ⚠️ 우리 위젯은 **반드시** 여기서 없앤다. 라이브러리가 내려간 뒤에 Qt 가
    //    이 창을 건드리면 사라진 코드(WallDialog 의 vtable)를 부른다.
    //
    // ⚠️ close() 로는 부족하다 — WA_DeleteOnClose 의 실제 삭제는 deleteLater 라
    //    **이벤트 루프가 한 번 더 돌아야** 일어난다. 언로드 직후 엔진은 dlclose 를
    //    부르므로 그 한 바퀴가 오지 않는다(실측: 언로드 뒤에도 창이 그대로 남았다).
    //    그래서 여기서는 바로 delete 한다. 대기 중인 deleteLater 가 있어도 QObject
    //    소멸자가 자기 앞으로 온 이벤트를 걷어가므로 이중 삭제가 되지 않는다.
    delete g_modeless.data();    // 이미 닫혔으면 QPointer 가 null 이라 무해하다
    g_modeless = nullptr;

    // ⚠️ QApplication 은 **일부러 delete 하지 않는다.** 한 프로세스에서
    //    한 번만 만드는 것이 안전하고(정적 등록·플랫폼 통합·QMetaType 이 되살아나지
    //    않는다), 지운 뒤 다시 만들면 그 자리에서 죽는 경우가 있다. .NET 런타임을
    //    못 내리는 것과 같은 종류의 제약이다.
    std::printf("[QtPlugin] 언로드됨\n");
}

}  // extern "C"
