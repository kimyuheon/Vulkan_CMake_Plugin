using System.Runtime.InteropServices;

namespace WpfPlugin;

/// <summary>
/// 엔진 C API (VulkanCADCore.dll) 로 가는 P/Invoke 래퍼.
///
/// 문자열은 전부 UTF-8 이다. C# 의 기본 마셜링은 ANSI 라 한글이 깨지므로
/// <c>StringMarshalling = StringMarshalling.Utf8</c> 을 명시한다.
///
/// DLL 이름에 확장자를 안 쓰면 OS 마다 알아서 찾는다(Windows .dll / Linux lib*.so).
/// 여기선 Windows 전용이지만 습관을 맞춰 둔다.
/// </summary>
internal static partial class Cad
{
    private const string Dll = "VulkanCADCore";

    // ── 명령 등록 ──
    // fn 은 C 함수 포인터다. C# 에선 [UnmanagedCallersOnly] 정적 메서드의 주소를 넘긴다.
    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool CAD_RegisterCommand(
        string name, string title, nint fn, nint user, uint owner);

    // ── UI 등록 ──
    // kind: 0=메뉴항목 1=구분선 2=툴바버튼 3=리본버튼 4=패널
    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial uint CAD_AddUiItem(
        int kind, string path, string title, string command, string icon, uint owner);

    // ── 엔티티 속성(특성창) ──
    // kind: 0=실수 1=정수 2=참거짓 3=문자(읽기전용)
    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool CAD_AddEntityProperty(
        string owner, string type, string key, string label,
        int kind, float minV, float maxV, uint pluginId);

    [LibraryImport(Dll)]
    internal static partial void CAD_SetOnPluginEntityChanged(nint cb);

    // ── 형상 ──
    // normals 가 0 이면 엔진이 삼각형별 flat 법선을 만든다.
    [LibraryImport(Dll)]
    internal static partial uint CAD_CreateMesh(
        [In] float[] xyz, uint vertexCount, [In] uint[] indices, uint indexCount, nint normals);

    [LibraryImport(Dll)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool CAD_ReplaceMesh(
        uint id, [In] float[] xyz, uint vertexCount, [In] uint[] indices, uint indexCount, nint normals);

    [LibraryImport(Dll, StringMarshalling = StringMarshalling.Utf8)]
    [return: MarshalAs(UnmanagedType.I1)]
    internal static partial bool CAD_SetPluginTag(uint id, string owner, string type, string data);

    // ── 뷰 ──
    [LibraryImport(Dll)]
    internal static partial void CAD_RequestZoomExtents();
}
