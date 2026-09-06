// HelloPlugin — VulkanCAD 플러그인 최소 예제.
//
// 이 파일 하나가 플러그인의 전부다. 엔진 소스는 필요 없고 헤더 둘만 참조한다:
//   plugin/lot_plugin_sdk.h   내보낼 심볼 셋과 ABI 번호
//   api/VulkanCAD_API.h       엔진에게 시킬 것들 (명령 200개 이상이 여기로 열린다)
//
// 보여주는 것:
//   · 명령 등록      wall / tower / wallthick
//   · 메뉴·툴바 등록  '건축' 메뉴, '건축' 툴바 패널
//   · 커스텀 엔티티   벽 = 엔진에는 평범한 메시 + 플러그인 딱지(두께·높이·길이)
//                    → 저장/열기 뒤에도 딱지가 살아 있고, 값을 바꾸면 형상을 다시 만든다
//
// ⚠️ 등록할 때 owner 에 **엔진이 준 pluginId** 를 넣어야 한다. 그래야 언로드할 때
//    엔진이 owner 로 묶어 한꺼번에 지운다.

#include "plugin/lot_plugin_sdk.h"
#include "api/VulkanCAD_API.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

    unsigned int g_id = 0;          // 엔진이 발급한 우리 ID
    const char*  kOwner = "HelloPlugin";   // 딱지 owner — 파일 이름과 같게 둔다

    // ── 벽 정의 → 형상 ──────────────────────────────────────────────────
    // 정의(길이·두께·높이)가 원본이고 메시는 그로부터 만든 결과물이다.
    struct Wall { float length = 2.0f, thick = 0.2f, height = 2.4f; };

    // 정의를 딱지 문자열로. JSON 파서를 안 들이려고 아주 단순한 형식을 쓴다.
    std::string encode(const Wall& w) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "{\"length\":%g,\"thick\":%g,\"height\":%g}", w.length, w.thick, w.height);
        return buf;
    }
    bool decode(const char* s, Wall& w) {
        if (!s) return false;
        return std::sscanf(s, "{\"length\":%f,\"thick\":%f,\"height\":%f}", &w.length, &w.thick, &w.height) == 3;
    }

    // 상자 8정점 12삼각형. 법선은 NULL 로 넘겨 엔진이 삼각형별 flat 법선을 만들게 한다.
    void buildWallMesh(const Wall& w, std::vector<float>& xyz, std::vector<unsigned int>& idx) {
        const float L = w.length, T = w.thick, H = w.height;
        const float v[8][3] = { {0,0,0},{L,0,0},{L,T,0},{0,T,0}, {0,0,H},{L,0,H},{L,T,H},{0,T,H} };
        xyz.assign(&v[0][0], &v[0][0] + 24);
        static const unsigned int tri[36] = {
            0,2,1, 0,3,2,   // 바닥
            4,5,6, 4,6,7,   // 천장
            0,1,5, 0,5,4,   // 앞
            1,2,6, 1,6,5,   // 오른쪽
            2,3,7, 2,7,6,   // 뒤
            3,0,4, 3,4,7 }; // 왼쪽
        idx.assign(tri, tri + 36);
    }

    // 벽 하나를 만든다: 형상 → 딱지.
    unsigned int makeWall(const Wall& w) {
        std::vector<float> xyz; std::vector<unsigned int> idx;
        buildWallMesh(w, xyz, idx);
        const unsigned int id = CAD_CreateMesh(xyz.data(), static_cast<unsigned int>(xyz.size() / 3),
                                               idx.data(), static_cast<unsigned int>(idx.size()), nullptr);
        if (id) CAD_SetPluginTag(id, kOwner, "wall", encode(w).c_str());
        return id;
    }

    // ── 명령 ──────────────────────────────────────────────────────────────
    void onWall(void*) {
        makeWall(Wall{});
        CAD_RequestZoomExtents();
    }

    void onTower(void*) {
        for (int i = 0; i < 5; ++i)
            CAD_CreateBox(0.0f, 0.0f, static_cast<float>(i) * 1.1f, 1.0f, 1.0f, 1.0f);
        CAD_RequestZoomExtents();
    }

    // 선택된 벽들의 두께를 두 배로 — **재생성** 경로. 딱지의 정의를 바꾸고 형상을 다시 만든다.
    // 엔진 입장에선 메시 교체일 뿐이고, 딱지·위치·색은 그대로다.
    void onWallThick(void*) {
        const unsigned int n = CAD_GetSelectedCount();
        std::vector<unsigned int> ids;
        for (unsigned int i = 0; i < n; ++i) { unsigned int sid = 0; if (CAD_GetSelectedObjectId(i, &sid)) ids.push_back(sid); }
        int changed = 0;
        for (unsigned int id : ids) {
            char type[32] = {}, data[128] = {};
            if (!CAD_HasPluginTag(id)) continue;
            CAD_GetPluginTagType(id, type, sizeof(type));
            if (std::strcmp(type, "wall") != 0) continue;
            CAD_GetPluginTagData(id, data, sizeof(data));
            Wall w; if (!decode(data, w)) continue;
            w.thick *= 2.0f;
            std::vector<float> xyz; std::vector<unsigned int> idx;
            buildWallMesh(w, xyz, idx);
            if (CAD_ReplaceMesh(id, xyz.data(), static_cast<unsigned int>(xyz.size() / 3),
                                idx.data(), static_cast<unsigned int>(idx.size()), nullptr)) {
                CAD_SetPluginTag(id, kOwner, "wall", encode(w).c_str());
                ++changed;
            }
        }
        std::printf("[HelloPlugin] 두께 두 배: %d개\n", changed);
    }

    // 특성창에서 값이 바뀌었을 때 — 정의는 엔진이 이미 갱신했고, 형상만 다시 만들면 된다.
    // 이게 "커스텀 엔티티" 의 핵심이다: 두께 슬라이더를 끌면 벽이 실제로 두꺼워진다.
    void onEntityChanged(unsigned int id, const char* owner, const char* type, const char* data) {
        if (std::strcmp(owner, kOwner) != 0 || std::strcmp(type, "wall") != 0) return;
        Wall w;
        if (!decode(data, w)) return;
        std::vector<float> xyz; std::vector<unsigned int> idx;
        buildWallMesh(w, xyz, idx);
        CAD_ReplaceMesh(id, xyz.data(), static_cast<unsigned int>(xyz.size() / 3),
                        idx.data(), static_cast<unsigned int>(idx.size()), nullptr);
    }

    // 엔진 이벤트 — 여러 플러그인이 동시에 구독할 수 있는 목록형이다.
    // (CAD_SetOnObjectCreated 같은 단일 슬롯은 호스트 것을 덮어써서 플러그인이 쓰면 안 된다)
    void onEngineEvent(int kind, unsigned int id, void*) {
        if (kind == 1) std::printf("[HelloPlugin] 객체 생성 id=%u\n", id);
        else if (kind == 2) std::printf("[HelloPlugin] 객체 삭제 id=%u\n", id);
    }

    // .lot 을 열어 우리 딱지가 붙은 객체가 복원됐을 때. 형상은 이미 엔진이 올렸다 —
    // 여기서는 정의를 읽어 "살아 있는 벽" 으로 넘겨받는다(지금은 확인만 찍는다).
    void onEntityLoaded(unsigned int id, const char* owner, const char* type, const char* data) {
        if (std::strcmp(owner, kOwner) != 0) return;   // 남의 것
        Wall w;
        if (decode(data, w))
            std::printf("[HelloPlugin] 복원: id=%u %s 길이=%g 두께=%g 높이=%g\n", id, type, w.length, w.thick, w.height);
    }

}  // namespace

extern "C" {

LOT_PLUGIN_ABI_EXPORT unsigned int CAD_PluginAbiVersion(void) {
    return LOT_PLUGIN_ABI_VERSION;
}

LOT_PLUGIN_ABI_EXPORT bool CAD_PluginLoad(unsigned int pluginId) {
    g_id = pluginId;

    if (!CAD_RegisterCommand("wall",      "벽 만들기",   &onWall,      nullptr, g_id)) return false;
    if (!CAD_RegisterCommand("tower",     "탑 쌓기",     &onTower,     nullptr, g_id)) return false;
    if (!CAD_RegisterCommand("wallthick", "벽 두께 x2",  &onWallThick, nullptr, g_id)) return false;

    CAD_AddUiItem(0, "건축", "벽 만들기",  "wall",      "", g_id);
    CAD_AddUiItem(0, "건축", "벽 두께 x2", "wallthick", "", g_id);
    CAD_AddUiItem(0, "건축", "탑 쌓기",    "tower",     "", g_id);
    CAD_AddUiItem(1, "건축", "",           "",          "", g_id);
    CAD_AddUiItem(0, "건축/도구", "전체 보기", "zoom", "", g_id);

    CAD_AddUiItem(2, "건축/벽체", "벽",     "wall",      "", g_id);
    CAD_AddUiItem(2, "건축/벽체", "두께x2", "wallthick", "", g_id);

    // 특성창에 벽의 값을 노출한다 — 플러그인은 UI 를 그리지 않고 "무엇이 있는지" 만 말한다.
    // 값은 딱지 data 의 JSON 키와 같은 이름이어야 엔진이 찾는다.
    CAD_AddEntityProperty(kOwner, "wall", "length", "길이", 0, 0.1f, 20.0f, g_id);
    CAD_AddEntityProperty(kOwner, "wall", "thick",  "두께", 0, 0.05f, 2.0f,  g_id);
    CAD_AddEntityProperty(kOwner, "wall", "height", "높이", 0, 0.1f, 10.0f,  g_id);

    // 열기 알림 — 우리 엔티티가 복원되면 정의를 돌려받는다.
    CAD_SetOnPluginEntityLoaded(&onEntityLoaded);
    // 값이 바뀌면 형상을 다시 만든다.
    CAD_SetOnPluginEntityChanged(&onEntityChanged);
    // 객체 생성/삭제 구독 (1|2). 언로드 때는 엔진이 pluginId 로 묶어 알아서 지운다.
    CAD_AddEventListener(1 | 2, &onEngineEvent, nullptr, g_id);

    std::printf("[HelloPlugin] 로드됨 (id=%u)\n", g_id);
    return true;
}

LOT_PLUGIN_ABI_EXPORT void CAD_PluginUnload(void) {
    // 콜백은 등록부와 달리 엔진이 안 지운다 — 직접 뗀다.
    // (이벤트 리스너·속성·명령·UI 는 pluginId 로 묶여 엔진이 지운다)
    CAD_SetOnPluginEntityLoaded(nullptr);
    CAD_SetOnPluginEntityChanged(nullptr);
    std::printf("[HelloPlugin] 언로드됨\n");
}

}  // extern "C"
