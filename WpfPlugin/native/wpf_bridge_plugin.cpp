// WpfBridgePlugin — .NET 런타임을 띄워 C#/WPF 플러그인을 불러들이는 **새시**.
//
// ⭐ 왜 새시가 필요한가:
//    NativeAOT 로 컴파일한 C# 은 네이티브 DLL 이 되어 엔진이 그냥 로드할 수 있다.
//    하지만 **NativeAOT 는 WPF 를 지원하지 않는다.** WPF 창을 띄우려면 온전한
//    .NET 런타임이 필요하고, 그걸 프로세스에 띄우는 것이 이 새시의 일이다.
//    AutoCAD 의 acmgd.dll 이 같은 자리에 있다.
//
//    엔진 입장에선 이것도 그냥 평범한 C++ 플러그인이다 — 엔진은 .NET 을 전혀 모른다.
//
// 흐름:
//    엔진 → CAD_PluginLoad → hostfxr 로 런타임 초기화 → WpfPlugin.dll 의 Entry.Load 호출
//                          → 그 안에서 C# 이 CAD_* 를 P/Invoke 로 부른다
//
// ⚠️ 언로드: .NET 런타임은 한 번 뜨면 프로세스가 끝날 때까지 못 내린다.
//    (AutoCAD 도 NETLOAD 한 어셈블리는 재시작해야 내려간다 — 같은 제약이다)
//    그래서 CAD_PluginUnload 는 관리 코드에 정리만 시키고 런타임은 그대로 둔다.

#include "plugin/lot_plugin_sdk.h"
#include "api/VulkanCAD_API.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ⚠️ nethost(get_hostfxr_path)는 쓰지 않는다. libnethost.lib 이 정적 CRT(/MT)로
//    빌드돼 있어 우리 /MD 와 링크가 안 맞고(LNK2038), nethost.dll 을 따로 배포하기도 번거롭다.
//    hostfxr.dll 위치는 규칙이 단순해서(dotnet/host/fxr/<버전>) 직접 찾는 편이 낫다.
#include <coreclr_delegates.h>
#include <hostfxr.h>

#include <algorithm>
#include <filesystem>
#include <vector>

#include <cstdio>
#include <string>

namespace {

    // hostfxr 함수 포인터들 — DLL 을 직접 로드해 받아온다.
    hostfxr_initialize_for_runtime_config_fn  g_initFn = nullptr;
    hostfxr_get_runtime_delegate_fn           g_getDelegateFn = nullptr;
    hostfxr_close_fn                          g_closeFn = nullptr;
    load_assembly_and_get_function_pointer_fn g_loadAssemblyFn = nullptr;

    // 관리 코드 진입점 — [UnmanagedCallersOnly] 라 시그니처가 그대로 노출된다.
    using ManagedLoadFn   = int  (CORECLR_DELEGATE_CALLTYPE*)(unsigned int pluginId);
    using ManagedUnloadFn = void (CORECLR_DELEGATE_CALLTYPE*)();
    ManagedUnloadFn g_managedUnload = nullptr;

    // 이 DLL 이 놓인 폴더 — 관리 어셈블리와 runtimeconfig 를 그 옆에서 찾는다.
    std::wstring pluginDir() {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&pluginDir), &self);
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(self, path, MAX_PATH);
        std::wstring s(path);
        const auto slash = s.find_last_of(L"\\/");
        return (slash == std::wstring::npos) ? L"." : s.substr(0, slash);
    }

    // dotnet 설치 위치 — 환경변수가 있으면 그것을, 없으면 표준 경로를 쓴다.
    std::filesystem::path dotnetRoot() {
        wchar_t buf[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"DOTNET_ROOT", buf, MAX_PATH) > 0) return buf;
        if (GetEnvironmentVariableW(L"ProgramW6432", buf, MAX_PATH) > 0)
            return std::filesystem::path(buf) / L"dotnet";
        return L"C:\\Program Files\\dotnet";
    }

    // dotnet/host/fxr/<버전>/hostfxr.dll — 설치된 것 중 가장 높은 버전을 쓴다.
    // (폴더 이름이 곧 버전이라 사전순 최대가 대체로 최신이다. 5.x 와 10.x 가 섞이면
    //  사전순이 어긋나지만, 그런 조합은 이 예제의 관심사가 아니다.)
    std::filesystem::path findHostfxr() {
        std::error_code ec;
        const auto fxrDir = dotnetRoot() / L"host" / L"fxr";
        std::vector<std::filesystem::path> versions;
        for (const auto& e : std::filesystem::directory_iterator(fxrDir, ec)) {
            if (ec) break;
            if (e.is_directory()) versions.push_back(e.path());
        }
        if (versions.empty()) return {};
        std::sort(versions.begin(), versions.end());
        return versions.back() / L"hostfxr.dll";
    }

    bool loadHostfxr() {
        const auto path = findHostfxr();
        if (path.empty()) {
            std::fprintf(stderr, "[WpfBridge] hostfxr 를 못 찾았습니다 — .NET 8 런타임이 필요합니다\n");
            return false;
        }
        HMODULE lib = LoadLibraryW(path.wstring().c_str());
        if (!lib) { std::fprintf(stderr, "[WpfBridge] hostfxr 로드 실패\n"); return false; }

        g_initFn = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            GetProcAddress(lib, "hostfxr_initialize_for_runtime_config"));
        g_getDelegateFn = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            GetProcAddress(lib, "hostfxr_get_runtime_delegate"));
        g_closeFn = reinterpret_cast<hostfxr_close_fn>(GetProcAddress(lib, "hostfxr_close"));
        return g_initFn && g_getDelegateFn && g_closeFn;
    }

    // runtimeconfig.json 을 읽어 런타임을 올리고, 어셈블리 로더를 받아온다.
    bool startRuntime(const std::wstring& configPath) {
        hostfxr_handle ctx = nullptr;
        if (g_initFn(configPath.c_str(), nullptr, &ctx) != 0 || !ctx) {
            std::fprintf(stderr, "[WpfBridge] 런타임 초기화 실패 — WpfPlugin.runtimeconfig.json 확인\n");
            if (ctx) g_closeFn(ctx);
            return false;
        }
        void* fn = nullptr;
        const int rc = g_getDelegateFn(ctx, hdt_load_assembly_and_get_function_pointer, &fn);
        // ⚠️ 델리게이트를 받은 뒤엔 컨텍스트를 닫아도 런타임은 살아 있다.
        //    (닫지 않으면 두 번째 초기화에서 걸린다)
        g_closeFn(ctx);
        if (rc != 0 || !fn) { std::fprintf(stderr, "[WpfBridge] 어셈블리 로더를 못 받았습니다\n"); return false; }
        g_loadAssemblyFn = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(fn);
        return true;
    }

    // 관리 어셈블리에서 [UnmanagedCallersOnly] 메서드 하나를 함수 포인터로 받는다.
    void* managedMethod(const std::wstring& asmPath, const wchar_t* typeName, const wchar_t* method) {
        void* fn = nullptr;
        const int rc = g_loadAssemblyFn(asmPath.c_str(), typeName, method,
                                        UNMANAGEDCALLERSONLY_METHOD, nullptr, &fn);
        if (rc != 0 || !fn) {
            std::fwprintf(stderr, L"[WpfBridge] %s.%s 를 못 찾았습니다 (rc=0x%x)\n", typeName, method, rc);
            return nullptr;
        }
        return fn;
    }

}  // namespace

extern "C" {

LOT_PLUGIN_ABI_EXPORT unsigned int CAD_PluginAbiVersion(void) {
    return LOT_PLUGIN_ABI_VERSION;
}

LOT_PLUGIN_ABI_EXPORT bool CAD_PluginLoad(unsigned int pluginId) {
    if (!loadHostfxr()) return false;

    const std::wstring dir = pluginDir();
    // ⚠️ 관리 어셈블리는 **하위 폴더**에 둔다. plugins/ 최상위에 두면 엔진 로더가
    //    그것도 네이티브 플러그인인 줄 알고 열어보다 "필수 심볼이 없습니다" 를 찍는다.
    //    (관리 쪽은 deps.json/runtimeconfig 등 딸린 파일도 많아 폴더로 묶는 게 낫다)
    const std::wstring managed = dir + L"\\WpfPlugin";
    const std::wstring cfg = managed + L"\\WpfPlugin.runtimeconfig.json";
    const std::wstring asmPath = managed + L"\\WpfPlugin.dll";
    if (!startRuntime(cfg)) return false;

    auto load = reinterpret_cast<ManagedLoadFn>(
        managedMethod(asmPath, L"WpfPlugin.Entry, WpfPlugin", L"Load"));
    if (!load) return false;
    g_managedUnload = reinterpret_cast<ManagedUnloadFn>(
        managedMethod(asmPath, L"WpfPlugin.Entry, WpfPlugin", L"Unload"));

    // 여기서부터는 C# 이 CAD_* 를 직접 부른다 — 새시는 더 관여하지 않는다.
    if (load(pluginId) == 0) {
        std::fprintf(stderr, "[WpfBridge] 관리 코드 Load 가 실패를 돌려줬습니다\n");
        return false;
    }
    std::printf("[WpfBridge] .NET 런타임 시작 + WpfPlugin 로드 (id=%u)\n", pluginId);
    return true;
}

LOT_PLUGIN_ABI_EXPORT void CAD_PluginUnload(void) {
    // 관리 코드에 정리만 시킨다. 런타임은 못 내린다(위 주석 참조).
    if (g_managedUnload) g_managedUnload();
    std::printf("[WpfBridge] 언로드 (런타임은 프로세스 종료까지 유지)\n");
}

}  // extern "C"
