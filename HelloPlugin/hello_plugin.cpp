// HelloPlugin — VulkanCAD 플러그인 최소 예제.
//
// 이 파일 하나가 플러그인의 전부다. 엔진 소스는 필요 없고 헤더 둘만 참조한다:
//   plugin/lot_plugin_sdk.h   내보낼 심볼 셋과 ABI 번호
//   api/VulkanCAD_API.h       엔진에게 시킬 것들 (명령 200개 이상이 여기로 열린다)
//
// 하는 일:
//   · 명령 두 개를 등록한다 — wall(상자 하나), tower(상자 다섯 개를 쌓는다)
//   · 메뉴바에 '건축' 메뉴를 올린다
//   · 툴바 버튼도 하나 올린다 (등록만 — 렌더링은 엔진 몫)
//
// ⚠️ 등록할 때 owner 에 **엔진이 준 pluginId** 를 넣어야 한다. 그래야 언로드할 때
//    엔진이 owner 로 묶어 한꺼번에 지운다. 안 그러면 이 DLL 이 내려간 뒤에도
//    등록이 남아, 사라진 코드를 부르며 죽는다.

#include "plugin/lot_plugin_sdk.h"
#include "api/VulkanCAD_API.h"

#include <cstdio>

namespace {

    // 엔진이 발급한 우리 ID. 등록할 때 owner 로 쓴다.
    unsigned int g_id = 0;

    // 명령 콜백은 C 함수 포인터다. user 는 등록할 때 준 값이 그대로 온다.
    void onWall(void* /*user*/) {
        // 엔진 명령을 그대로 부른다 — 명령행에 치는 것과 같다.
        CAD_CreateBox(0.0f, 0.0f, 0.0f, 2.0f, 0.3f, 1.0f);
        CAD_RequestZoomExtents();
    }

    void onTower(void* /*user*/) {
        // 여러 개를 쌓는다. 좌표만 바꿔 같은 API 를 반복 호출.
        for (int i = 0; i < 5; ++i) {
            CAD_CreateBox(0.0f, 0.0f, static_cast<float>(i) * 1.1f, 1.0f, 1.0f, 1.0f);
        }
        CAD_RequestZoomExtents();
    }

}  // namespace

extern "C" {

// 엔진이 제일 먼저 부른다. 이 값이 엔진과 다르면 로드 자체를 안 한다.
LOT_PLUGIN_ABI_EXPORT unsigned int CAD_PluginAbiVersion(void) {
    return LOT_PLUGIN_ABI_VERSION;
}

// 등록은 전부 여기서. false 를 돌려주면 엔진이 로드를 취소한다.
LOT_PLUGIN_ABI_EXPORT bool CAD_PluginLoad(unsigned int pluginId) {
    g_id = pluginId;

    // ── 명령 ── 명령행에 이름을 치면 실행된다.
    if (!CAD_RegisterCommand("wall",  "벽 만들기", &onWall,  nullptr, g_id)) return false;
    if (!CAD_RegisterCommand("tower", "탑 쌓기",   &onTower, nullptr, g_id)) return false;

    // ── 메뉴 ── kind 0 = 메뉴 항목, 1 = 구분선. 경로 '/' 로 하위 메뉴.
    CAD_AddUiItem(0, "건축", "벽 만들기", "wall",  "", g_id);
    CAD_AddUiItem(0, "건축", "탑 쌓기",   "tower", "", g_id);
    CAD_AddUiItem(1, "건축", "",          "",      "", g_id);
    CAD_AddUiItem(0, "건축/도구", "전체 보기", "zoom", "", g_id);

    // ── 툴바 ── kind 2 = 툴바 버튼. 경로는 "패널/그룹".
    CAD_AddUiItem(2, "건축/벽체", "벽", "wall", "", g_id);

    std::printf("[HelloPlugin] 로드됨 (id=%u)\n", g_id);
    return true;
}

// 자기 정리만 하면 된다. 등록물은 엔진이 owner 로 묶어 지운다 —
// 여기서 일일이 지울 필요가 없고, 빠뜨려도 새지 않는다.
LOT_PLUGIN_ABI_EXPORT void CAD_PluginUnload(void) {
    std::printf("[HelloPlugin] 언로드됨\n");
}

}  // extern "C"
