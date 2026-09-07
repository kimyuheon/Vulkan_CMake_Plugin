// Swift 에게 엔진 헤더를 보여 주기 위한 껍데기. 내용은 include 두 줄이 전부다.
//
// ⚠️ 순서가 중요하다. plugin/lot_plugin_sdk.h 는 typedef 에 bool 을 쓰면서 <stdbool.h> 를
//    안 들인다 — C++ 로 include 될 때만 성립하는 가정이다. Swift 의 임포터는 헤더를 **C 로**
//    읽으므로 stdbool 을 먼저 깔아 줘야 한다. (C++ 예제에선 드러나지 않던 차이다)
//
// 헤더 둘을 modulemap 에 직접 나열하지 않고 이 파일 하나로 묶는 이유도 같다 —
// 모듈 안에서의 헤더 처리 순서는 보장되지 않으므로, 순서를 여기서 못 박는다.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "api/VulkanCAD_API.h"     // 엔진에게 시킬 것들
#include "plugin/lot_plugin_sdk.h" // 내보낼 심볼 셋과 ABI 번호
