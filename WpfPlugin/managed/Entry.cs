using System.Globalization;
using System.Runtime.InteropServices;
using System.Windows;

namespace WpfPlugin;

/// <summary>
/// 네이티브 새시(WpfBridgePlugin)가 부르는 진입점.
///
/// hostfxr 의 load_assembly_and_get_function_pointer 는 [UnmanagedCallersOnly] 로 표시된
/// 정적 메서드만 함수 포인터로 내준다. 인자·반환은 blittable 이어야 한다
/// (그래서 bool 대신 int, string 대신 nint 를 쓴다).
/// </summary>
public static class Entry
{
    private const string Owner = "WpfPlugin";
    private static uint _id;

    // 마지막에 넣은 값 — 벽을 여러 개 세울 때 매번 다시 치지 않게.
    private static double _length = 2.0, _thick = 0.2, _height = 2.4;

    [UnmanagedCallersOnly]
    public static int Load(uint pluginId)
    {
        try
        {
            _id = pluginId;

            unsafe
            {
                Cad.CAD_RegisterCommand("wallwpf", "벽 만들기(WPF)",
                    (nint)(delegate* unmanaged<nint, void>)&OnWallCommand, 0, _id);
                Cad.CAD_SetOnPluginEntityChanged(
                    (nint)(delegate* unmanaged<uint, nint, nint, nint, void>)&OnEntityChanged);
            }

            // UI 는 이름만 등록한다 — 여기엔 WPF 도 ImGui 도 안 쓴다.
            Cad.CAD_AddUiItem(0, "건축(WPF)", "벽 만들기...", "wallwpf", "", _id);
            Cad.CAD_AddUiItem(2, "건축(WPF)/벽체", "벽...", "wallwpf", "", _id);
            Cad.CAD_AddUiItem(3, "건축(WPF)/벽체", "벽...", "wallwpf", "", _id);

            // 특성창에서 값을 고칠 수 있게.
            Cad.CAD_AddEntityProperty(Owner, "wall", "length", "길이", 0, 0.1f, 100.0f, _id);
            Cad.CAD_AddEntityProperty(Owner, "wall", "thick", "두께", 0, 0.01f, 5.0f, _id);
            Cad.CAD_AddEntityProperty(Owner, "wall", "height", "높이", 0, 0.1f, 50.0f, _id);

            Console.WriteLine($"[WpfPlugin] 로드됨 (id={_id})");
            return 1;
        }
        catch (Exception e)
        {
            // 예외가 네이티브 경계를 넘으면 프로세스가 죽는다 — 여기서 삼킨다.
            Console.Error.WriteLine("[WpfPlugin] 로드 실패: " + e);
            return 0;
        }
    }

    [UnmanagedCallersOnly]
    public static void Unload()
    {
        try
        {
            Cad.CAD_SetOnPluginEntityChanged(0);   // 콜백은 엔진이 안 지운다 — 직접 뗀다
            Console.WriteLine("[WpfPlugin] 언로드됨");
        }
        catch { /* 언로드 중 예외는 삼킨다 */ }
    }

    // ── 명령: WPF 대화상자를 띄운다 ──────────────────────────────────────
    [UnmanagedCallersOnly]
    private static void OnWallCommand(nint user)
    {
        try
        {
            if (!ShowDialogOnStaThread()) return;   // 취소

            var (xyz, idx) = BuildWallMesh(_length, _thick, _height);
            uint id = Cad.CAD_CreateMesh(xyz, (uint)(xyz.Length / 3), idx, (uint)idx.Length, 0);
            if (id == 0) return;

            Cad.CAD_SetPluginTag(id, Owner, "wall", Encode(_length, _thick, _height));
            Cad.CAD_RequestZoomExtents();
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("[WpfPlugin] 명령 실패: " + e);
        }
    }

    /// <summary>
    /// ⚠️ WPF 창은 **STA 스레드**에서만 뜬다. 엔진의 주 스레드는 GLFW 루프라
    /// STA 도 아니고 Dispatcher 도 없다. 그래서 전용 STA 스레드를 만들어 거기서 띄우고,
    /// 닫힐 때까지 기다린다(모달처럼 동작).
    ///
    /// 이 방식이면 엔진 렌더 루프가 그동안 멈춘다 — 명령 콜백이 원래 그렇다
    /// (MFC 예제의 DoModal 도 같다).
    /// </summary>
    private static bool ShowDialogOnStaThread()
    {
        bool ok = false;
        var t = new Thread(() =>
        {
            try
            {
                var w = new WallWindow(_length, _thick, _height);
                if (w.ShowDialog() == true)
                {
                    _length = w.WallLength; _thick = w.WallThick; _height = w.WallHeight;
                    ok = true;
                }
                // 이 스레드의 Dispatcher 를 정리해야 다음 호출에서 새 창이 뜬다.
                System.Windows.Threading.Dispatcher.CurrentDispatcher.InvokeShutdown();
            }
            catch (Exception e)
            {
                Console.Error.WriteLine("[WpfPlugin] 대화상자 실패: " + e);
            }
        });
        t.SetApartmentState(ApartmentState.STA);
        t.Start();
        t.Join();
        return ok;
    }

    // ── 특성창에서 값이 바뀌면 형상을 다시 만든다 ────────────────────────
    [UnmanagedCallersOnly]
    private static void OnEntityChanged(uint id, nint ownerPtr, nint typePtr, nint dataPtr)
    {
        try
        {
            string owner = Marshal.PtrToStringUTF8(ownerPtr) ?? "";
            string type = Marshal.PtrToStringUTF8(typePtr) ?? "";
            if (owner != Owner || type != "wall") return;

            string data = Marshal.PtrToStringUTF8(dataPtr) ?? "";
            if (!Decode(data, out double l, out double t, out double h)) return;

            var (xyz, idx) = BuildWallMesh(l, t, h);
            Cad.CAD_ReplaceMesh(id, xyz, (uint)(xyz.Length / 3), idx, (uint)idx.Length, 0);
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("[WpfPlugin] 재생성 실패: " + e);
        }
    }

    // ── 정의 ↔ 문자열 ────────────────────────────────────────────────────
    // 딱지 data 는 JSON 이어야 한다 — 엔진이 특성창에서 이 JSON 을 읽고 쓴다.
    private static string Encode(double l, double t, double h) =>
        string.Format(CultureInfo.InvariantCulture,
                      "{{\"length\":{0},\"thick\":{1},\"height\":{2}}}", l, t, h);

    private static bool Decode(string s, out double l, out double t, out double h)
    {
        l = t = h = 0;
        try
        {
            using var doc = System.Text.Json.JsonDocument.Parse(s);
            var r = doc.RootElement;
            l = r.GetProperty("length").GetDouble();
            t = r.GetProperty("thick").GetDouble();
            h = r.GetProperty("height").GetDouble();
            return true;
        }
        catch { return false; }
    }

    // 상자 8정점 12삼각형. 법선은 넘기지 않는다 — 엔진이 삼각형별 flat 법선을 만든다.
    private static (float[] xyz, uint[] idx) BuildWallMesh(double length, double thick, double height)
    {
        float L = (float)length, T = (float)thick, H = (float)height;
        float[] xyz =
        {
            0,0,0,  L,0,0,  L,T,0,  0,T,0,
            0,0,H,  L,0,H,  L,T,H,  0,T,H,
        };
        uint[] idx =
        {
            0,2,1, 0,3,2,   4,5,6, 4,6,7,
            0,1,5, 0,5,4,   1,2,6, 1,6,5,
            2,3,7, 2,7,6,   3,0,4, 3,4,7,
        };
        return (xyz, idx);
    }
}
