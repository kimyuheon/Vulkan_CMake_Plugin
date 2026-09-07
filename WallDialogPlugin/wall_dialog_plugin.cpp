// WallDialogPlugin — 플러그인이 **자기 MFC 대화상자**를 띄우는 예제.
//
// ⭐ 이 예제가 보여주려는 것: 플러그인 UI 가 ImGui 에 묶이지 않는다.
//    · 메뉴·툴바·리본은 이름만 등록하면 엔진(또는 호스트)이 그린다 → ImGui 필요 없음
//    · **대화상자는 플러그인이 직접 띄운다** → MFC 든 Win32 든 Qt 든 자유
//    ImGui 가 필요한 건 "엔진 창 안에 도킹되는 패널" 하나뿐이다.
//
// 그래서 이 플러그인은 엔진 창이 GLFW 든 MFC 호스트든 상관없이 똑같이 동작한다.
//
// ⚠️ MFC 를 쓰는 DLL 의 규칙:
//    · CWinApp 파생 전역 객체가 하나 있어야 MFC 가 초기화된다(정규 DLL 방식)
//    · 밖에서 들어오는 함수마다 AFX_MANAGE_STATE 로 모듈 상태를 우리 것으로 바꿔야 한다.
//      안 하면 대화상자 리소스를 **엔진 모듈에서** 찾다가 못 찾고 실패한다.

#include "plugin/lot_plugin_sdk.h"
#include "api/VulkanCAD_API.h"

#include <afxwin.h>
#include <afxdlgs.h>

#include "resource.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

    unsigned int g_id = 0;
    const char*  kOwner = "WallDialogPlugin";

    struct Wall { double length = 2.0, thick = 0.2, height = 2.4; };

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

    // ── MFC 대화상자 ────────────────────────────────────────────────────
    class CWallDlg : public CDialog {
    public:
        explicit CWallDlg(CWnd* parent = nullptr) : CDialog(IDD_WALL, parent) {}
        double m_length = 2.0, m_thick = 0.2, m_height = 2.4;

    protected:
        void DoDataExchange(CDataExchange* pDX) override {
            CDialog::DoDataExchange(pDX);
            DDX_Text(pDX, IDC_EDIT_LENGTH, m_length);
            DDV_MinMaxDouble(pDX, m_length, 0.1, 100.0);
            DDX_Text(pDX, IDC_EDIT_THICK,  m_thick);
            DDV_MinMaxDouble(pDX, m_thick, 0.01, 5.0);
            DDX_Text(pDX, IDC_EDIT_HEIGHT, m_height);
            DDV_MinMaxDouble(pDX, m_height, 0.1, 50.0);
        }
    };

    // 마지막에 넣은 값을 기억한다 — 벽을 여러 개 세울 때 매번 다시 치지 않게.
    Wall g_last;

    void onWallDialog(void*) {
        // ⚠️ 엔진이 부른 함수다. 모듈 상태를 우리 것으로 바꿔야 IDD_WALL 을
        //    이 DLL 의 리소스에서 찾는다. 빠뜨리면 대화상자가 안 뜬다.
        AFX_MANAGE_STATE(AfxGetStaticModuleState());

        CWallDlg dlg;
        dlg.m_length = g_last.length;
        dlg.m_thick  = g_last.thick;
        dlg.m_height = g_last.height;
        if (dlg.DoModal() != IDOK) return;   // 취소 — 아무 일도 안 한다

        g_last.length = dlg.m_length;
        g_last.thick  = dlg.m_thick;
        g_last.height = dlg.m_height;

        std::vector<float> xyz; std::vector<unsigned int> idx;
        buildWallMesh(g_last, xyz, idx);
        const unsigned int id = CAD_CreateMesh(
            xyz.data(), static_cast<unsigned int>(xyz.size() / 3),
            idx.data(), static_cast<unsigned int>(idx.size()), nullptr);
        if (!id) return;

        // 딱지를 붙여 두면 저장/열기 뒤에도 벽으로 남고, 특성창에서 값을 고칠 수 있다.
        CAD_SetPluginTag(id, kOwner, "wall", encode(g_last).c_str());
        CAD_RequestZoomExtents();
    }

    // 특성창에서 값이 바뀌면 형상을 다시 만든다(재생성).
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

// MFC 정규 DLL — 이 전역 객체가 있어야 MFC 가 초기화된다.
class CWallDialogPluginApp : public CWinApp {
public:
    BOOL InitInstance() override { return CWinApp::InitInstance(); }
};
static CWallDialogPluginApp theApp;

extern "C" {

LOT_PLUGIN_ABI_EXPORT unsigned int CAD_PluginAbiVersion(void) {
    return LOT_PLUGIN_ABI_VERSION;
}

LOT_PLUGIN_ABI_EXPORT bool CAD_PluginLoad(unsigned int pluginId) {
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    g_id = pluginId;

    if (!CAD_RegisterCommand("walldlg", "벽 만들기(대화상자)", &onWallDialog, nullptr, g_id))
        return false;

    // UI 는 이름만 등록한다 — 여기엔 ImGui 도 MFC 도 안 쓴다.
    CAD_AddUiItem(0, "건축(MFC)", "벽 만들기...", "walldlg", "", g_id);
    CAD_AddUiItem(2, "건축(MFC)/벽체", "벽...", "walldlg", "", g_id);
    CAD_AddUiItem(3, "건축(MFC)/벽체", "벽...", "walldlg", "", g_id);

    // 특성창에서 값을 고칠 수 있게 — 이것도 ImGui 없이 된다.
    CAD_AddEntityProperty(kOwner, "wall", "length", "길이", 0, 0.1f, 100.0f, g_id);
    CAD_AddEntityProperty(kOwner, "wall", "thick",  "두께", 0, 0.01f, 5.0f,  g_id);
    CAD_AddEntityProperty(kOwner, "wall", "height", "높이", 0, 0.1f, 50.0f,  g_id);
    CAD_SetOnPluginEntityChanged(&onEntityChanged);

    std::printf("[WallDialogPlugin] 로드됨 (id=%u)\n", g_id);
    return true;
}

LOT_PLUGIN_ABI_EXPORT void CAD_PluginUnload(void) {
    // 콜백은 등록부와 달리 엔진이 안 지운다 — 직접 뗀다.
    CAD_SetOnPluginEntityChanged(nullptr);
    std::printf("[WallDialogPlugin] 언로드됨\n");
}

}  // extern "C"
