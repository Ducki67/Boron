#include "pch.h"
#include "../Public/GUI.h"
#include "../../FortniteGame/Public/BattleRoyaleGamePhaseLogic.h"
#include "../../FortniteGame/Public/BuildingSMActor.h"
#include "../../Engine/Public/NetDriver.h"
#include "../../ImGui/imgui.h"
#include "../../ImGui/imgui_internal.h"
#include "../../ImGui/imgui_impl_dx11.h"
#include "../../ImGui/imgui_impl_win32.h"
#include "../Public/Configuration.h"
#include "../Public/Events.h"
#include "../Public/ZonePlanner.h"

#include "../Public/Dumper.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <wincodec.h>
#include <fstream>
#include <sstream>

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <atomic>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

#if __has_include("../Public/BoronAssets.h")
#include "../Public/BoronAssets.h"
#define BORON_EMBEDDED_ASSETS
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

UINT g_ResizeWidth = 0, g_ResizeHeight = 0;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
// GUI SIZE
auto WindowWidth = 1000;
auto WindowHeight = 660;

static float S = 1.f;
static HWND g_hWnd = nullptr;
static bool g_Pinned = false;
static float g_WindowAlpha = 255.f;
static std::vector<std::string> g_ConsoleHistory;
static ImFont* g_FontDisplay = nullptr;
static ImFont* g_FontBold = nullptr;

static const ImVec4 kRed(0.94f, 0.22f, 0.27f, 1.f);
static const ImVec4 kRedDark(0.55f, 0.06f, 0.10f, 1.f);
static const ImVec4 kYellow(1.f, 0.86f, 0.17f, 1.f);
static const ImVec4 kGreen(0.36f, 0.90f, 0.47f, 1.f);
static const ImVec4 kBlue(0.27f, 0.66f, 1.f, 1.f);
static const ImVec4 kInk(0.035f, 0.05f, 0.10f, 1.f);
static const ImVec4 kMuted(0.60f, 0.65f, 0.76f, 1.f);

enum EIcon
{
    IcoServer,
    IcoStorm,
    IcoPeople,
    IcoCogs,
    IcoCrown,
    IcoToolbox,
    IcoCalendar,
    IcoBus,
    IcoStopwatch,
    IcoKeyboard,
    IcoGift,
    IcoKick,
    IcoHeart,
    IcoMedal,
    IcoPlace,
    IcoCount
};

static const char* g_IconFiles[IcoCount] = { "Server", "StormCloud", "People", "Cogs", "Crown", "Toolbox", "Calendar", "Bus", "Stopwatch", "Keyboard", "Gift", "Disconnected", "Heartbeat", "Medal", "Place" };
static ID3D11ShaderResourceView* g_Icons[IcoCount] = {};
static ID3D11DeviceContext* g_IconContext = nullptr;
static ID3D11Device* g_Device = nullptr;

static ImVec4 Alpha(ImVec4 c, float a)
{
    c.w = a;
    return c;
}

static ImU32 U32(const ImVec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(c);
}

static ImVec4 Mix(const ImVec4& x, const ImVec4& y, float t)
{
    return ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t);
}

static float Anim(ImGuiID id, float target, float speed = 14.f)
{
    float* v = ImGui::GetStateStorage()->GetFloatRef(id, target);
    float k = ImGui::GetIO().DeltaTime * speed;
    *v += (target - *v) * (k > 1.f ? 1.f : k);
    return *v;
}

static std::string Upper(const char* Text)
{
    std::string s = Text ? Text : "";
    for (auto& Ch : s)
        Ch = (char)toupper((unsigned char)Ch);
    return s;
}

static ID3D11ShaderResourceView* LoadTexture(ID3D11Device* Device, const char* Path, const unsigned char* Memory = nullptr, unsigned int MemorySize = 0, bool bWhite = true)
{
    wchar_t WPath[MAX_PATH] = { 0 };
    if (!Memory)
    {
        if (!Path || GetFileAttributesA(Path) == INVALID_FILE_ATTRIBUTES)
            return nullptr;
        MultiByteToWideChar(CP_ACP, 0, Path, -1, WPath, MAX_PATH);
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* Factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Factory))))
        return nullptr;

    ID3D11ShaderResourceView* Srv = nullptr;
    IWICStream* Stream = nullptr;
    IWICBitmapDecoder* Decoder = nullptr;
    IWICBitmapFrameDecode* Frame = nullptr;
    IWICFormatConverter* Converter = nullptr;
    bool bDecoder = false;
    if (Memory)
        bDecoder = SUCCEEDED(Factory->CreateStream(&Stream)) && SUCCEEDED(Stream->InitializeFromMemory((BYTE*)Memory, MemorySize)) &&
                   SUCCEEDED(Factory->CreateDecoderFromStream(Stream, nullptr, WICDecodeMetadataCacheOnDemand, &Decoder));
    else
        bDecoder = SUCCEEDED(Factory->CreateDecoderFromFilename(WPath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &Decoder));
    if (bDecoder && SUCCEEDED(Decoder->GetFrame(0, &Frame)) &&
        SUCCEEDED(Factory->CreateFormatConverter(&Converter)) &&
        SUCCEEDED(Converter->Initialize(Frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {
        UINT W = 0, H = 0;
        Converter->GetSize(&W, &H);
        std::vector<BYTE> Pixels(W * H * 4);
        if (W && H && SUCCEEDED(Converter->CopyPixels(nullptr, W * 4, (UINT)Pixels.size(), Pixels.data())))
        {
            for (UINT i = 0; bWhite && i < W * H; i++)
            {
                BYTE* Px = &Pixels[i * 4];
                Px[0] = 255;
                Px[1] = 255;
                Px[2] = 255;
            }
            D3D11_TEXTURE2D_DESC Desc{};
            Desc.Width = W;
            Desc.Height = H;
            Desc.MipLevels = g_IconContext ? 0 : 1;
            Desc.ArraySize = 1;
            Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            Desc.SampleDesc.Count = 1;
            Desc.Usage = D3D11_USAGE_DEFAULT;
            Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (g_IconContext ? D3D11_BIND_RENDER_TARGET : 0);
            Desc.MiscFlags = g_IconContext ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
            D3D11_SUBRESOURCE_DATA Init{ Pixels.data(), W * 4, 0 };
            ID3D11Texture2D* Tex = nullptr;
            if (SUCCEEDED(Device->CreateTexture2D(&Desc, g_IconContext ? nullptr : &Init, &Tex)))
            {
                if (SUCCEEDED(Device->CreateShaderResourceView(Tex, nullptr, &Srv)) && g_IconContext)
                {
                    g_IconContext->UpdateSubresource(Tex, 0, nullptr, Pixels.data(), W * 4, 0);
                    g_IconContext->GenerateMips(Srv);
                }
                Tex->Release();
            }
        }
    }
    if (Converter)
        Converter->Release();
    if (Frame)
        Frame->Release();
    if (Decoder)
        Decoder->Release();
    if (Stream)
        Stream->Release();
    Factory->Release();
    return Srv;
}

static void DrawIcon(ImDrawList* Dl, EIcon Icon, ImVec2 Pos, float Size, ImU32 Col)
{
    if (!g_Icons[Icon])
        return;
    Dl->AddImage(ImTextureRef((ImTextureID)(uintptr_t)g_Icons[Icon]), Pos, ImVec2(Pos.x + Size, Pos.y + Size), ImVec2(0, 0), ImVec2(1, 1), Col);
}

static void CenteredGlyphText(ImDrawList* Dl, ImVec2 C, ImU32 Col, const char* Text)
{
    auto Baked = ImGui::GetFontBaked();
    float MinX = 1e9f, MaxX = -1e9f, MinY = 1e9f, MaxY = -1e9f, Pen = 0.f;
    for (const char* c = Text; *c; c++)
    {
        auto Glyph = Baked ? Baked->FindGlyph((ImWchar)(unsigned char)*c) : nullptr;
        if (!Glyph)
            continue;
        MinX = (std::min)(MinX, Pen + Glyph->X0);
        MaxX = (std::max)(MaxX, Pen + Glyph->X1);
        MinY = (std::min)(MinY, Glyph->Y0);
        MaxY = (std::max)(MaxY, Glyph->Y1);
        Pen += Glyph->AdvanceX;
    }
    if (MinX > MaxX)
    {
        ImVec2 ts = ImGui::CalcTextSize(Text);
        Dl->AddText(ImVec2(C.x - ts.x * 0.5f, C.y - ts.y * 0.5f), Col, Text);
        return;
    }
    Dl->AddText(ImVec2(IM_ROUND(C.x - (MinX + MaxX) * 0.5f), IM_ROUND(C.y - (MinY + MaxY) * 0.5f)), Col, Text);
}

static void DrawLogo(ImDrawList* Dl, ImVec2 C, float R)
{
    for (int i = 4; i >= 1; i--)
        Dl->AddCircleFilled(C, R + i * 3.f * S, U32(Alpha(kRed, 0.035f * (5 - i))), 48);
    Dl->AddCircleFilled(C, R, U32(kRedDark), 48);
    const int Steps = 14;
    for (int i = 1; i <= Steps; i++)
    {
        float t = (float)i / Steps;
        float r = R * (1.f - t * 0.82f);
        ImVec2 o(C.x - (R - r) * 0.42f, C.y - (R - r) * 0.42f);
        Dl->AddCircleFilled(o, r, U32(Mix(kRedDark, ImVec4(1.f, 0.42f, 0.42f, 1.f), t * t)), 48);
    }
    Dl->AddCircleFilled(ImVec2(C.x - R * 0.38f, C.y - R * 0.42f), R * 0.22f, IM_COL32(255, 255, 255, 60), 24);

    ImGui::PushFont(g_FontDisplay, R * 1.2f / S);
    CenteredGlyphText(Dl, C, IM_COL32(255, 255, 255, 255), "B");
    ImGui::PopFont();

    ImVec2 Sc(C.x + R * 0.86f, C.y - R * 0.70f);
    float Sr = R * 0.38f;
    Dl->AddCircleFilled(Sc, Sr, U32(kRedDark), 32);
    Dl->AddCircleFilled(ImVec2(Sc.x - Sr * 0.15f, Sc.y - Sr * 0.15f), Sr * 0.82f, U32(Mix(kRed, ImVec4(1.f, 0.5f, 0.5f, 1.f), 0.3f)), 32);
    ImGui::PushFont(g_FontBold, Sr * 1.15f / S);
    CenteredGlyphText(Dl, Sc, IM_COL32(255, 255, 255, 255), "5");
    ImGui::PopFont();
}

static void ApplyColors()
{
    auto& c = ImGui::GetStyle().Colors;
    c[ImGuiCol_Text] = ImVec4(0.96f, 0.97f, 1.f, 1.f);
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_WindowBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ChildBg] = ImVec4(1.f, 1.f, 1.f, 0.04f);
    c[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.07f, 0.14f, 0.99f);
    c[ImGuiCol_Border] = ImVec4(1.f, 1.f, 1.f, 0.08f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_FrameBg] = ImVec4(0.f, 0.f, 0.f, 0.30f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.f, 0.f, 0.f, 0.42f);
    c[ImGuiCol_FrameBgActive] = Alpha(kRed, 0.25f);
    c[ImGuiCol_TitleBg] = kInk;
    c[ImGuiCol_TitleBgActive] = kInk;
    c[ImGuiCol_TitleBgCollapsed] = kInk;
    c[ImGuiCol_Button] = ImVec4(1.f, 1.f, 1.f, 0.08f);
    c[ImGuiCol_ButtonHovered] = ImVec4(1.f, 1.f, 1.f, 0.16f);
    c[ImGuiCol_ButtonActive] = Alpha(kRed, 0.6f);
    c[ImGuiCol_CheckMark] = kRed;
    c[ImGuiCol_SliderGrab] = kRed;
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.f, 0.45f, 0.48f, 1.f);
    c[ImGuiCol_Header] = Alpha(kRed, 0.22f);
    c[ImGuiCol_HeaderHovered] = ImVec4(1.f, 1.f, 1.f, 0.08f);
    c[ImGuiCol_HeaderActive] = Alpha(kRed, 0.35f);
    c[ImGuiCol_Separator] = ImVec4(1.f, 1.f, 1.f, 0.08f);
    c[ImGuiCol_SeparatorHovered] = Alpha(kRed, 0.5f);
    c[ImGuiCol_SeparatorActive] = kRed;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(1.f, 1.f, 1.f, 0.12f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.f, 1.f, 1.f, 0.2f);
    c[ImGuiCol_ScrollbarGrabActive] = Alpha(kRed, 0.7f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.f, 0.f, 0.f, 0.25f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(1.f, 1.f, 1.f, 0.08f);
    c[ImGuiCol_TableBorderLight] = ImVec4(1.f, 1.f, 1.f, 0.05f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.f, 1.f, 1.f, 0.03f);
    c[ImGuiCol_TextSelectedBg] = Alpha(kRed, 0.4f);
    c[ImGuiCol_NavCursor] = kYellow;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.f, 0.f, 0.05f, 0.55f);
}

static void Hint(const char* Text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", Text);
}

static bool Toggle(const char* Label, bool* V, const char* Tip = nullptr)
{
    float fh = ImGui::GetFrameHeight();
    float h = fh * 0.80f;
    float w = h * 1.9f;
    float inner = 12.f * S;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 ts = ImGui::CalcTextSize(Label);
    bool bPressed = ImGui::InvisibleButton(Label, ImVec2(w + inner + ts.x, fh));
    if (bPressed)
        *V = !*V;
    bool bHovered = ImGui::IsItemHovered();
    float t = Anim(ImGui::GetItemID(), *V ? 1.f : 0.f);
    bool bDisabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;

    auto Dl = ImGui::GetWindowDrawList();
    float y = p.y + (fh - h) * 0.5f;
    ImVec4 Off(0.f, 0.f, 0.f, bHovered ? 0.55f : 0.40f);
    Dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), U32(Mix(Off, kRed, t)), 4.f * S);
    Dl->AddRect(ImVec2(p.x, y), ImVec2(p.x + w, y + h), IM_COL32(255, 255, 255, (int)(40 * (1.f - t))), 4.f * S);
    float k = h - 6.f * S;
    float kx = p.x + 3.f * S + t * (w - k - 6.f * S);
    Dl->AddRectFilled(ImVec2(kx, y + 3.f * S), ImVec2(kx + k, y + 3.f * S + k), IM_COL32(255, 255, 255, bDisabled ? 120 : 250), 3.f * S);
    Dl->AddText(ImVec2(p.x + w + inner, p.y + (fh - ts.y) * 0.5f), ImGui::GetColorU32(bDisabled ? ImGuiCol_TextDisabled : ImGuiCol_Text), Label);
    if (Tip)
        Hint(Tip);
    return bPressed;
}

static bool SkewButton(const char* Id, const char* Label, ImVec2 Size, ImVec4 Fill, ImVec4 TextCol, int Icon = -1, float FontSize = 0.f)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool bPressed = ImGui::InvisibleButton(Id, Size);
    bool bHovered = ImGui::IsItemHovered();
    bool bHeld = ImGui::IsItemActive();
    float t = Anim(ImGui::GetItemID(), bHovered ? 1.f : 0.f, 16.f);
    auto Dl = ImGui::GetWindowDrawList();
    float Sk = Size.y * 0.16f;
    ImVec4 Col = bHeld ? Mix(Fill, ImVec4(0, 0, 0, 1), 0.15f) : Mix(Fill, ImVec4(1, 1, 1, 1), 0.18f * t);
    float Lift = t * 2.f * S;
    ImVec2 a(p.x + Sk, p.y - Lift), b(p.x + Size.x, p.y - Lift), c(p.x + Size.x - Sk, p.y + Size.y - Lift), d(p.x, p.y + Size.y - Lift);
    Dl->AddQuadFilled(ImVec2(a.x, a.y + 4 * S), ImVec2(b.x, b.y + 4 * S), ImVec2(c.x, c.y + 4 * S), ImVec2(d.x, d.y + 4 * S), IM_COL32(0, 0, 0, 70));
    Dl->AddQuadFilled(a, b, c, d, U32(Col));
    Dl->AddQuadFilled(a, b, ImVec2(b.x - Sk * 0.35f, b.y + Size.y * 0.35f), ImVec2(a.x - Sk * 0.35f, a.y + Size.y * 0.35f), IM_COL32(255, 255, 255, 28));

    ImGui::PushFont(g_FontDisplay, FontSize > 0.f ? FontSize : (g_FontDisplay ? Size.y * 0.52f / S : 17.f));
    std::string Text = Upper(Label);
    ImVec2 ts = ImGui::CalcTextSize(Text.c_str());
    float IconSize = Icon >= 0 && g_Icons[Icon] ? ts.y * 0.95f : 0.f;
    float Gap = IconSize > 0.f ? 8.f * S : 0.f;
    float x = p.x + (Size.x - ts.x - IconSize - Gap) * 0.5f;
    float Cy = p.y - Lift + Size.y * 0.5f;
    if (IconSize > 0.f)
        DrawIcon(Dl, (EIcon)Icon, ImVec2(x, Cy - IconSize * 0.5f), IconSize, U32(TextCol));
    CenteredGlyphText(Dl, ImVec2(x + IconSize + Gap + ts.x * 0.5f, Cy), U32(TextCol), Text.c_str());
    ImGui::PopFont();
    return bPressed;
}

static bool Tile(const char* Id, EIcon Icon, const char* Title, const char* Sub, ImVec2 Size, ImVec4 Tint)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool bPressed = ImGui::InvisibleButton(Id, Size);
    bool bHovered = ImGui::IsItemHovered();
    bool bHeld = ImGui::IsItemActive();
    float t = Anim(ImGui::GetItemID(), bHovered ? 1.f : 0.f, 16.f);
    auto Dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + Size.x, p.y + Size.y);
    Dl->AddRectFilled(p, q, U32(Mix(ImVec4(1, 1, 1, 0.04f), Alpha(Tint, bHeld ? 0.35f : 0.22f), t)), 6.f * S);
    Dl->AddRect(p, q, U32(Mix(ImVec4(1, 1, 1, 0.08f), Alpha(Tint, 0.9f), t)), 6.f * S, 0, 1.5f * S);
    Dl->AddRectFilled(p, ImVec2(p.x + 4 * S, q.y), U32(Tint), 6.f * S, ImDrawFlags_RoundCornersLeft);
    float Ic = Size.y * 0.46f;
    DrawIcon(Dl, Icon, ImVec2(p.x + 18 * S, p.y + (Size.y - Ic) * 0.5f), Ic, U32(Mix(Alpha(Tint, 0.85f), ImVec4(1, 1, 1, 1), t)));
    float tx = p.x + 18 * S + Ic + 14 * S;
    ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 24.f : 18.f);
    std::string T = Upper(Title);
    ImVec2 ts = ImGui::CalcTextSize(T.c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, 14.f);
    ImVec2 ss = ImGui::CalcTextSize(Sub);
    ImGui::PopFont();
    float ty = p.y + (Size.y - ts.y - ss.y - 2 * S) * 0.5f;
    ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 24.f : 18.f);
    Dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), T.c_str());
    ImGui::PopFont();
    ImGui::PushFont(nullptr, 14.f);
    Dl->AddText(ImVec2(tx, ty + ts.y + 2 * S), U32(kMuted), Sub);
    ImGui::PopFont();
    return bPressed;
}

static void BeginPanel(const char* Id, const char* Title = nullptr, int Icon = -1, ImVec2 Size = ImVec2(0, 0))
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * S, 14 * S));
    ImGui::BeginChild(Id, Size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | (Size.y == 0 ? ImGuiChildFlags_AutoResizeY : 0), ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    if (Title)
    {
        auto Dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 22.f : 16.f);
        std::string T = Upper(Title);
        ImVec2 ts = ImGui::CalcTextSize(T.c_str());
        float x = p.x;
        Dl->AddQuadFilled(ImVec2(x + 4 * S, p.y + 2 * S), ImVec2(x + 9 * S, p.y + 2 * S), ImVec2(x + 5 * S, p.y + ts.y - 2 * S), ImVec2(x, p.y + ts.y - 2 * S), U32(kRed));
        x += 16 * S;
        if (Icon >= 0 && g_Icons[Icon])
        {
            DrawIcon(Dl, (EIcon)Icon, ImVec2(x, p.y + ts.y * 0.12f), ts.y * 0.76f, IM_COL32(255, 255, 255, 200));
            x += ts.y * 0.76f + 8 * S;
        }
        Dl->AddText(ImVec2(x, p.y), IM_COL32(255, 255, 255, 255), T.c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, ts.y + 6 * S));
    }
}

static void EndPanel()
{
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6 * S));
}

static void PageTitle(const char* Title, const char* Sub)
{
    ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 40.f : 26.f);
    ImGui::TextUnformatted(Upper(Title).c_str());
    ImGui::PopFont();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4 * S);
    ImGui::TextDisabled("%s", Sub);
    ImGui::Dummy(ImVec2(0, 6 * S));
}

static void Spinner(float Radius, float Thickness, ImVec4 Col)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(Radius * 2, Radius * 2));
    auto Dl = ImGui::GetWindowDrawList();
    float t = (float)ImGui::GetTime() * 5.f;
    ImVec2 c(p.x + Radius, p.y + Radius);
    Dl->AddCircle(c, Radius - Thickness * 0.5f, IM_COL32(255, 255, 255, 30), 32, Thickness);
    Dl->PathClear();
    Dl->PathArcTo(c, Radius - Thickness * 0.5f, t, t + 3.14159265f * 1.3f, 32);
    Dl->PathStroke(U32(Col), 0, Thickness);
}

static void Chip(EIcon Icon, const char* Text, ImVec4 Col)
{
    ImGui::PushFont(g_FontBold, 15.f);
    ImVec2 ts = ImGui::CalcTextSize(Text);
    float Ic = ts.y;
    ImVec2 Size(Ic + ts.x + 26 * S, ts.y + 12 * S);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(Size);
    auto Dl = ImGui::GetWindowDrawList();
    Dl->AddRectFilled(p, ImVec2(p.x + Size.x, p.y + Size.y), IM_COL32(0, 0, 0, 90), 4 * S);
    DrawIcon(Dl, Icon, ImVec2(p.x + 9 * S, p.y + 6 * S), Ic, U32(Col));
    Dl->AddText(ImVec2(p.x + 9 * S + Ic + 7 * S, p.y + 6 * S), IM_COL32(255, 255, 255, 235), Text);
    ImGui::PopFont();
}

static bool NavTab(const char* Label, bool bSelected, float Height)
{
    std::string T = Upper(Label);
    ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 20.f : 15.f);
    ImVec2 ts = ImGui::CalcTextSize(T.c_str());
    ImVec2 Size(ts.x + 24 * S, Height);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool bPressed = ImGui::InvisibleButton(Label, Size);
    bool bHovered = ImGui::IsItemHovered();
    float t = Anim(ImGui::GetItemID(), bSelected ? 1.f : 0.f, 18.f);
    float h = Anim(ImGui::GetItemID() + 1, bHovered ? 1.f : 0.f, 18.f);
    auto Dl = ImGui::GetWindowDrawList();
    float PillH = (std::min)(Size.y - 12 * S, 34 * S);
    float pad = (Size.y - PillH) * 0.5f;
    ImVec2 a(p.x, p.y + pad), b(p.x + Size.x, p.y + Size.y - pad);
    if (h > 0.01f && !bSelected)
        Dl->AddRectFilled(a, b, U32(ImVec4(1, 1, 1, 0.10f * h)), 5 * S);
    if (t > 0.01f)
        Dl->AddRectFilled(ImVec2(a.x + (1.f - t) * Size.x * 0.5f, a.y), ImVec2(b.x - (1.f - t) * Size.x * 0.5f, b.y), U32(ImVec4(0.93f, 0.95f, 1.f, t)), 5 * S);
    ImVec4 TextCol = Mix(ImVec4(1, 1, 1, 0.78f + 0.22f * h), Alpha(kInk, 1.f), t);
    CenteredGlyphText(Dl, ImVec2(p.x + Size.x * 0.5f, p.y + Size.y * 0.5f), U32(TextCol), T.c_str());
    ImGui::PopFont();
    return bPressed;
}

static bool TitleButton(const char* Id, int Kind, const ImVec2& Pos, float Size)
{
    ImGui::SetCursorPos(Pos);
    bool bPressed = ImGui::InvisibleButton(Id, ImVec2(Size, Size));
    bool bHovered = ImGui::IsItemHovered();
    ImVec2 a = ImGui::GetItemRectMin();
    ImVec2 b = ImGui::GetItemRectMax();
    ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    auto Dl = ImGui::GetWindowDrawList();
    float t = Anim(ImGui::GetItemID(), bHovered ? 1.f : 0.f, 18.f);
    ImVec4 Hover = Kind == 2 ? Alpha(kRed, 0.95f) : ImVec4(1.f, 1.f, 1.f, 0.12f);
    if (t > 0.01f)
        Dl->AddRectFilled(a, b, U32(Alpha(Hover, Hover.w * t)), 6 * S);
    ImU32 g = IM_COL32(235, 240, 250, 235);
    float e = 5 * S;
    if (Kind == 0)
    {
        if (g_Pinned)
            Dl->AddCircleFilled(c, 4.5f * S, U32(kYellow));
        else
            Dl->AddCircle(c, 4.5f * S, g, 16, 1.6f * S);
    }
    else if (Kind == 1)
        Dl->AddLine(ImVec2(c.x - e, c.y), ImVec2(c.x + e, c.y), g, 1.8f * S);
    else
    {
        Dl->AddLine(ImVec2(c.x - e, c.y - e), ImVec2(c.x + e, c.y + e), g, 1.8f * S);
        Dl->AddLine(ImVec2(c.x - e, c.y + e), ImVec2(c.x + e, c.y - e), g, 1.8f * S);
    }
    return bPressed;
}

static void ShutdownModal()
{
    ImVec2 wp = ImGui::GetWindowPos();
    ImGui::SetNextWindowPos(ImVec2(wp.x + ImGui::GetWindowWidth() * 0.5f, wp.y + ImGui::GetWindowHeight() * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24 * S, 20 * S));
    if (ImGui::BeginPopupModal("##shutdown", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 34.f : 22.f);
        ImGui::TextUnformatted("SHUT DOWN SERVER?");
        ImGui::PopFont();
        ImGui::TextDisabled("Every connected player gets disconnected.");
        ImGui::Dummy(ImVec2(0, 10 * S));
        if (SkewButton("##sd_yes", "Shut down", ImVec2(170 * S, 44 * S), kRed, ImVec4(1, 1, 1, 1)))
            PostMessage(g_hWnd, WM_CLOSE, 0, 0);
        ImGui::SameLine(0, 12 * S);
        if (SkewButton("##sd_no", "Cancel", ImVec2(150 * S, 44 * S), ImVec4(0.93f, 0.95f, 1.f, 1.f), kInk))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

static void ConsolePanel(char* commandBuffer)
{
    BeginPanel("console", "Console", IcoKeyboard);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.35f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12 * S, 8 * S));
    ImGui::BeginChild("history", ImVec2(0, 92 * S), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (g_ConsoleHistory.empty())
        ImGui::TextDisabled("Commands you run show up here. Click one to reuse it.");
    for (size_t i = 0; i < g_ConsoleHistory.size(); i++)
    {
        ImGui::PushID((int)i);
        ImGui::TextColored(kRed, ">");
        ImGui::SameLine(0, 8 * S);
        if (ImGui::Selectable(g_ConsoleHistory[i].c_str()))
            strncpy_s(commandBuffer, 1024, g_ConsoleHistory[i].c_str(), _TRUNCATE);
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 2 * S));

    float RunW = 120 * S;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - RunW - 12 * S);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12 * S, 10 * S));
    bool bRun = ImGui::InputTextWithHint("##console", "Console command...", commandBuffer, 1024, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 12 * S);
    bRun |= SkewButton("##exec", "Execute", ImVec2(RunW, ImGui::GetItemRectSize().y), kRed, ImVec4(1, 1, 1, 1));
    if (bRun && commandBuffer[0])
    {
        std::string str = commandBuffer;
        auto wstr = std::wstring(str.begin(), str.end());

        GUI::RunOnGameThread([wstr] { UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(wstr.c_str()), nullptr); });

        g_ConsoleHistory.insert(g_ConsoleHistory.begin(), str);
        if (g_ConsoleHistory.size() > 6)
            g_ConsoleHistory.pop_back();
        commandBuffer[0] = 0;
    }
    EndPanel();
}

struct FPlayerEntry
{
    AFortPlayerControllerAthena* PC;
    std::string Name;
};

struct FRawWideArray
{
    wchar_t* Data;
    int32 Num;
    int32 Max;
};

static bool ReadUrlName(void* Conn, int Offset, char* Out, int OutSize)
{
    __try
    {
        auto Arr = (FRawWideArray*)(__int64(Conn) + Offset);
        if (!Arr->Data || Arr->Num < 6 || Arr->Num > 4096 || Arr->Max < Arr->Num || (uintptr_t)Arr->Data < 0x10000)
            return false;
        const wchar_t* Str = Arr->Data;
        if (Str[Arr->Num - 1] != 0)
            return false;
        const wchar_t* At = wcsstr(Str, L"Name=");
        if (!At || (At != Str && At[-1] != L'?'))
            return false;
        At += 5;
        int n = 0;
        while (At[n] && At[n] != L'?' && n < OutSize - 1)
        {
            Out[n] = (At[n] >= 32 && At[n] < 127) ? (char)At[n] : '_';
            n++;
        }
        Out[n] = 0;
        return n > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static std::string GetConnectionName(UNetConnection* Conn)
{
    static int UrlOffset = -1;
    static int Tries = 0;
    char Name[64];
    if (UrlOffset < 0 && Tries < 20)
    {
        Tries++;
        for (int Off = 0x40; Off < 0x1000; Off += 8)
            if (ReadUrlName(Conn, Off, Name, sizeof(Name)))
            {
                UrlOffset = Off;
                BORON_LOG("[Boron][GUI] connection RequestURL found at +0x%X (name=%s)\n", Off, Name);
                break;
            }
    }
    if (UrlOffset >= 0 && ReadUrlName(Conn, UrlOffset, Name, sizeof(Name)))
        return Name;
    return "";
}

std::string GUI::GetPlayerName(void* PlayerController)
{
    auto World = UWorld::GetWorld();
    auto NetDriver = World ? (UNetDriver*)World->NetDriver : nullptr;
    if (!NetDriver || !PlayerController)
        return "";
    for (auto Conn : NetDriver->ClientConnections)
        if (Conn && (void*)Conn->PlayerController == PlayerController)
            return GetConnectionName(Conn);
    return "";
}

static std::vector<FPlayerEntry> CollectPlayers()
{
    std::vector<FPlayerEntry> Out;
    auto World = UWorld::GetWorld();
    auto NetDriver = World ? (UNetDriver*)World->NetDriver : nullptr;
    if (!NetDriver)
        return Out;
    for (auto Conn : NetDriver->ClientConnections)
    {
        if (!Conn || !Conn->PlayerController)
            continue;
        FPlayerEntry Entry{ Conn->PlayerController, GetConnectionName(Conn) };
        if (Entry.Name.empty())
        {
            auto PS = Entry.PC->PlayerState;
            if (PS && PS->HasPlayerNamePrivate() && PS->PlayerNamePrivate.Num() > 0)
            {
                auto Wide = PS->PlayerNamePrivate.ToWString();
                Entry.Name = std::string(Wide.begin(), Wide.end());
            }
        }
        if (Entry.Name.empty())
            Entry.Name = "Player " + std::to_string(Out.size() + 1);
        Out.push_back(Entry);
    }
    return Out;
}

static void GiveTo(AFortPlayerControllerAthena* PlayerController, const char* Path, int Count)
{
    if (!PlayerController || !PlayerController->WorldInventory)
        return;
    wchar_t wPath[500];
    swprintf_s(wPath, L"%hs", Path);
    auto ItemDef = FindObject<UFortItemDefinition>(wPath);
    if (ItemDef)
        PlayerController->WorldInventory->GiveItem(ItemDef, Count < 1 ? 1 : Count);
}

static void KickPlayer(AFortPlayerControllerAthena* PlayerController, const char* Reason)
{
    std::string r = Reason;
    std::wstring w(r.begin(), r.end());
    if (auto Fn = PlayerController->GetFunction("ClientReturnToMainMenuWithTextReason"))
    {
        struct
        {
            FText ReturnReason;
        } Params{ UKismetTextLibrary::Conv_StringToText(FString(w.c_str())) };
        PlayerController->ProcessEvent(Fn, &Params);
    }
    else if (auto Fn = PlayerController->GetFunction("ClientReturnToMainMenu"))
    {
        struct
        {
            FString ReturnReason;
        } Params{ FString(w.c_str()) };
        PlayerController->ProcessEvent(Fn, &Params);
    }
}

static ImVec4 TeamColor(int Team)
{
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(fmodf(Team * 0.137f + 0.55f, 1.f), 0.62f, 0.95f, r, g, b);
    return ImVec4(r, g, b, 1.f);
}

static void DrawAvatar(ImDrawList* Dl, ImVec2 C, float R, const char* Name, int Team, bool bAlive)
{
    ImVec4 Col = TeamColor(Team);
    Dl->AddCircleFilled(C, R, U32(Mix(Col, kInk, 0.55f)), 40);
    Dl->AddCircleFilled(ImVec2(C.x - R * 0.12f, C.y - R * 0.12f), R * 0.86f, U32(Mix(Col, kInk, 0.25f)), 40);
    Dl->AddCircle(C, R, U32(bAlive ? Col : kMuted), 40, 2.f * S);
    char Letter[2] = { Name && Name[0] ? (char)toupper((unsigned char)Name[0]) : '?', 0 };
    ImGui::PushFont(g_FontDisplay, R * 1.15f / S);
    ImVec2 ts = ImGui::CalcTextSize(Letter);
    Dl->AddText(ImVec2(C.x - ts.x * 0.5f, C.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255), Letter);
    ImGui::PopFont();
    if (bAlive)
        Dl->AddCircleFilled(ImVec2(C.x + R * 0.72f, C.y + R * 0.72f), R * 0.22f, U32(kGreen), 16);
}

static void StatBar(ImDrawList* Dl, ImVec2 P, float W, float H, float Value, float Max, ImVec4 Col, bool bLabel)
{
    float Frac = Max > 0.f ? Value / Max : 0.f;
    Frac = Frac < 0.f ? 0.f : (Frac > 1.f ? 1.f : Frac);
    float Sk = H * 0.45f;
    auto Para = [&](float x0, float x1, ImU32 c)
    {
        Dl->AddQuadFilled(ImVec2(P.x + x0 + Sk, P.y), ImVec2(P.x + x1 + Sk, P.y), ImVec2(P.x + x1, P.y + H), ImVec2(P.x + x0, P.y + H), c);
    };
    float Usable = W - Sk;
    Para(0.f, Usable, IM_COL32(0, 0, 0, 130));
    if (Frac > 0.f)
    {
        Para(0.f, Usable * Frac, U32(Col));
        Dl->AddQuadFilled(ImVec2(P.x + Sk, P.y), ImVec2(P.x + Usable * Frac + Sk, P.y), ImVec2(P.x + Usable * Frac + Sk * 0.6f, P.y + H * 0.4f), ImVec2(P.x + Sk * 0.6f, P.y + H * 0.4f), IM_COL32(255, 255, 255, 55));
    }
    for (int i = 1; i < 4; i++)
    {
        float x = Usable * i * 0.25f;
        Dl->AddLine(ImVec2(P.x + x + Sk, P.y), ImVec2(P.x + x, P.y + H), IM_COL32(0, 0, 0, 120), 2.f * S);
    }
    if (bLabel)
    {
        char Text[32];
        sprintf_s(Text, "%d", (int)(Value + 0.5f));
        ImGui::PushFont(g_FontDisplay, H / S * 1.05f);
        ImVec2 ts = ImGui::CalcTextSize(Text);
        Dl->AddText(ImVec2(P.x + Sk + 8 * S, P.y + (H - ts.y) * 0.5f), IM_COL32(255, 255, 255, 255), Text);
        ImGui::PopFont();
    }
}

static float PawnFloat(UObject* Pawn, const char* Name, bool* bFound = nullptr)
{
    static std::vector<std::pair<std::string, UFunction*>> Cache;
    UFunction* Fn = nullptr;
    bool bCached = false;
    for (auto& Entry : Cache)
        if (Entry.first == Name)
        {
            Fn = Entry.second;
            bCached = true;
        }
    if (!bCached)
    {
        Fn = Pawn->GetFunction(Name);
        Cache.push_back({ Name, Fn });
    }
    if (bFound)
        *bFound = Fn != nullptr;
    if (!Fn)
        return 0.f;
    struct
    {
        float ReturnValue;
    } Params{ 0.f };
    Pawn->ProcessEvent(Fn, &Params);
    return Params.ReturnValue;
}

static int GetElims(AFortPlayerStateAthena* PS)
{
    if (!PS)
        return 0;
    if (PS->HasKillScore())
        return (int)PS->KillScore;
    return PS->HasKills() ? (int)PS->Kills : 0;
}

static void DrawShieldGlyph(ImDrawList* Dl, ImVec2 C, float R, ImU32 Col)
{
    ImVec2 Pts[5] = { ImVec2(C.x - R * 0.8f, C.y - R * 0.7f), ImVec2(C.x, C.y - R), ImVec2(C.x + R * 0.8f, C.y - R * 0.7f), ImVec2(C.x + R * 0.7f, C.y + R * 0.1f), ImVec2(C.x, C.y + R) };
    Dl->AddConvexPolyFilled(Pts, 5, Col);
    ImVec2 Left[3] = { ImVec2(C.x - R * 0.8f, C.y - R * 0.7f), ImVec2(C.x, C.y + R), ImVec2(C.x - R * 0.7f, C.y + R * 0.1f) };
    Dl->AddConvexPolyFilled(Left, 3, Col);
}

static void DrawPlusGlyph(ImDrawList* Dl, ImVec2 C, float R, ImU32 Col)
{
    float t = R * 0.36f;
    Dl->AddRectFilled(ImVec2(C.x - t, C.y - R), ImVec2(C.x + t, C.y + R), Col, 1.f * S);
    Dl->AddRectFilled(ImVec2(C.x - R, C.y - t), ImVec2(C.x + R, C.y + t), Col, 1.f * S);
}

static void HudBar(ImDrawList* Dl, ImVec2 P, float W, float H, float Value, float Max, ImVec4 ColA, ImVec4 ColB, int Glyph)
{
    float IconW = H + 10 * S;
    float NumW = 46 * S;
    ImVec2 Ic(P.x + H * 0.5f, P.y + H * 0.5f);
    if (Glyph == 0)
        DrawShieldGlyph(Dl, Ic, H * 0.42f, IM_COL32(220, 228, 240, 220));
    else if (Glyph == 1)
        DrawPlusGlyph(Dl, Ic, H * 0.40f, IM_COL32(220, 228, 240, 220));
    else
    {
        DrawShieldGlyph(Dl, Ic, H * 0.46f, U32(ColA));
        DrawShieldGlyph(Dl, Ic, H * 0.26f, IM_COL32(255, 255, 255, 200));
    }
    float BarX = P.x + IconW;
    float BarW = W - IconW - NumW;
    float Frac = Max > 0.f ? Value / Max : 0.f;
    Frac = Frac < 0.f ? 0.f : (Frac > 1.f ? 1.f : Frac);
    Dl->AddRectFilled(ImVec2(BarX, P.y), ImVec2(BarX + BarW, P.y + H), IM_COL32(0, 0, 0, 120), 3 * S);
    if (Frac > 0.f)
    {
        Dl->AddRectFilledMultiColor(ImVec2(BarX, P.y), ImVec2(BarX + BarW * Frac, P.y + H), U32(ColA), U32(ColB), U32(ColB), U32(ColA));
        Dl->AddRectFilled(ImVec2(BarX, P.y), ImVec2(BarX + BarW * Frac, P.y + H * 0.35f), IM_COL32(255, 255, 255, 40));
        Dl->AddRectFilled(ImVec2(BarX + BarW * Frac - 2 * S, P.y), ImVec2(BarX + BarW * Frac, P.y + H), IM_COL32(255, 255, 255, 150));
    }
    char Text[16];
    sprintf_s(Text, "%d", (int)(Value + 0.5f));
    ImGui::PushFont(g_FontDisplay, H / S * 1.1f);
    ImVec2 ts = ImGui::CalcTextSize(Text);
    Dl->AddText(ImVec2(BarX + BarW + 10 * S, P.y + (H - ts.y) * 0.5f), IM_COL32(255, 255, 255, 240), Text);
    ImGui::PopFont();
}

static std::string GetSkinName(AFortPlayerControllerAthena* PlayerController)
{
    std::string Out;
    auto CID = PlayerController->HasCosmeticLoadoutPC() ? PlayerController->CosmeticLoadoutPC.Character : nullptr;
    if (CID && CID->HasDisplayName())
        Out = UKismetTextLibrary::Conv_TextToString(CID->DisplayName).ToString().c_str();
    return Out;
}

struct FPlayerView
{
    AFortPlayerControllerAthena* PC = nullptr;
    std::string Name;
    std::string Skin;
    int Team = 0;
    bool bHasTeam = false;
    int Elims = 0;
    bool bAlive = false;
    float Health = 0.f, Shield = 0.f, Over = 0.f, OverMax = 0.f;
    bool bHasOver = false;
    double Loc[3] = {};
};

struct FMatchView
{
    bool bGameMode = false;
    int Alive = 0;
    int Secs = 0;
    std::string PlaylistName;
    std::string PlaylistSub;
    std::vector<FPlayerView> Players;
};

static SRWLOCK g_GameLock = SRWLOCK_INIT;
static std::vector<std::function<void()>> g_GameQueue;
static FMatchView g_MatchView;
static std::atomic<const UClass*> g_LlamaClass{ nullptr };
static std::atomic<const UClass*> g_SupplyClass{ nullptr };

static bool IsLivePC(AFortPlayerControllerAthena* PC)
{
    auto World = UWorld::GetWorld();
    auto NetDriver = World ? (UNetDriver*)World->NetDriver : nullptr;
    if (!NetDriver || !PC)
        return false;
    for (auto Conn : NetDriver->ClientConnections)
        if (Conn && Conn->PlayerController == PC)
            return true;
    return false;
}

static FMatchView GetMatchView()
{
    AcquireSRWLockShared(&g_GameLock);
    FMatchView Out = g_MatchView;
    ReleaseSRWLockShared(&g_GameLock);
    return Out;
}

void GUI::RunOnGameThread(std::function<void()> Fn)
{
    AcquireSRWLockExclusive(&g_GameLock);
    g_GameQueue.push_back(std::move(Fn));
    ReleaseSRWLockExclusive(&g_GameLock);
}

void GUI::GameThreadTick()
{
    std::vector<std::function<void()>> Jobs;
    AcquireSRWLockExclusive(&g_GameLock);
    Jobs.swap(g_GameQueue);
    ReleaseSRWLockExclusive(&g_GameLock);
    for (auto& Job : Jobs)
        Job();

    static auto NextView = std::chrono::steady_clock::time_point{};
    auto Now = std::chrono::steady_clock::now();
    if (Now < NextView || GUI::gsStatus == NotReady)
        return;
    NextView = Now + std::chrono::milliseconds(200);

    FMatchView View;
    auto World = UWorld::GetWorld();
    auto GameMode = World ? (AFortGameMode*)World->AuthorityGameMode : nullptr;
    if (GameMode && !GameMode->IsA<AFortGameModeAthena>())
        GameMode = nullptr;
    View.bGameMode = GameMode != nullptr;
    if (GameMode)
    {
        View.Alive = GameMode->HasAlivePlayers() ? GameMode->AlivePlayers.Num() : 0;
        View.Secs = (int)floor(UGameplayStatics::GetTimeSeconds(GameMode));
        auto GameState = GameMode->GameState;
        auto Playlist = GameState && VersionInfo.FortniteVersion >= 3.5 && GameMode->HasWarmupRequiredPlayerCount()
                            ? (GameState->HasCurrentPlaylistInfo() ? GameState->CurrentPlaylistInfo.BasePlaylist : GameState->CurrentPlaylistData)
                            : nullptr;
        if (Playlist)
        {
            View.PlaylistName = UKismetTextLibrary::Conv_TextToString(Playlist->UIDisplayName).ToString().c_str();
            if (Playlist->HasUIDisplaySubName())
                View.PlaylistSub = UKismetTextLibrary::Conv_TextToString(Playlist->UIDisplaySubName).ToString().c_str();
        }
    }

    static auto NextClassLookup = std::chrono::steady_clock::time_point{};
    if (World && (!g_LlamaClass || !g_SupplyClass) && Now >= NextClassLookup)
    {
        NextClassLookup = Now + std::chrono::seconds(5);
        if (!g_LlamaClass)
            g_LlamaClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/Llama/AthenaSupplyDrop_Llama.AthenaSupplyDrop_Llama_C");
        if (!g_SupplyClass)
            g_SupplyClass = FindObject<UClass>(L"/Game/Athena/SupplyDrops/AthenaSupplyDrop.AthenaSupplyDrop_C");
    }

    static std::vector<std::pair<AFortPlayerControllerAthena*, std::string>> SkinCache;
    std::vector<std::pair<AFortPlayerControllerAthena*, std::string>> KeptSkins;
    for (auto& P : CollectPlayers())
    {
        FPlayerView Pv;
        Pv.PC = P.PC;
        Pv.Name = P.Name;
        auto PS = (AFortPlayerStateAthena*)P.PC->PlayerState;
        Pv.bHasTeam = PS && PS->HasTeamIndex();
        Pv.Team = Pv.bHasTeam ? (int)PS->TeamIndex : 0;
        Pv.Elims = GetElims(PS);
        auto Pawn = (AFortPlayerPawnAthena*)P.PC->Pawn;
        Pv.bAlive = Pawn != nullptr;
        if (Pawn)
        {
            Pv.Over = PawnFloat(Pawn, "GetCurrentOvershield", &Pv.bHasOver);
            Pv.OverMax = Pv.bHasOver ? PawnFloat(Pawn, "GetMaxOvershield") : 0.f;
            Pv.Shield = Pawn->GetShield();
            Pv.Health = Pawn->GetHealth();
            auto L = Pawn->K2_GetActorLocation();
            Pv.Loc[0] = (double)L.X;
            Pv.Loc[1] = (double)L.Y;
            Pv.Loc[2] = (double)L.Z;
        }
        bool bCached = false;
        for (auto& Entry : SkinCache)
            if (Entry.first == P.PC)
            {
                Pv.Skin = Entry.second;
                bCached = true;
            }
        if (!bCached)
            Pv.Skin = GetSkinName(P.PC);
        if (!Pv.Skin.empty())
            KeptSkins.push_back({ P.PC, Pv.Skin });
        View.Players.push_back(std::move(Pv));
    }
    SkinCache = std::move(KeptSkins);

    AcquireSRWLockExclusive(&g_GameLock);
    g_MatchView = std::move(View);
    ReleaseSRWLockExclusive(&g_GameLock);
}

static void Bar(float Frac, ImVec4 Col, float Width, float Height)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(Width, Height));
    auto Dl = ImGui::GetWindowDrawList();
    Frac = Frac < 0.f ? 0.f : (Frac > 1.f ? 1.f : Frac);
    Dl->AddRectFilled(p, ImVec2(p.x + Width, p.y + Height), IM_COL32(0, 0, 0, 110), 2 * S);
    if (Frac > 0.f)
        Dl->AddRectFilled(p, ImVec2(p.x + Width * Frac, p.y + Height), U32(Col), 2 * S);
}

struct FMapTexture
{
    std::string Key;
    ID3D11ShaderResourceView* Srv = nullptr;
    std::string Source;
};
static FMapTexture g_MapTex;

struct FMapImage
{
    std::string Name;
    std::string Terrain;
    double Version = 0;
    const unsigned char* Data = nullptr;
    unsigned int Size = 0;
    std::string Path;
};
static std::vector<FMapImage> g_MapImages;
static bool g_MapImagesScanned = false;

static std::string LowerStr(std::string Text)
{
    for (auto& c : Text)
        c = (char)tolower((unsigned char)c);
    return Text;
}

static bool ParseMapImageName(const std::string& Name, FMapImage& Out)
{
    auto Dash = Name.find('-');
    if (Dash == std::string::npos || Dash == 0 || Dash + 1 >= Name.size())
        return false;
    Out.Name = Name;
    Out.Version = atof(Name.substr(0, Dash).c_str());
    Out.Terrain = Name.substr(Dash + 1);
    return Out.Version > 0;
}

static void ScanMapImages()
{
    g_MapImages.clear();
#ifdef BORON_EMBEDDED_ASSETS
    for (auto& Asset : BoronMapAssets)
    {
        FMapImage Img;
        if (Asset.Name && ParseMapImageName(Asset.Name, Img))
        {
            Img.Data = Asset.Data;
            Img.Size = Asset.Size;
            g_MapImages.push_back(Img);
        }
    }
#endif
    auto Dir = ZonePlanner::ModuleDir() + "Maps\\";
    WIN32_FIND_DATAA Fd;
    HANDLE Find = FindFirstFileA((Dir + "*.*").c_str(), &Fd);
    if (Find != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::string File = Fd.cFileName;
            auto Dot = File.find_last_of('.');
            if (Dot == std::string::npos)
                continue;
            auto Ext = LowerStr(File.substr(Dot));
            FMapImage Img;
            if ((Ext != ".png" && Ext != ".jpg" && Ext != ".jpeg") || !ParseMapImageName(File.substr(0, Dot), Img))
                continue;
            Img.Path = Dir + File;
            g_MapImages.erase(std::remove_if(g_MapImages.begin(), g_MapImages.end(), [&](const FMapImage& E) { return LowerStr(E.Name) == LowerStr(Img.Name); }), g_MapImages.end());
            g_MapImages.push_back(Img);
        } while (FindNextFileA(Find, &Fd));
        FindClose(Find);
    }
    std::sort(g_MapImages.begin(), g_MapImages.end(), [](const FMapImage& A, const FMapImage& B) { return A.Terrain != B.Terrain ? A.Terrain < B.Terrain : A.Version < B.Version; });
    g_MapImagesScanned = true;
}

static const FMapImage* PickMapImage(const std::string& Terrain, const std::string& Choice)
{
    if (!g_MapImagesScanned)
        ScanMapImages();
    if (Choice == "none")
        return nullptr;
    for (auto& Img : g_MapImages)
        if (Img.Name == Choice)
            return &Img;
    double V = VersionInfo.FortniteVersion;
    static const double Supported[] = { 1.72, 1.82, 2.42, 4.5, 14.60, 15.30, 21.40, 31.30, 31.41, 32.00, 32.11, 33.30, 34.40 };
    if (std::none_of(std::begin(Supported), std::end(Supported), [&](double S) { return fabs(S - V) < 0.001; }))
        return nullptr;
    auto Score = [&](const FMapImage& Img) { return Img.Version <= V + 0.001 ? 1e6 - (V - Img.Version) : -(Img.Version - V); };
    const FMapImage* Best = nullptr;
    auto Want = LowerStr(Terrain);
    for (auto& Img : g_MapImages)
        if (LowerStr(Img.Terrain) == Want && (!Best || Score(Img) > Score(*Best)))
            Best = &Img;
    return Best;
}

static ID3D11ShaderResourceView* GetMapTexture(const std::string& Terrain, const std::string& Choice, std::string& Source)
{
    auto Img = PickMapImage(Terrain, Choice);
    std::string Want = Img ? Img->Name + (Img->Path.empty() ? "#builtin" : "#folder") : "#none";
    if (g_MapTex.Key != Want)
    {
        if (g_MapTex.Srv)
            g_MapTex.Srv->Release();
        g_MapTex = FMapTexture{ Want };
        if (Img && g_Device)
        {
            g_MapTex.Srv = Img->Path.empty() ? LoadTexture(g_Device, nullptr, Img->Data, Img->Size, false) : LoadTexture(g_Device, Img->Path.c_str(), nullptr, 0, false);
            g_MapTex.Source = (Img->Path.empty() ? "Built-in " : "Maps\\") + Img->Name;
        }
    }
    Source = g_MapTex.Source;
    return g_MapTex.Srv;
}

struct FMapXf
{
    double U0, UX, UY, V0, VX, VY;
    FZoneAlign Al;
    double PanU, PanV, Zoom;
    ImVec2 Origin;
    float Size;

    static void Rot(double& u, double& v, int r)
    {
        for (int i = 0; i < (r & 3); i++)
        {
            double t = u;
            u = 1.0 - v;
            v = t;
        }
    }

    static void RotInv(double& u, double& v, int r)
    {
        for (int i = 0; i < (r & 3); i++)
        {
            double t = v;
            v = 1.0 - u;
            u = t;
        }
    }

    ImVec2 UvToScreen(double u, double v) const
    {
        return ImVec2(Origin.x + (float)(((u - PanU) * Zoom + 0.5) * Size), Origin.y + (float)(((v - PanV) * Zoom + 0.5) * Size));
    }

    void ScreenToUv(ImVec2 p, double& u, double& v) const
    {
        u = ((p.x - Origin.x) / Size - 0.5) / Zoom + PanU;
        v = ((p.y - Origin.y) / Size - 0.5) / Zoom + PanV;
    }

    ImVec2 ToScreen(double x, double y) const
    {
        double u = U0 + UX * x + UY * y, v = V0 + VX * x + VY * y;
        Rot(u, v, Al.Rot);
        return UvToScreen(0.5 + (u - 0.5) * Al.Scale + Al.OffU, 0.5 + (v - 0.5) * Al.Scale + Al.OffV);
    }

    FZonePoint ToWorld(ImVec2 p) const
    {
        double u, v;
        ScreenToUv(p, u, v);
        u = (u - Al.OffU - 0.5) / Al.Scale + 0.5;
        v = (v - Al.OffV - 0.5) / Al.Scale + 0.5;
        RotInv(u, v, Al.Rot);
        double Du = u - U0, Dv = v - V0, Det = UX * VY - UY * VX;
        if (fabs(Det) < 1e-20)
            return {};
        return { (Du * VY - UY * Dv) / Det, (UX * Dv - Du * VX) / Det };
    }

    float Len(double r) const
    {
        return (float)(r * sqrt(fabs(UX * VY - UY * VX)) * Al.Scale * Zoom * Size);
    }
};

static const FZonePoi* NearestPoi(const std::vector<FZonePoi>& Pois, double X, double Y, double* OutDist = nullptr)
{
    const FZonePoi* Best = nullptr;
    double BestD = 1e30;
    for (auto& Poi : Pois)
    {
        double D = (Poi.X - X) * (Poi.X - X) + (Poi.Y - Y) * (Poi.Y - Y);
        if (D < BestD)
        {
            BestD = D;
            Best = &Poi;
        }
    }
    if (OutDist)
        *OutDist = sqrt(BestD);
    return Best;
}

static void ShadowText(ImDrawList* Dl, ImVec2 P, ImU32 Col, const char* Text)
{
    Dl->AddText(ImVec2(P.x + 1, P.y + 1), IM_COL32(0, 0, 0, 200), Text);
    Dl->AddText(P, Col, Text);
}

static void DrawZoneChain(ImDrawList* Dl, const FMapXf& X, const std::vector<FZonePoint>& Chain, const std::vector<double>& Radii, int From, int Base, bool bDashed, float Fade)
{
    int N = (int)(std::min)(Chain.size(), Radii.size());
    if (From < 0 || From >= N)
        return;
    for (int i = From; i < N - 1; i++)
    {
        ImVec2 A = X.ToScreen(Chain[i].X, Chain[i].Y), B = X.ToScreen(Chain[i + 1].X, Chain[i + 1].Y);
        if (bDashed)
        {
            float Dx = B.x - A.x, Dy = B.y - A.y, L = sqrtf(Dx * Dx + Dy * Dy);
            for (float t = 0; t < L; t += 9 * S)
            {
                float t2 = (std::min)(L, t + 5 * S);
                Dl->AddLine(ImVec2(A.x + Dx * t / L, A.y + Dy * t / L), ImVec2(A.x + Dx * t2 / L, A.y + Dy * t2 / L), IM_COL32(255, 255, 255, (int)(170 * Fade)), 1.5f * S);
            }
        }
        else
            Dl->AddLine(A, B, IM_COL32(255, 255, 255, (int)(130 * Fade)), 1.5f * S);
    }
    for (int i = From; i < N; i++)
    {
        float t = N - 1 > From ? (float)(i - From) / (float)(N - 1 - From) : 1.f;
        ImVec2 C = X.ToScreen(Chain[i].X, Chain[i].Y);
        float R = X.Len(Radii[i]);
        ImVec4 Col = Mix(ImVec4(1, 1, 1, 1), kYellow, t);
        if (i == N - 1)
        {
            Dl->AddCircleFilled(C, (std::max)(R, 4.f * S), U32(Alpha(kYellow, 0.28f * Fade)), 48);
            Dl->AddCircle(C, (std::max)(R, 4.f * S), U32(Alpha(kYellow, 0.95f * Fade)), 48, 2.f * S);
        }
        else if (R > 2.f)
            Dl->AddCircle(C, R, U32(Alpha(Col, (bDashed ? 0.55f : 0.8f) * Fade)), 96, (bDashed ? 1.2f : 1.6f) * S);
        if (R > 14 * S && i < N - 1)
        {
            char Num[8];
            sprintf_s(Num, "%d", i - Base + 1);
            ImGui::PushFont(g_FontBold, 13.f);
            ImVec2 Ts = ImGui::CalcTextSize(Num);
            ShadowText(Dl, ImVec2(C.x - Ts.x * 0.5f, C.y - (std::max)(R, 4.f * S) - Ts.y - 1 * S), U32(Alpha(Col, Fade)), Num);
            ImGui::PopFont();
        }
    }
}

static bool PlannerButton(const char* Id, const char* Label, ImVec2 Size, ImVec4 Fill, bool bPrimary, int Icon = -1)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool bPressed = ImGui::InvisibleButton(Id, Size);
    bool bDisabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
    float t = Anim(ImGui::GetItemID(), ImGui::IsItemHovered() && !bDisabled ? 1.f : 0.f, 16.f);
    auto Dl = ImGui::GetWindowDrawList();
    ImVec2 q(p.x + Size.x, p.y + Size.y);
    float R = 6 * S;
    ImVec4 Bg = bPrimary ? Mix(Fill, ImVec4(1, 1, 1, 1), 0.12f * t) : ImVec4(1, 1, 1, 0.05f + 0.07f * t);
    if (ImGui::IsItemActive() && !bDisabled)
        Bg = Mix(Bg, ImVec4(0, 0, 0, Bg.w), 0.18f);
    if (bDisabled)
        Bg.w *= 0.4f;
    Dl->AddRectFilled(p, q, U32(Bg), R);
    if (!bPrimary)
        Dl->AddRect(p, q, U32(ImVec4(1, 1, 1, 0.10f + 0.14f * t)), R);
    ImGui::PushFont(g_FontBold, 14.f);
    std::string T = Upper(Label);
    ImVec2 Ts = ImGui::CalcTextSize(T.c_str());
    float Ic = Icon >= 0 && g_Icons[Icon] ? 15 * S : 0.f;
    float Gap = Ic > 0 ? 7 * S : 0.f;
    float X = p.x + (Size.x - Ts.x - Ic - Gap) * 0.5f, Cy = p.y + Size.y * 0.5f;
    ImU32 Tc = IM_COL32(255, 255, 255, bDisabled ? 110 : 245);
    if (Ic > 0)
        DrawIcon(Dl, (EIcon)Icon, ImVec2(X, Cy - Ic * 0.5f), Ic, Tc);
    CenteredGlyphText(Dl, ImVec2(X + Ic + Gap + Ts.x * 0.5f, Cy), Tc, T.c_str());
    ImGui::PopFont();
    return bPressed && !bDisabled;
}

static void PlannerStatus(ImVec4 Col, const char* Text, float W)
{
    ImGui::PushFont(nullptr, 13.f);
    float Wrap = W - 26 * S;
    ImVec2 Ts = ImGui::CalcTextSize(Text, nullptr, false, Wrap);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float H = Ts.y + 14 * S;
    auto Dl = ImGui::GetWindowDrawList();
    Dl->AddRectFilled(p, ImVec2(p.x + W, p.y + H), U32(Alpha(Col, 0.10f)), 6 * S);
    Dl->AddRectFilled(p, ImVec2(p.x + 3 * S, p.y + H), U32(Col), 6 * S, ImDrawFlags_RoundCornersLeft);
    Dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p.x + 14 * S, p.y + 7 * S), U32(Mix(Col, ImVec4(1, 1, 1, 1), 0.4f)), Text, nullptr, Wrap);
    ImGui::Dummy(ImVec2(W, H));
    ImGui::PopFont();
}

static void ZonePlannerPanel()
{
    static std::string PinsKey = "#";
    static FZonePins Pins;
    static FZoneSettings Set = ZonePlanner::GetSettings();
    static int DragMode = 0;
    static int DragCircle = -1;
    static int Selected = -1;
    static bool bShowNames = true;
    static ImVec2 DragStart;
    static double Zoom = 1.0, ZoomTarget = 1.0, PanU = 0.5, PanV = 0.5;
    static double AnchorU = 0.5, AnchorV = 0.5;
    static ImVec2 AnchorScreen;
    static char PoiFilter[48] = "";

    auto V = ZonePlanner::GetView();
    if (V.MapKey != PinsKey)
    {
        PinsKey = V.MapKey;
        Pins = ZonePlanner::GetPins(PinsKey);
        DragMode = 0;
        Zoom = ZoomTarget = 1.0;
        PanU = PanV = 0.5;
    }
    bool bPinsDirty = false, bSetDirty = false;
    bool bMatch = GUI::gsStatus == StartedMatch;

    BeginPanel("lgplanner", "Zone planner", IcoPlace);
    bool bLoaded = V.bWorld && V.Radii.size() >= 2;
    bool bNoMap = bLoaded && !PickMapImage(V.MapKey, "");
    ZonePlanner::SetBlocked(bNoMap);
    const char* Why = !ZonePlanner::IsSupported() ? "Not available on this version yet."
                      : bNoMap                     ? "No map for this version yet, so the planner is off."
                      : !LategameConfig::bLateGame ? "Turn LateGame on to pick where the zones go."
                                                   : nullptr;
    if (Why)
    {
        bool bOff = false;
        ImGui::BeginDisabled();
        Toggle("Custom zones", &bOff);
        ImGui::EndDisabled();
        ImGui::TextDisabled("%s", Why);
        EndPanel();
        return;
    }
    if (!bLoaded)
    {
        if (Toggle("Custom zones", &Set.bEnabled, "Off = zones are random like normal"))
            ZonePlanner::SetSettings(Set);
        ImGui::TextDisabled("The map shows up here once the server has loaded the world.");
        EndPanel();
        return;
    }
    if (!Set.bEnabled)
    {
        if (Toggle("Custom zones", &Set.bEnabled, "Off = zones are random like normal"))
            ZonePlanner::SetSettings(Set);
        ImGui::TextDisabled("%s", bMatch ? "Zones are random this match. Turn this on to plan the next one." : "Zones are random. Turn this on to pick where they go.");
        EndPanel();
        return;
    }

    int N = (int)V.Radii.size();
    int S0 = (std::clamp)(V.StartPhase, 0, N - 1);
    int M = N - S0;
    int Cur = bMatch ? V.CurrentPhase : -1;
    if (Cur >= 0 && Cur < S0)
        Cur = S0;
    if (Selected < 0 || Selected >= M)
        Selected = M - 1;

    auto HasPin = [&](int k) { return k == 0 ? Pins.bStart : k == M - 1 ? Pins.bEnd : Pins.Phase.count(k) > 0; };
    auto SetPin = [&](int k, FZonePoint P)
    {
        if (k == 0)
        {
            Pins.bStart = true;
            Pins.Start = P;
        }
        else if (k == M - 1)
        {
            Pins.bEnd = true;
            Pins.End = P;
        }
        else
            Pins.Phase[k] = P;
    };
    auto ClearPin = [&](int k)
    {
        if (k == 0)
            Pins.bStart = false;
        else if (k == M - 1)
            Pins.bEnd = false;
        else
            Pins.Phase.erase(k);
    };
    auto Editable = [&](int k) { return !bMatch; };

    std::vector<FZonePoint> Plan;
    bool bPulled = false;
    int PlanFrom = S0;
    if (bMatch && Cur >= 0 && Cur < (int)V.Actual.size() && Cur < N)
    {
        auto Live = ZonePlanner::Rebase(Pins, S0, Cur);
        Live.bStart = true;
        Live.Start = V.Actual[Cur];
        Plan = ZonePlanner::BuildChain(Live, false, V.Radii, Cur, V.Actual, V.Pois, V.MapCenter, &bPulled);
        PlanFrom = Cur;
    }
    else if (!bMatch)
        Plan = ZonePlanner::BuildChain(Pins, false, V.Radii, S0, V.Actual, V.Pois, V.MapCenter, &bPulled);

    float Avail = ImGui::GetContentRegionAvail().x;
    bool bWide = Avail > 760 * S;
    float MapSize = bWide ? (std::min)(Avail - 300 * S, 560 * S) : Avail;
    auto Al = ZonePlanner::GetAlign(V.MapKey);

    FMapXf X{ V.U0, V.UX, V.UY, V.V0, V.VX, V.VY, Al, PanU, PanV, Zoom, ImGui::GetCursorScreenPos(), MapSize };
    if (!V.ProjSource)
    {
        double E = V.Radii[0] * 1.08;
        X.UX = 0;
        X.UY = 1.0 / (2 * E);
        X.U0 = -(V.MapCenter.Y - E) / (2 * E);
        X.VX = -1.0 / (2 * E);
        X.VY = 0;
        X.V0 = (V.MapCenter.X + E) / (2 * E);
    }

    ImVec2 P0 = X.Origin, P1(P0.x + MapSize, P0.y + MapSize);
    ImGui::InvisibleButton("##zonemap", ImVec2(MapSize, MapSize), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool bHover = ImGui::IsItemHovered();
    if (bHover)
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    auto& Io = ImGui::GetIO();
    auto Dl = ImGui::GetWindowDrawList();

    auto ZoomAt = [&](double Factor, ImVec2 At)
    {
        X.ScreenToUv(At, AnchorU, AnchorV);
        AnchorScreen = At;
        ZoomTarget = (std::clamp)(ZoomTarget * Factor, 1.0, 10.0);
    };

    float Bs = 26 * S;
    ImVec2 BtnPos[3] = { ImVec2(P1.x - Bs - 8 * S, P0.y + 8 * S), ImVec2(P1.x - Bs - 8 * S, P0.y + 12 * S + Bs), ImVec2(P1.x - Bs - 8 * S, P0.y + 16 * S + Bs * 2) };
    int BtnHover = -1;
    for (int b = 0; b < 3; b++)
        if (bHover && Io.MousePos.x >= BtnPos[b].x && Io.MousePos.x <= BtnPos[b].x + Bs && Io.MousePos.y >= BtnPos[b].y && Io.MousePos.y <= BtnPos[b].y + Bs)
            BtnHover = b;

    if (bHover && Io.MouseWheel != 0.f)
        ZoomAt(pow(1.35, (double)Io.MouseWheel), Io.MousePos);

    auto Snap = [&](ImVec2 Mouse, const FZonePoi** OutPoi = nullptr) -> FZonePoint
    {
        auto W = X.ToWorld(Mouse);
        const FZonePoi* Best = nullptr;
        float BestD = 16 * S * 16 * S;
        for (auto& Poi : V.Pois)
        {
            ImVec2 C = X.ToScreen(Poi.X, Poi.Y);
            float D = (C.x - Mouse.x) * (C.x - Mouse.x) + (C.y - Mouse.y) * (C.y - Mouse.y);
            if (D < BestD)
            {
                BestD = D;
                Best = &Poi;
            }
        }
        if (OutPoi)
            *OutPoi = Best;
        return Best ? FZonePoint{ Best->X, Best->Y } : W;
    };

    auto HandlePos = [&](int k) -> ImVec2
    {
        int i = S0 + k;
        if (i < (int)Plan.size())
            return X.ToScreen(Plan[i].X, Plan[i].Y);
        return ImVec2(-1e9f, -1e9f);
    };
    int HoverCircle = -1;
    if (bHover && BtnHover < 0)
    {
        float BestD = 13 * S * 13 * S;
        for (int k = M - 1; k >= 0; k--)
        {
            if (S0 + k < PlanFrom || !Editable(k))
                continue;
            ImVec2 H = HandlePos(k);
            float Dx = H.x - Io.MousePos.x, Dy = H.y - Io.MousePos.y;
            if (Dx * Dx + Dy * Dy < BestD)
            {
                BestD = Dx * Dx + Dy * Dy;
                HoverCircle = k;
            }
        }
    }

    if (bHover && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (BtnHover == 0)
            ZoomAt(1.6, ImVec2((P0.x + P1.x) * 0.5f, (P0.y + P1.y) * 0.5f));
        else if (BtnHover == 1)
            ZoomAt(1.0 / 1.6, ImVec2((P0.x + P1.x) * 0.5f, (P0.y + P1.y) * 0.5f));
        else if (BtnHover == 2)
        {
            ZoomTarget = 1.0;
            AnchorU = PanU;
            AnchorV = PanV;
            AnchorScreen = ImVec2((P0.x + P1.x) * 0.5f, (P0.y + P1.y) * 0.5f);
        }
        else if (HoverCircle >= 0)
        {
            DragMode = 3;
            DragCircle = HoverCircle;
            Selected = HoverCircle;
        }
        else
        {
            DragMode = 1;
            DragStart = Io.MousePos;
        }
    }
    if (bHover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && BtnHover < 0 && HoverCircle < 0)
        ZoomAt(2.0, Io.MousePos);
    if (DragMode == 1 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float Dx = Io.MousePos.x - DragStart.x, Dy = Io.MousePos.y - DragStart.y;
        if (Dx * Dx + Dy * Dy > 5 * S * 5 * S)
            DragMode = 2;
    }
    if ((DragMode == 2 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.f)))
    {
        PanU -= Io.MouseDelta.x / (MapSize * Zoom);
        PanV -= Io.MouseDelta.y / (MapSize * Zoom);
        AnchorScreen = ImVec2(-1e9f, -1e9f);
    }
    if (DragMode == 3 && DragCircle >= 0)
        SetPin(DragCircle, Snap(Io.MousePos));
    if (DragMode != 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (DragMode == 1 && bHover && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && Editable(Selected))
        {
            SetPin(Selected, Snap(Io.MousePos));
            bPinsDirty = true;
        }
        if (DragMode == 3)
            bPinsDirty = true;
        DragMode = 0;
        DragCircle = -1;
    }
    if (bHover && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && HoverCircle >= 0 && HasPin(HoverCircle))
    {
        ClearPin(HoverCircle);
        bPinsDirty = true;
    }

    if (fabs(Zoom - ZoomTarget) > 1e-4)
    {
        double Next = Zoom + (ZoomTarget - Zoom) * (std::min)(1.0, (double)Io.DeltaTime * 14.0);
        if (fabs(Next - ZoomTarget) < 1e-3)
            Next = ZoomTarget;
        Zoom = Next;
        X.Zoom = Zoom;
        if (AnchorScreen.x > -1e8f)
        {
            PanU = AnchorU - ((AnchorScreen.x - P0.x) / MapSize - 0.5) / Zoom;
            PanV = AnchorV - ((AnchorScreen.y - P0.y) / MapSize - 0.5) / Zoom;
        }
    }
    PanU = (std::clamp)(PanU, 0.5 / Zoom, 1.0 - 0.5 / Zoom);
    PanV = (std::clamp)(PanV, 0.5 / Zoom, 1.0 - 0.5 / Zoom);
    X.PanU = PanU;
    X.PanV = PanV;
    X.Zoom = Zoom;

    std::string ImageSource;
    auto Tex = GetMapTexture(V.MapKey, Al.Image, ImageSource);
    Dl->PushClipRect(P0, P1, true);
    Dl->AddRectFilled(P0, P1, IM_COL32(10, 22, 52, 255), 6 * S);
    if (Tex)
        Dl->AddImage(ImTextureRef((ImTextureID)(uintptr_t)Tex), X.UvToScreen(0, 0), X.UvToScreen(1, 1));

    if (V.bHasStorm)
    {
        ImVec2 C = X.ToScreen(V.StormCenter.X, V.StormCenter.Y);
        float R = X.Len(V.StormRadius);
        float Th = MapSize * 3.f * (float)Zoom;
        Dl->AddCircle(C, R + Th * 0.5f, IM_COL32(110, 40, 190, 70), 128, Th);
        Dl->AddCircle(C, R, U32(kBlue), 128, 2.5f * S);
    }

    if (bMatch && !V.Actual.empty())
        DrawZoneChain(Dl, X, V.Actual, V.Radii, (std::max)(S0, (std::max)(Cur, 0)), S0, false, 0.45f);
    if (!Plan.empty() && Set.bEnabled)
        DrawZoneChain(Dl, X, Plan, V.Radii, PlanFrom, S0, bMatch, 1.f);

    if (bShowNames)
    {
        ImGui::PushFont(g_FontBold, 11.f + 2.f * (float)(std::min)(Zoom, 3.0));
        std::vector<ImVec4> Placed;
        auto Overlaps = [&](ImVec2 P, ImVec2 Ts)
        {
            for (auto& R : Placed)
                if (P.x < R.z && P.x + Ts.x > R.x && P.y < R.w && P.y + Ts.y > R.y)
                    return true;
            return false;
        };
        for (auto& Poi : V.Pois)
        {
            ImVec2 C = X.ToScreen(Poi.X, Poi.Y);
            std::string Name = Upper(Poi.Name.c_str());
            ImVec2 Ts = ImGui::CalcTextSize(Name.c_str());
            ImVec2 P(IM_ROUND(C.x - Ts.x * 0.5f), IM_ROUND(C.y + 3 * S));
            if (Overlaps(P, Ts))
                P.y = IM_ROUND(C.y - 3 * S - Ts.y);
            if (Overlaps(P, Ts))
                continue;
            Placed.push_back(ImVec4(P.x - 2 * S, P.y, P.x + Ts.x + 2 * S, P.y + Ts.y));
            Dl->AddCircleFilled(C, 2.f * S, IM_COL32(255, 255, 255, 200), 8);
            ShadowText(Dl, P, IM_COL32(255, 255, 255, 215), Name.c_str());
        }
        ImGui::PopFont();
    }

    for (auto& Dot : V.Dots)
    {
        ImVec2 C = X.ToScreen(Dot.X, Dot.Y);
        if (Dot.bBot)
        {
            Dl->AddCircleFilled(C, 3.f * S, IM_COL32(20, 20, 20, 200), 12);
            Dl->AddCircle(C, 3.f * S, IM_COL32(220, 220, 220, 230), 12, 1.2f * S);
        }
        else
        {
            Dl->AddCircleFilled(C, 4.5f * S, IM_COL32(0, 0, 0, 220), 16);
            Dl->AddCircleFilled(C, 3.3f * S, U32(TeamColor(Dot.Team)), 16);
        }
    }

    if (V.bHasBus)
    {
        ImVec2 C = X.ToScreen(V.Bus.X, V.Bus.Y);
        Dl->AddCircleFilled(C, 11 * S, IM_COL32(0, 0, 0, 160), 20);
        DrawIcon(Dl, IcoBus, ImVec2(C.x - 8 * S, C.y - 8 * S), 16 * S, U32(kYellow));
    }

    if (Set.bEnabled)
        for (int k = 0; k < M; k++)
        {
            if (S0 + k < PlanFrom || S0 + k >= (int)Plan.size())
                continue;
            ImVec2 H = HandlePos(k);
            bool bPinned = HasPin(k);
            bool bSel = k == Selected;
            bool bHot = k == HoverCircle || k == DragCircle;
            bool bLocked = !Editable(k);
            ImVec4 Col = k == 0 ? kGreen : k == M - 1 ? kYellow : ImVec4(1, 1, 1, 1);
            float R = (bHot ? 10.f : bSel ? 9.f : 7.5f) * S;
            if (bSel)
                Dl->AddCircle(H, R + 4 * S, U32(Alpha(Col, 0.8f)), 24, 2.f * S);
            Dl->AddCircleFilled(H, R, bPinned ? U32(Alpha(Col, bLocked ? 0.5f : 1.f)) : IM_COL32(0, 0, 0, 170), 24);
            Dl->AddCircle(H, R, U32(Alpha(Col, bLocked ? 0.4f : 0.95f)), 24, 1.5f * S);
            char Num[8];
            if (k == M - 1)
                strcpy_s(Num, "F");
            else
                sprintf_s(Num, "%d", k + 1);
            ImGui::PushFont(g_FontBold, R * 1.25f / S);
            CenteredGlyphText(Dl, H, bPinned ? U32(kInk) : U32(Col), Num);
            ImGui::PopFont();
        }

    if (bHover && DragMode == 0 && BtnHover < 0 && HoverCircle < 0)
    {
        const FZonePoi* Poi = nullptr;
        Snap(Io.MousePos, &Poi);
        if (Poi)
            Dl->AddCircle(X.ToScreen(Poi->X, Poi->Y), 9 * S, U32(kYellow), 24, 2.f * S);
    }

    ImGui::PushFont(nullptr, 12.f);
    char Help[160];
    sprintf_s(Help, "Click: place circle %s   Drag a number: move it   Right click a number: unpin", Selected == M - 1 ? "F" : std::to_string(Selected + 1).c_str());
    ImVec2 Hs = ImGui::CalcTextSize(Help);
    Dl->AddRectFilled(ImVec2(P0.x + 8 * S, P1.y - Hs.y - 14 * S), ImVec2(P0.x + Hs.x + 18 * S, P1.y - 8 * S), IM_COL32(0, 0, 0, 160), 4 * S);
    Dl->AddText(ImVec2(P0.x + 13 * S, P1.y - Hs.y - 11 * S), IM_COL32(255, 255, 255, 215), Help);
    ImGui::PopFont();

    const char* BtnLabels[3] = { "+", "-", "FIT" };
    for (int b = 0; b < 3; b++)
    {
        ImVec2 A = BtnPos[b], B(A.x + Bs, A.y + Bs);
        Dl->AddRectFilled(A, B, BtnHover == b ? IM_COL32(255, 255, 255, 60) : IM_COL32(0, 0, 0, 160), 5 * S);
        Dl->AddRect(A, B, IM_COL32(255, 255, 255, 50), 5 * S);
        ImGui::PushFont(g_FontBold, b == 2 ? 11.f : 18.f);
        CenteredGlyphText(Dl, ImVec2(A.x + Bs * 0.5f, A.y + Bs * 0.5f), IM_COL32(255, 255, 255, 230), BtnLabels[b]);
        ImGui::PopFont();
    }
    if (Zoom > 1.01)
    {
        ImVec2 Mp(P1.x - 8 * S - 70 * S, P1.y - 8 * S - 70 * S), Mq(P1.x - 8 * S, P1.y - 8 * S);
        Dl->AddRectFilled(Mp, Mq, IM_COL32(0, 0, 0, 170), 4 * S);
        if (Tex)
            Dl->AddImage(ImTextureRef((ImTextureID)(uintptr_t)Tex), Mp, Mq, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 180));
        float Hw = (float)(0.5 / Zoom) * 70 * S;
        ImVec2 Vc(Mp.x + (float)PanU * 70 * S, Mp.y + (float)PanV * 70 * S);
        Dl->AddRect(ImVec2(Vc.x - Hw, Vc.y - Hw), ImVec2(Vc.x + Hw, Vc.y + Hw), U32(kYellow), 2 * S, 0, 1.5f * S);
    }
    if (bHover && (BtnHover >= 0 || HoverCircle >= 0))
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    else if (DragMode == 2)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    Dl->PopClipRect();
    Dl->AddRect(P0, P1, IM_COL32(255, 255, 255, 40), 6 * S, 0, 1.5f * S);

    if (bWide)
        ImGui::SameLine(0, 18 * S);
    ImGui::BeginGroup();
    float Cw = bWide ? (std::max)(240 * S, Avail - MapSize - 18 * S) : Avail;
    ImGui::PushID("planner");

    if (Toggle("Custom zones", &Set.bEnabled, "Off = zones are random like normal"))
        bSetDirty = true;
    Toggle("Place names", &bShowNames);
    ImGui::BeginDisabled(bMatch);
    if (Toggle("Free placement", &Pins.bFree, "On: circles can sit outside the one before (the storm slides across the map). Off: every circle stays inside the last one, like real Fortnite"))
        bPinsDirty = true;
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0, 4 * S));

    ImGui::PushFont(g_FontBold, 12.f);
    ImGui::TextColored(Alpha(kMuted, 0.9f), "CIRCLES");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::PushFont(nullptr, 12.f);
    ImGui::TextColored(Alpha(kMuted, 0.7f), "pick one, then click the map");
    ImGui::PopFont();
    float ListH = (std::min)((float)M * ImGui::GetFrameHeight() + 8 * S, 230 * S);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6 * S);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4 * S, 4 * S));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 0.08f));
    ImGui::BeginChild("##circles", ImVec2(Cw, ListH + 8 * S), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    float RowW = ImGui::GetContentRegionAvail().x;
    for (int k = 0; k < M; k++)
    {
        ImGui::PushID(k);
        bool bLocked = !Editable(k);
        ImVec4 Col = k == 0 ? kGreen : k == M - 1 ? kYellow : ImVec4(0.85f, 0.88f, 0.95f, 1.f);
        char Label[32];
        if (k == 0)
            strcpy_s(Label, "First");
        else if (k == M - 1)
            strcpy_s(Label, "Final");
        else
            sprintf_s(Label, "Circle %d", k + 1);
        std::string Where = "auto";
        if (HasPin(k))
        {
            FZonePoint Pt = k == 0 ? Pins.Start : k == M - 1 ? Pins.End : Pins.Phase[k];
            double D = 0;
            auto Poi = NearestPoi(V.Pois, Pt.X, Pt.Y, &D);
            Where = Poi && D < 25000 ? Poi->Name : "pinned";
        }
        ImGui::BeginDisabled(bLocked);
        float Fh = ImGui::GetFrameHeight();
        ImVec2 Rp = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##row", Selected == k, ImGuiSelectableFlags_AllowOverlap, ImVec2(RowW, Fh)))
            Selected = k;
        auto Rl = ImGui::GetWindowDrawList();
        float Ry = Rp.y + Fh * 0.5f;
        ImGui::PushFont(g_FontBold, 13.5f);
        ImVec2 Ls = ImGui::CalcTextSize(Label);
        CenteredGlyphText(Rl, ImVec2(Rp.x + 8 * S + Ls.x * 0.5f, Ry), U32(Col), Label);
        ImGui::PopFont();
        char Radius[24];
        sprintf_s(Radius, "%.0fm", V.Radii[S0 + k] / 100.0);
        ImGui::PushFont(nullptr, 12.f);
        ImVec2 Rs = ImGui::CalcTextSize(Radius);
        CenteredGlyphText(Rl, ImVec2(Rp.x + 80 * S + Rs.x * 0.5f, Ry), U32(Alpha(kMuted, 0.8f)), Radius);
        ImGui::PopFont();
        ImGui::PushFont(nullptr, 13.5f);
        ImVec2 Ws = ImGui::CalcTextSize(Where.c_str());
        CenteredGlyphText(Rl, ImVec2(Rp.x + 132 * S + Ws.x * 0.5f, Ry), HasPin(k) ? IM_COL32(255, 255, 255, 240) : U32(Alpha(kMuted, 0.75f)), Where.c_str());
        ImGui::PopFont();
        if (HasPin(k))
        {
            ImGui::SameLine(RowW - Fh + 6 * S);
            ImVec2 Xp = ImGui::GetCursorScreenPos();
            Xp.y = Rp.y;
            ImGui::SetCursorScreenPos(Xp);
            float Xs = Fh - 6 * S;
            ImGui::SetCursorScreenPos(ImVec2(Xp.x, Rp.y + 3 * S));
            if (ImGui::InvisibleButton("##unpin", ImVec2(Xs, Xs)))
            {
                ClearPin(k);
                bPinsDirty = true;
            }
            bool bXHot = ImGui::IsItemHovered();
            ImVec2 Xa(Xp.x, Rp.y + 3 * S), Xb(Xp.x + Xs, Rp.y + 3 * S + Xs);
            Rl->AddRectFilled(Xa, Xb, bXHot ? U32(Alpha(kRed, 0.85f)) : IM_COL32(255, 255, 255, 22), 4 * S);
            ImVec2 Xc((Xa.x + Xb.x) * 0.5f, (Xa.y + Xb.y) * 0.5f);
            float Xr = Xs * 0.18f;
            Rl->AddLine(ImVec2(Xc.x - Xr, Xc.y - Xr), ImVec2(Xc.x + Xr, Xc.y + Xr), IM_COL32(255, 255, 255, bXHot ? 255 : 190), 1.6f * S);
            Rl->AddLine(ImVec2(Xc.x - Xr, Xc.y + Xr), ImVec2(Xc.x + Xr, Xc.y - Xr), IM_COL32(255, 255, 255, bXHot ? 255 : 190), 1.6f * S);
            if (bXHot)
                ImGui::SetTooltip("Unpin");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0, 4 * S));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6 * S);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * S, 8 * S));
    ImGui::SetNextItemWidth(Cw);
    char PoiHint[64];
    sprintf_s(PoiHint, "Put %s at a place...", Selected == M - 1 ? "the final circle" : Selected == 0 ? "the first circle" : ("circle " + std::to_string(Selected + 1)).c_str());
    if (ImGui::BeginCombo("##poi", PoiHint, ImGuiComboFlags_HeightLarge))
    {
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##poif", "Search places", PoiFilter, sizeof(PoiFilter));
        auto Filter = LowerStr(PoiFilter);
        for (auto& Poi : V.Pois)
        {
            if (!Filter.empty() && LowerStr(Poi.Name).find(Filter) == std::string::npos)
                continue;
            if (ImGui::Selectable(Poi.Name.c_str()) && Editable(Selected))
            {
                SetPin(Selected, { Poi.X, Poi.Y });
                bPinsDirty = true;
            }
        }
        if (V.Pois.empty())
            ImGui::TextDisabled("No named places found on this map");
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0, 4 * S));
    float Bw = (Cw - 8 * S) * 0.5f;
    ImGui::BeginDisabled(bMatch);
    if (PlannerButton("##reroll", "Reroll path", ImVec2(Bw, 34 * S), ImVec4(), false))
    {
        Pins.Seed = (uint32_t)(GetTickCount64() * 2654435761ull) | 1u;
        bPinsDirty = true;
    }
    Hint("Same pins, new random path for the circles you did not pin");
    ImGui::SameLine(0, 8 * S);
    if (PlannerButton("##clearall", "Clear all", ImVec2(Bw, 34 * S), ImVec4(), false))
    {
        for (int k = 0; k < M; k++)
            if (Editable(k))
                ClearPin(k);
        bPinsDirty = true;
    }
    ImGui::EndDisabled();
    if (bMatch)
    {
        ImGui::Dummy(ImVec2(0, 2 * S));
        ImGui::TextColored(kYellow, "Locked: the bus already left. Changes apply next match.");
    }

    ImGui::Dummy(ImVec2(0, 6 * S));
    if (!Set.bEnabled)
        PlannerStatus(kMuted, "Custom zones are off, zones stay random.", Cw);
    else if (!bMatch)
        PlannerStatus(kGreen, "Ready for the next match.", Cw);
    else if (!V.LastEvent.empty())
        PlannerStatus(kGreen, V.LastEvent.c_str(), Cw);
    if (bPulled)
    {
        ImGui::Dummy(ImVec2(0, 2 * S));
        PlannerStatus(kYellow, Pins.bFree ? "A pinned circle could not be reached." : "A pinned circle is too far from the one before it, so it gets pulled closer. Turn on Free placement to keep it exactly.", Cw);
    }

    ImGui::PopID();
    ImGui::EndGroup();
    EndPanel();

    if (bSetDirty)
        ZonePlanner::SetSettings(Set);
    if (bPinsDirty)
        ZonePlanner::SetPins(PinsKey, Pins);
}

void GUI::Init()
{
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
    S = main_scale;

    WNDCLASS wc{};
    wc.lpszClassName = L"BoronWC";
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    wchar_t buffer[67];
    swprintf_s(buffer,
               VersionInfo.EngineVersion >= 5.0 ? L"Boron (FN %.2f, UE %.1f)"
                                                : (VersionInfo.FortniteVersion >= 5.00 || VersionInfo.FortniteVersion < 1.2 ? L"Boron (FN %.2f, UE %.2f)" : L"Boron (FN %.1f, UE %.2f)"),
               VersionInfo.FortniteVersion, VersionInfo.EngineVersion);
    auto hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_APPWINDOW, wc.lpszClassName, buffer, WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, 100, 100, (int)(WindowWidth * main_scale), (int)(WindowHeight * main_scale), nullptr, nullptr,
                               nullptr, nullptr);
    g_hWnd = hWnd;
    SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
    MARGINS Margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hWnd, &Margins);
    BOOL bDark = TRUE;
    DwmSetWindowAttribute(hWnd, 20, &bDark, sizeof(bDark));
    int Corners = 2;
    DwmSetWindowAttribute(hWnd, 33, &Corners, sizeof(Corners));

    IDXGISwapChain* g_pSwapChain = nullptr;
    ID3D11Device* g_pd3dDevice = nullptr;
    ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    HRESULT res =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res =
            D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
    {
        printf("[Boron][GUI] D3D11CreateDeviceAndSwapChain failed hr=0x%08X - GUI disabled for this run (fn=%.2f ue=%.2f)\n",
               (unsigned int)res, VersionInfo.FortniteVersion, VersionInfo.EngineVersion);
        return;
    }

    ID3D11RenderTargetView* g_mainRenderTargetView;

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);
    DWORD dwMyID = ::GetCurrentThreadId();
    DWORD dwCurID = ::GetWindowThreadProcessId(hWnd, NULL);
    AttachThreadInput(dwCurID, dwMyID, TRUE);
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);
    SetActiveWindow(hWnd);
    AttachThreadInput(dwCurID, dwMyID, FALSE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImFontConfig FontConfig;
    FontConfig.FontDataOwnedByAtlas = false;
    char BaseDir[MAX_PATH] = { 0 };
    HMODULE Self = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&ApplyColors, &Self) && GetModuleFileNameA(Self, BaseDir, MAX_PATH))
    {
        if (auto Slash = strrchr(BaseDir, '\\'))
            *(Slash + 1) = 0;
    }
    auto LoadFontFile = [&](const char* Name, float Size) -> ImFont*
    {
        char Path[MAX_PATH];
        sprintf_s(Path, "%sFonts\\%s", BaseDir, Name);
        if (GetFileAttributesA(Path) == INVALID_FILE_ATTRIBUTES)
            return nullptr;
        return ImGui::GetIO().Fonts->AddFontFromFileTTF(Path, Size);
    };
    ImFont* BodyFont = nullptr;
#ifdef BORON_HAS_FONTBODY
    BodyFont = ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)BoronAsset_FontBody, sizeof(BoronAsset_FontBody), 17.f, &FontConfig);
#endif
    if (!BodyFont)
        BodyFont = LoadFontFile("BarlowSemiCondensed-Medium.ttf", 17.f);
    if (!BodyFont)
        ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)font, sizeof(font), 17.f, &FontConfig);
#ifdef BORON_HAS_FONTDISPLAY
    g_FontDisplay = ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)BoronAsset_FontDisplay, sizeof(BoronAsset_FontDisplay), 30.f, &FontConfig);
#endif
    if (!g_FontDisplay)
        g_FontDisplay = LoadFontFile("BarlowCondensed-Black.ttf", 30.f);
#ifdef BORON_HAS_FONTBOLD
    g_FontBold = ImGui::GetIO().Fonts->AddFontFromMemoryTTF((void*)BoronAsset_FontBold, sizeof(BoronAsset_FontBold), 17.f, &FontConfig);
#endif
    if (!g_FontBold)
        g_FontBold = LoadFontFile("BarlowSemiCondensed-Bold.ttf", 17.f);

    g_IconContext = g_pd3dDeviceContext;
    g_Device = g_pd3dDevice;
    int IconsLoaded = 0;
    for (int i = 0; i < IcoCount; i++)
    {
#ifdef BORON_EMBEDDED_ASSETS
        for (auto& Asset : BoronIconAssets)
            if (Asset.Name && strcmp(Asset.Name, g_IconFiles[i]) == 0)
                g_Icons[i] = LoadTexture(g_pd3dDevice, nullptr, Asset.Data, Asset.Size);
#endif
        if (!g_Icons[i])
        {
            char Path[MAX_PATH];
            sprintf_s(Path, "%sGUI\\%s.png", BaseDir, g_IconFiles[i]);
            g_Icons[i] = LoadTexture(g_pd3dDevice, Path);
        }
        IconsLoaded += g_Icons[i] != nullptr;
    }
    BORON_LOG("[Boron][GUI] dir=%s display=%d bold=%d icons=%d/%d\n", BaseDir, g_FontDisplay != nullptr, g_FontBold != nullptr, IconsLoaded, (int)IcoCount);

    auto& style = ImGui::GetStyle();
    style.WindowRounding = 10.f;
    style.ChildRounding = 8.f;
    style.FrameRounding = 4.f;
    style.PopupRounding = 8.f;
    style.GrabRounding = 3.f;
    style.ScrollbarRounding = 4.f;
    style.TabRounding = 4.f;
    style.WindowBorderSize = 0.f;
    style.ChildBorderSize = 1.f;
    style.PopupBorderSize = 1.f;
    style.FrameBorderSize = 0.f;
    style.WindowPadding = ImVec2(14, 12);
    style.FramePadding = ImVec2(10, 7);
    style.ItemSpacing = ImVec2(10, 9);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.GrabMinSize = 14.f;
    style.ScrollbarSize = 9.f;
    style.SelectableTextAlign = ImVec2(0.f, 0.5f);
    ApplyColors();

    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    bool g_SwapChainOccluded = false;

    while (!done)
    {
        MSG msg;

        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }

        if (done)
            break;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            Sleep(10);
            continue;
        }

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_mainRenderTargetView->Release();

            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;

            ID3D11Texture2D* pBackBuffer;
            g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }

        {
            RECT Rect;
            POINT Cursor;
            GetWindowRect(hWnd, &Rect);
            GetCursorPos(&Cursor);
            bool bInside = PtInRect(&Rect, Cursor) || ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
            float Target = bInside ? 255.f : 214.f;
            float k = io.DeltaTime * 8.f;
            float Next = g_WindowAlpha + (Target - g_WindowAlpha) * (k > 1.f ? 1.f : k);
            if ((int)Next != (int)g_WindowAlpha)
                SetLayeredWindowAttributes(hWnd, 0, (BYTE)Next, LWA_ALPHA);
            g_WindowAlpha = Next;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Boron", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();

        static int SelectedUI = 0;
        static int hasEvent = 0;

        if (hasEvent == 0)
        {
            hasEvent = 1;
            for (auto& Event : Events::EventsArray)
            {
                if (Event.EventVersion != VersionInfo.FortniteVersion)
                    continue;

                hasEvent = 2;
            }
        }

        bool bHasCalendar = VersionInfo.FortniteVersion >= 11.31 && VersionInfo.FortniteVersion <= 11.50 || VersionInfo.FortniteVersion == 13.40 || VersionInfo.FortniteVersion >= 19.00 && VersionInfo.FortniteVersion <= 19.40;
        if ((SelectedUI == 1 || SelectedUI == 2) && gsStatus != StartedMatch)
            SelectedUI = 0;
        if (SelectedUI == 5 && !LategameConfig::bLateGame)
            SelectedUI = 0;

        auto Dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float W = ImGui::GetWindowWidth();
        float H = ImGui::GetWindowHeight();
        float Rounding = style.WindowRounding;
        float TopH = 64 * S;
        float FootH = 30 * S;

        {
            int v0 = Dl->VtxBuffer.Size;
            Dl->AddRectFilled(wp, ImVec2(wp.x + W, wp.y + H), IM_COL32(255, 255, 255, 255), Rounding);
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(Dl, v0, Dl->VtxBuffer.Size, wp, ImVec2(wp.x + W * 0.35f, wp.y + H), U32(ImVec4(0.075f, 0.115f, 0.245f, 1.f)), U32(ImVec4(0.025f, 0.032f, 0.07f, 1.f)));
            for (int i = 0; i < 6; i++)
                Dl->AddCircleFilled(ImVec2(wp.x + 40 * S, wp.y + 32 * S), (60 + i * 40) * S, U32(Alpha(kRed, 0.022f)), 48);
            v0 = Dl->VtxBuffer.Size;
            Dl->AddRectFilled(wp, ImVec2(wp.x + W, wp.y + TopH), IM_COL32(255, 255, 255, 255), Rounding, ImDrawFlags_RoundCornersTop);
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(Dl, v0, Dl->VtxBuffer.Size, wp, ImVec2(wp.x, wp.y + TopH), IM_COL32(8, 12, 30, 150), IM_COL32(8, 12, 30, 70));
            Dl->AddLine(ImVec2(wp.x, wp.y + TopH), ImVec2(wp.x + W, wp.y + TopH), IM_COL32(255, 255, 255, 22));
            Dl->AddRectFilled(ImVec2(wp.x, wp.y + H - FootH), ImVec2(wp.x + W, wp.y + H), IM_COL32(0, 0, 0, 90), Rounding, ImDrawFlags_RoundCornersBottom);
            Dl->AddRect(wp, ImVec2(wp.x + W, wp.y + H), IM_COL32(255, 255, 255, 30), Rounding);
        }

        DrawLogo(Dl, ImVec2(wp.x + 32 * S, wp.y + TopH * 0.5f), 15 * S);
        ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 28.f : 20.f);
        float WordW = ImGui::CalcTextSize("BORON").x;
        CenteredGlyphText(Dl, ImVec2(wp.x + 60 * S + WordW * 0.5f, wp.y + TopH * 0.5f), IM_COL32(255, 255, 255, 255), "BORON");
        float NavX = 60 * S + WordW + 22 * S;
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(NavX, 0));
        if (NavTab("Dashboard", SelectedUI == 0, TopH))
            SelectedUI = 0;
        if (gsStatus == StartedMatch)
        {
            ImGui::SameLine(0, 2 * S);
            if (NavTab("Match", SelectedUI == 1, TopH))
                SelectedUI = 1;
        }
        ImGui::SameLine(0, 2 * S);
        if (NavTab("Players", SelectedUI == 6, TopH))
            SelectedUI = 6;
        /*  // other maybe idfk just not bots tab plz"
        ImGui::SameLine(0, 2 * S);
        if (NavTab("tab7", SelectedUI == 7, TopH))
            SelectedUI = 7;
        */
        ImGui::SameLine(0, 2 * S);
        if (NavTab("Gameplay", SelectedUI == 3, TopH))
            SelectedUI = 3;
        if (LategameConfig::bLateGame)
        {
            ImGui::SameLine(0, 2 * S);
            if (NavTab("LateGame", SelectedUI == 5, TopH))
                SelectedUI = 5;
        }
        ImGui::SameLine(0, 2 * S);
        if (NavTab("Tools", SelectedUI == 4, TopH))
            SelectedUI = 4;

        float BtnSize = 32 * S;
        float BtnsW = BtnSize * 3 + 8 * S + 14 * S;
        float bx = W - BtnsW;
        float by = (TopH - BtnSize) * 0.5f;
        if (TitleButton("##pin", 0, ImVec2(bx, by), BtnSize))
        {
            g_Pinned = !g_Pinned;
            SetWindowPos(g_hWnd, g_Pinned ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        }
        Hint(g_Pinned ? "Unpin (stop staying on top)" : "Pin (always on top)");
        if (TitleButton("##min", 1, ImVec2(bx + BtnSize + 4 * S, by), BtnSize))
            ShowWindow(g_hWnd, SW_MINIMIZE);
        if (TitleButton("##close", 2, ImVec2(bx + (BtnSize + 4 * S) * 2, by), BtnSize))
            ImGui::OpenPopup("##shutdown");

        if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && ImGui::IsMouseHoveringRect(wp, ImVec2(wp.x + W - BtnsW, wp.y + TopH), false))
        {
            ReleaseCapture();
            SendMessage(g_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            io.AddMouseButtonEvent(0, false);
        }

        {
            float Fcy = wp.y + H - FootH * 0.5f;
            ImGui::PushFont(g_FontBold, 12.5f);
            // do not remove
            const char* Credit = "Made by Sarah (@ustruct on Discord)";
            ImVec2 Cs = ImGui::CalcTextSize(Credit);
            ImGui::SetCursorScreenPos(ImVec2(wp.x + 12 * S, Fcy - Cs.y * 0.5f - 3 * S));
            if (ImGui::InvisibleButton("##credits", ImVec2(Cs.x + 8 * S, Cs.y + 6 * S)))
                SelectedUI = 61;
            bool bCredHot = ImGui::IsItemHovered() || SelectedUI == 61;
            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            CenteredGlyphText(Dl, ImVec2(wp.x + 16 * S + Cs.x * 0.5f, Fcy), bCredHot ? IM_COL32(255, 255, 255, 235) : U32(Alpha(kMuted, 0.85f)), Credit);

            char Fn[24], Ue[24];
            sprintf_s(Fn, "FN %.2f", VersionInfo.FortniteVersion);
            sprintf_s(Ue, VersionInfo.EngineVersion >= 5.0 ? "UE %.1f" : "UE %.2f", VersionInfo.EngineVersion);
            ImVec2 Fs = ImGui::CalcTextSize(Fn), Us = ImGui::CalcTextSize(Ue);
            float Gap = 18 * S;
            float Vx = wp.x + (W - Fs.x - Gap - Us.x) * 0.5f;
            CenteredGlyphText(Dl, ImVec2(Vx + Fs.x * 0.5f, Fcy), U32(Alpha(kMuted, 0.85f)), Fn);
            Dl->AddCircleFilled(ImVec2(Vx + Fs.x + Gap * 0.5f, Fcy), 1.8f * S, U32(Alpha(kMuted, 0.6f)), 8);
            CenteredGlyphText(Dl, ImVec2(Vx + Fs.x + Gap + Us.x * 0.5f, Fcy), U32(Alpha(kMuted, 0.85f)), Ue);
            ImGui::PopFont();

        }

        ShutdownModal();

        ImGui::SetCursorPos(ImVec2(0, TopH));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26 * S, 18 * S));
        ImGui::BeginChild("page", ImVec2(0, H - TopH - FootH), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        static char commandBuffer[1024] = { 0 };
        auto MV = GetMatchView();
        {
            auto ZV = ZonePlanner::GetView();
            ZonePlanner::SetBlocked(ZV.bWorld && ZV.Radii.size() >= 2 && !PickMapImage(ZV.MapKey, ""));
        }
        switch (SelectedUI)
        {
        case 0:
        {
#ifdef MANUAL_SERVER_SETUP  // TODO: we need some palylist options and "custom" one like Reboot ultimate
            if (!GUI::bServerSetupRequested)
            {
                PageTitle("Server setup", "Pick a playlist and rules, then start the server");
                BeginPanel("setupserver", "Main settings", IcoServer);
                char PlaylistBuf[9999];
                sprintf_s(PlaylistBuf, "%ls", FConfig::Playlist);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextWithHint("##playlist", "Playlist ID path", PlaylistBuf, 9999))
                {
                    swprintf_s(FConfig::Playlist, L"%hs", PlaylistBuf);
                }
                Hint("Playlist ID Path");

                Toggle("Force Respawns", &GameRuleConfig::bForceRespawns, "Requires the client dll!");
                if (GameRuleConfig::bForceRespawns)
                {
                /* // uh maybe i need this idfk bruh
                    ImGui::SliderInt("Respawn Hight (Client)", &GameRuleConfig::RespawnHightClient, 10000, 999999);
                    ImGui::SliderInt("Respawn Hight (GameMode)", &GameRuleConfig::RespawnHightGamemode, 10000, 999999);*/
                    ImGui::Indent(14 * S);
                    ImGui::SetNextItemWidth(260 * S);
                    ImGui::SliderInt("Respawn Time (Client)", &GameRuleConfig::RespawnTimeClient, 1, 25);
                    ImGui::SetNextItemWidth(260 * S);
                    ImGui::SliderInt("Respawn Time (GameMode)", &GameRuleConfig::RespawnTimeGamemode, 1, 25);
                    ImGui::Unindent(14 * S);
                }
                Toggle("Creative Extra Ammo", &GameRuleConfig::bCreativeExtraAmmo, "Currently bugs the inventory out!!");
                Toggle("LWM AI", &GameRuleConfig::bLWM_AI, "NPC/boss move + shoot");

                char CustomMapDefBuffer[9999];
                sprintf_s(CustomMapDefBuffer, "%ls", FConfig::CreativeModeConfig::CustomMapDefinition);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextWithHint("##custommapdef", "Custom Map Definition (Creative)", CustomMapDefBuffer, 9999))
                {
                    swprintf_s(FConfig::CreativeModeConfig::CustomMapDefinition, L"%hs", CustomMapDefBuffer);
                }
                Hint("Custom Map Definition");

                ImGui::Dummy(ImVec2(0, 4 * S));
                if (SkewButton("##setup", "Setup server", ImVec2(260 * S, 52 * S), kYellow, kInk, IcoServer))
                    GUI::bServerSetupRequested = true;
                EndPanel();
            }
#endif
            std::string PlaylistTitle = "Boron";
            std::wstring wPlaylist = FConfig::Playlist;
            std::string sPlaylist(wPlaylist.begin(), wPlaylist.end());
            auto Slash = sPlaylist.find_last_of('/');
            std::string AssetName = Slash == std::string::npos ? sPlaylist : sPlaylist.substr(Slash + 1);
            if (gsStatus >= Joinable && MV.bGameMode)
            {
                if (MV.PlaylistName.size())
                    PlaylistTitle = MV.PlaylistName;
                if (MV.PlaylistSub.size() && PlaylistTitle.find(MV.PlaylistSub) == std::string::npos)
                    PlaylistTitle += " - " + MV.PlaylistSub;
            }
            else if (!AssetName.empty())
            {
                std::string Raw = AssetName.substr(0, AssetName.find('.'));
                if (Raw.rfind("Playlist_", 0) == 0)
                    Raw = Raw.substr(9);
                static const std::pair<const char*, const char*> Words[] = {
                    { "ShowdownTournament", "Tournament" }, { "ShowdownAlt", "Arena" }, { "Showdown", "Arena" }, { "NoBuildBR", "Zero Build" },
                    { "Figment", "" }, { "Clyde", "" }, { "Habanero", "" }, { "Default", "" } };
                std::string Pretty;
                size_t Start = 0;
                while (Start <= Raw.size())
                {
                    size_t End = Raw.find('_', Start);
                    std::string Token = Raw.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
                    for (auto& [From, To] : Words)
                    {
                        auto At = Token.find(From);
                        if (At != std::string::npos)
                            Token.replace(At, strlen(From), std::string(To) + (*To && At + strlen(From) < Token.size() ? " " : ""));
                    }
                    if (!Token.empty())
                        Pretty += (Pretty.empty() ? "" : " ") + Token;
                    if (End == std::string::npos)
                        break;
                    Start = End + 1;
                }
                PlaylistTitle = Pretty.empty() ? Raw : Pretty;
            }
            {
                const char* Mode = nullptr;
                if (sPlaylist.find("/FigmentPlaylists/") != std::string::npos)
                    Mode = "Fortnite OG";
                else if (sPlaylist.find("Habanero") != std::string::npos)
                    Mode = "Ranked";
                else if (sPlaylist.find("/Clyde/") != std::string::npos)
                    Mode = "Remix";
                if (Mode && PlaylistTitle.find(Mode) == std::string::npos)
                    PlaylistTitle = std::string(Mode) + " - " + PlaylistTitle;
            }

            {
                float HeroH = 176 * S;
                ImVec2 p = ImGui::GetCursorScreenPos();
                float HW = ImGui::GetContentRegionAvail().x;
                auto PDl = ImGui::GetWindowDrawList();
                int v0 = PDl->VtxBuffer.Size;
                PDl->AddRectFilled(p, ImVec2(p.x + HW, p.y + HeroH), IM_COL32(255, 255, 255, 255), 8 * S);
                ImGui::ShadeVertsLinearColorGradientKeepAlpha(PDl, v0, PDl->VtxBuffer.Size, p, ImVec2(p.x + HW, p.y), U32(ImVec4(0.62f, 0.08f, 0.13f, 0.95f)), U32(ImVec4(0.10f, 0.14f, 0.32f, 0.55f)));
                PDl->PushClipRect(p, ImVec2(p.x + HW, p.y + HeroH), true);
                for (int i = 0; i < 14; i++)
                {
                    float x = p.x + HW * 0.42f + i * 34 * S;
                    PDl->AddQuadFilled(ImVec2(x, p.y), ImVec2(x + 14 * S, p.y), ImVec2(x - 40 * S, p.y + HeroH), ImVec2(x - 54 * S, p.y + HeroH), IM_COL32(255, 255, 255, 7));
                }
                PDl->PopClipRect();
                PDl->AddRect(p, ImVec2(p.x + HW, p.y + HeroH), IM_COL32(255, 255, 255, 30), 8 * S);

                ImGui::SetCursorScreenPos(ImVec2(p.x + 24 * S, p.y + 18 * S));
                ImGui::PushFont(g_FontBold, 14.f);
                ImGui::TextColored(ImVec4(1, 1, 1, 0.75f), "%s", gsStatus == NotReady ? "WARMING UP" : (gsStatus == Joinable ? "NOW HOSTING  -  READY FOR PLAYERS" : "NOW HOSTING  -  MATCH LIVE"));
                ImGui::PopFont();
                ImGui::SetCursorScreenPos(ImVec2(p.x + 22 * S, ImGui::GetCursorScreenPos().y - 4 * S));
                {
                    std::string UpperTitle = Upper(PlaylistTitle.c_str());
                    float TitleSize = g_FontDisplay ? 58.f : 30.f;
                    float MaxW = HW - 24 * S - 120 * S;
                    ImGui::PushFont(g_FontDisplay, TitleSize);
                    float TitleW = ImGui::CalcTextSize(UpperTitle.c_str()).x;
                    ImGui::PopFont();
                    if (TitleW > MaxW && TitleW > 0)
                        TitleSize = (std::max)(24.f, TitleSize * MaxW / TitleW);
                    ImGui::PushFont(g_FontDisplay, TitleSize);
                    ImGui::TextUnformatted(UpperTitle.c_str());
                    ImGui::PopFont();
                }
                ImGui::SetCursorScreenPos(ImVec2(p.x + 24 * S, ImGui::GetCursorScreenPos().y - 6 * S));
                ImGui::TextColored(ImVec4(1, 1, 1, 0.6f), "%s", AssetName.substr(0, AssetName.find('.')).c_str());

                ImGui::SetCursorScreenPos(ImVec2(p.x + 24 * S, p.y + HeroH - 44 * S));
                if (gsStatus >= Joinable && MV.bGameMode)
                {
                    char Buf[64];
                    sprintf_s(Buf, "%d PLAYERS", MV.Alive);
                    Chip(IcoPeople, Buf, kGreen);
                    ImGui::SameLine(0, 8 * S);
                    int Secs = MV.Secs;
                    sprintf_s(Buf, "%d:%02d:%02d", Secs / 3600, (Secs / 60) % 60, Secs % 60);
                    Chip(IcoStopwatch, Buf, kYellow);
                    ImGui::SameLine(0, 8 * S);
                    sprintf_s(Buf, "PORT %d", (int)FConfig::Port);
                    Chip(IcoServer, Buf, kBlue);
                    ImGui::SameLine(0, 8 * S);
                    sprintf_s(Buf, "%s  /  %d TICK", VersionInfo.FortniteVersion >= 24.40 ? (FConfig::bEnableIris ? "IRIS" : "LEGACY") : "LEGACY", (int)FConfig::MaxTickRate);
                    Chip(IcoCogs, Buf, kMuted);
                }

                float CtaW = 250 * S;
                float CtaH = 70 * S;
                ImVec2 Cta(p.x + HW - CtaW - 28 * S, p.y + (HeroH - CtaH) * 0.5f);
                if (gsStatus == Joinable)
                {
                    ImGui::SetCursorScreenPos(Cta);
                    if (SkewButton("##startbus", "Start bus", ImVec2(CtaW, CtaH), kYellow, kInk, IcoBus))
                        GUI::RunOnGameThread([]
                    {
                        if (UFortGameStateComponent_BattleRoyaleGamePhaseLogic::GetDefaultObj())
                        {
                            UFortGameStateComponent_BattleRoyaleGamePhaseLogic::bStartAircraft = true;
                            // auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get();

                            // GamePhaseLogic->StartAircraftPhase();
                        }
                        else
                            UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), FString(L"startaircraft"), nullptr);
                    });
                    Hint("Start Bus Early");
                }
                else if (gsStatus == StartedMatch && MV.bGameMode)
                {
                    int Alive = MV.Alive;
                    char Num[16];
                    sprintf_s(Num, "%d", Alive);
                    ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 84.f : 40.f);
                    ImVec2 ns = ImGui::CalcTextSize(Num);
                    PDl->AddText(ImVec2(p.x + HW - 40 * S - ns.x, p.y + (HeroH - ns.y) * 0.5f - 10 * S), IM_COL32(255, 255, 255, 255), Num);
                    ImGui::PopFont();
                    ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 22.f : 15.f);
                    const char* Left = "PLAYERS LEFT";
                    ImVec2 ls = ImGui::CalcTextSize(Left);
                    PDl->AddText(ImVec2(p.x + HW - 40 * S - ls.x, p.y + (HeroH + ns.y) * 0.5f - 18 * S), U32(kYellow), Left);
                    ImGui::PopFont();
                }
                else
                {
                    ImGui::SetCursorScreenPos(ImVec2(p.x + HW - 90 * S, p.y + (HeroH - 44 * S) * 0.5f));
                    Spinner(22 * S, 4 * S, kYellow);
                }
                ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + HeroH));
                ImGui::Dummy(ImVec2(HW, 10 * S));
            }

            if (gsStatus <= Joinable)
            {
                BeginPanel("pregame", "Before the bus", IcoCrown);
                if (VersionInfo.FortniteVersion >= 7.00 && wcscmp(FConfig::Playlist, L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2") == 0)
                {
                    ImGui::TextColored(kYellow, "Lategame is disabled due to the Creative playlist!!!");
                    LategameConfig::bLateGame = false; // froce it ig cuz then ppl dont have fucking strom issue on creative
                }
                else
                    Toggle("Lategame", &LategameConfig::bLateGame, "Adds the LateGame tab");
                EndPanel();
            }

            ConsolePanel(commandBuffer);
        }
        break;
        case 1:
        {
            PageTitle(hasEvent == 2 ? "Match & Event" : "Match", "Storm controls for the running match");
            if (!MV.bGameMode)
                break;
            BeginPanel("storm", "Storm", IcoStorm);
            float TW = (ImGui::GetContentRegionAvail().x - 12 * S) * 0.5f;
            ImVec2 TS(TW, 84 * S);
            auto WithGameMode = [](auto Fn)
            {
                GUI::RunOnGameThread([Fn]
                                     {
                                         auto World = UWorld::GetWorld();
                                         auto GameMode = World ? (AFortGameMode*)World->AuthorityGameMode : nullptr;
                                         if (GameMode)
                                             Fn(World, GameMode);
                                     });
            };
            auto Indicator = [](UWorld* World, AFortGameMode* GameMode) -> AFortSafeZoneIndicator*
            {
                if (GameMode->HasSafeZoneIndicator())
                    return GameMode->SafeZoneIndicator;
                auto GamePhaseLogic = UFortGameStateComponent_BattleRoyaleGamePhaseLogic::Get(World);
                return GamePhaseLogic ? GamePhaseLogic->SafeZoneIndicator : nullptr;
            };
            if (Tile("##pause", IcoStopwatch, "Pause storm", "Freeze the current zone", TS, kBlue))
                WithGameMode([](UWorld* World, AFortGameMode* GameMode)
                             {
                                 UFortGameStateComponent_BattleRoyaleGamePhaseLogic::bPausedZone = true;
                                 if (GameMode->HasbSafeZonePaused())
                                     GameMode->bSafeZonePaused = true;
                                 UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"pausesafezone"), nullptr);
                             });
            ImGui::SameLine(0, 12 * S);
            if (Tile("##resume", IcoStorm, "Resume storm", "Let the zone move again", TS, kGreen))
                WithGameMode([](UWorld* World, AFortGameMode* GameMode)
                             {
                                 UFortGameStateComponent_BattleRoyaleGamePhaseLogic::bPausedZone = false;
                                 if (GameMode->HasbSafeZonePaused())
                                     GameMode->bSafeZonePaused = false;
                                 UKismetSystemLibrary::ExecuteConsoleCommand(World, FString(L"startsafezone"), nullptr);
                             });

            if (Tile("##skip", IcoPlace, "Skip zone", "Jump to the next safe zone", TS, kYellow))
                WithGameMode([Indicator](UWorld* World, AFortGameMode* GameMode)
                             {
                                 if (auto Zone = Indicator(World, GameMode))
                                 {
                                     Zone->SafeZoneStartShrinkTime = (float)UGameplayStatics::GetTimeSeconds(World);
                                     Zone->SafeZoneFinishShrinkTime = Zone->SafeZoneStartShrinkTime + 0.05f;
                                 }
                             });
            ImGui::SameLine(0, 12 * S);
            if (Tile("##shrink", IcoHeart, "Shrink now", "Start shrinking the safe zone", TS, kRed))
                WithGameMode([Indicator](UWorld* World, AFortGameMode* GameMode)
                             {
                                 if (auto Zone = Indicator(World, GameMode))
                                     Zone->SafeZoneStartShrinkTime = (float)UGameplayStatics::GetTimeSeconds(World);
                             });
            EndPanel();

            if (hasEvent == 2)
            {
                BeginPanel("event", "Live event", IcoMedal);
                ImGui::TextDisabled("This version has a live event.");
                if (SkewButton("##startevent", "Start event", ImVec2(240 * S, 56 * S), kYellow, kInk, IcoMedal))
                    GUI::RunOnGameThread([] { Events::StartEvent(); });
                EndPanel();
            }
        }
        break;
        case 2:
            if (ImGui::Button("Start Event"))
                GUI::RunOnGameThread([] { Events::StartEvent(); });

            break;
        case 3:
        {
            PageTitle("Gameplay", "Rules and world tools");
            BeginPanel("rules", "Rules", IcoCogs);
            Toggle("Infinite Materials", &GameRuleConfig::bInfiniteMats);
            Toggle("Infinite Ammo", &GameRuleConfig::bInfiniteAmmo);
            Toggle("Keep Inventory", &GameRuleConfig::bKeepInventory);
            EndPanel();

            BeginPanel("tuning", "Tuning", IcoHeart);
            ImGui::SetNextItemWidth(320 * S);
            ImGui::SliderInt("Siphon Amount", &GameRuleConfig::SiphonAmount, 0, 200);
            ImGui::SetNextItemWidth(320 * S);
            ImGui::SliderInt("Tick Rate", &FConfig::MaxTickRate, 30, 120);
            EndPanel();

            BeginPanel("world", "World", IcoToolbox);
            if (SkewButton("##resetbuilds", "Reset builds", ImVec2(210 * S, 46 * S), kRed, ImVec4(1, 1, 1, 1)))
                GUI::RunOnGameThread([]
                                     {
                                         TArray<ABuildingSMActor*> Builds;
                                         Utils::GetAll<ABuildingSMActor>(Builds);

                                         for (auto& Build : Builds)
                                             if (Build && Build->bPlayerPlaced)

                                                 if (FConfig::GuiShit::bPlayBuildsResetAnimation)
                                                 {
                                                     Build->K2_DestroyActor();
                                                 }
                                                 /* else if (VersionInfo.FortniteVersion <= 26.30)   // crashes for now wtf :(
                                                 {
                                                     Build->SilentDie();
                                                 }*/

                                         Builds.Free();
                                     });
            ImGui::SameLine(0, 18 * S);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8 * S);
            Toggle("Play reset animation", &FConfig::GuiShit::bPlayBuildsResetAnimation, "On some builds it might crash");

            if (SkewButton("##floorloot", "Destroy floor loot", ImVec2(210 * S, 46 * S), kRed, ImVec4(1, 1, 1, 1)))
                GUI::RunOnGameThread([]
                                     {
                                         TArray<AFortPickupAthena*> Pickups;
                                         Utils::GetAll<AFortPickupAthena>(Pickups);

                                         for (auto& Pickup : Pickups)
                                             if (Pickup)
                                                 Pickup->K2_DestroyActor();

                                         Pickups.Free();
                                     });
            EndPanel();
        }
        break;
        case 4:
        {
            PageTitle("Tools", "Dumpers and version extras");
            static auto PlaylistClass = UFortPlaylistAthena::StaticClass();

            BeginPanel("dumps", "Dump", IcoToolbox);
            float TW = (ImGui::GetContentRegionAvail().x - 12 * S) * 0.5f;
            if (Tile("##dumpitems", IcoGift, "Dump items", "Write every item definition to a file", ImVec2(TW, 80 * S), kBlue))
            {
                GUI::RunOnGameThread([] { DumpItems(); });
            }
            if (PlaylistClass)
            {
                ImGui::SameLine(0, 12 * S);
                if (Tile("##dumpplaylists", IcoCalendar, "Dump playlists", "Write every playlist to a file", ImVec2(TW, 80 * S), kGreen))
                {
                    GUI::RunOnGameThread([] { DumpPlaylists(); });
                }
            }
            EndPanel();

            if (bHasCalendar)
            {
                BeginPanel("calendar", "Calendar (experimental)", IcoCalendar);

                auto SetApolloSnow = [](float amount, float falling)
                {
                    auto SnowSetup = FindObject<UObject>(L"/Game/Athena/Apollo/Maps/Apollo_POI_Foundations.Apollo_POI_Foundations:PersistentLevel.BP_ApolloSnowSetup_2");
                    if (!SnowSetup)
                        return false;

                    static auto SnowAmountOffset = SnowSetup->GetOffset("SnowAmount");
                    static auto SnowFallingOffset = SnowSetup->GetOffset("SnowFalling");
                    if (SnowAmountOffset == -1 || SnowFallingOffset == -1)
                        return false;

                    GetFromOffset<float>(SnowSetup, SnowAmountOffset) = amount;
                    GetFromOffset<float>(SnowSetup, SnowFallingOffset) = falling;

                    bool HasAuthority = true;
                    if (auto AmountFn = SnowSetup->GetFunction("OnRep_Snow_Amount"))
                        SnowSetup->ProcessEvent(AmountFn, &HasAuthority);
                    if (auto FallingFn = SnowSetup->GetFunction("OnRep_SnowFalling"))
                        SnowSetup->ProcessEvent(FallingFn, &HasAuthority);

                    return true;
                };

                if (VersionInfo.FortniteVersion >= 11.31 && VersionInfo.FortniteVersion <= 11.50)
                {
                    static float SnowLevelS11 = 0.0f;

                    ImGui::SetNextItemWidth(300 * S);
                    ImGui::SliderFloat("Snow Level", &SnowLevelS11, 0, 100.0f);
                    if (ImGui::Button("Set Snow Level"))
                        GUI::RunOnGameThread([SetApolloSnow, Level = SnowLevelS11] { SetApolloSnow(Level, Level); });
                    ImGui::SameLine();
                    if (ImGui::Button("Toggle Full Snow Map"))
                    {
                        static bool bFullSnow = false;
                        bFullSnow = !bFullSnow;
                        SnowLevelS11 = bFullSnow ? 100.0f : 0.0f;
                        GUI::RunOnGameThread([SetApolloSnow, bOn = bFullSnow] { SetApolloSnow(bOn ? 100.0f : 0.0f, bOn ? 69.0f : 0.0f); });
                    }
                };
                if (VersionInfo.FortniteVersion == 13.40)
                {
                    float MinWaterS13 = 0;
                    float MaxWaterS13 = 7;

                    ImGui::SetNextItemWidth(300 * S);
                    ImGui::SliderFloat(("Water Level"), &MinWaterS13, 0, MaxWaterS13);

                    if (ImGui::Button("Set Water Level"))
                    {
                    // TODO
                    }

                };

                if (VersionInfo.FortniteVersion >= 19.00 && VersionInfo.FortniteVersion <= 19.40)
                {

                    float MinSnowS19 = 0.0f;
                    float MaxSnowS19 = 100.0f;

                    ImGui::SetNextItemWidth(300 * S);
                    ImGui::SliderFloat(("Snow Level"), &MinSnowS19, 0, MaxSnowS19);
                    if (ImGui::Button("Set Snow Level"))
                    {
                        // TODO
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Toggle Full Snow Map"))
                    {
                        // TODO
                    }

                };
                EndPanel();
            }
        }
        break;

        case 5:
        {
            PageTitle("LateGame", "Loot and zone settings for lategame");

            BeginPanel("lgloot", "Loot pool", IcoGift);
            Toggle("Versionized Lootpool", &LategameConfig::bLateGameVersionized, "Fixed per-version loadouts");
            if (LategameConfig::bLateGameVersionized)
            {
                LategameConfig::bLateGameCustom = false;

            }

            Toggle("Gamemode Lootpool", &LategameConfig::bPullGamemodeLootPool, "Pulls weapons and items from the playlist's own loot pool");

            Toggle("Custom Late Game loadout", &LategameConfig::bLateGameCustom, "Not fully done yet!!! One loadout for everyone in the game");
            if (gsStatus == StartedMatch)
                ImGui::TextDisabled("Loot changes apply to the next (re)spawn.");
            EndPanel();

            if (LategameConfig::bLateGameCustom)
            {

                LategameConfig::bLateGameVersionized = false;

                BeginPanel("LGc", "Custom loadout", IcoCrown);
                if (ImGui::BeginTable("lgslots", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX))
                {
                    ImGui::TableSetupColumn("SLOT", ImGuiTableColumnFlags_WidthFixed, 44 * S);
                    ImGui::TableSetupColumn("ITEM PATH", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("COUNT", ImGuiTableColumnFlags_WidthFixed, 130 * S);
                    ImGui::TableHeadersRow();

                    wchar_t* Slots[] = { LategameConfig::CustomSlot1Item, LategameConfig::CustomSlot2Item, LategameConfig::CustomSlot3Item, LategameConfig::CustomSlot4Item, LategameConfig::CustomSlot5Item };
                    int* Counts[] = { &LategameConfig::CustomSlot1ItemCount, &LategameConfig::CustomSlot2ItemCount, &LategameConfig::CustomSlot3ItemCount, &LategameConfig::CustomSlot4ItemCount, &LategameConfig::CustomSlot5ItemCount };
                    for (int i = 0; i < 5; i++)
                    {
                        ImGui::PushID(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::AlignTextToFramePadding();
                        ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 22.f : 17.f);
                        ImGui::TextColored(kYellow, "%d", i + 1);
                        ImGui::PopFont();
                        ImGui::TableNextColumn();
                        char buf[500];
                        sprintf_s(buf, "%ls", Slots[i]);
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##item", buf, 500))
                        {
                            swprintf_s(Slots[i], 500, L"%hs", buf);
                        }
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1);
                        ImGui::SliderInt("##count", Counts[i], 1, 99);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                EndPanel();
            }

            BeginPanel("lgzone", "Zone", IcoStorm);
            bool bZoneLocked = gsStatus == StartedMatch;
            ImGui::BeginDisabled(bZoneLocked);
            ImGui::SetNextItemWidth(260 * S);
            ImGui::SliderInt("Zone phase", &LategameConfig::LateGameZone, 3, 7); // pahse 7 is playeble ig
            // fix / not use Long Zones for pre-s13 so from 11.00 to 13.30
            if (VersionInfo.FortniteVersion < 11 || VersionInfo.FortniteVersion > 13.30)
            {
                Toggle("Long Zone", &LategameConfig::bLateGameLongZone, "A zone doesnt close for a long time.");
            }
            /*
            ImGui::Spacing();
            ImGui::Checkbox("Realistic Moving Bus", &LategameConfig::bLateGameMovingBus);
            */
            ImGui::EndDisabled();
            if (bZoneLocked)
                ImGui::TextColored(kYellow, "Locked: the match already started.");
            EndPanel();

            ZonePlannerPanel();

            BeginPanel("lgnotes", "Quick notes");
            ImGui::BulletText("LG Zone Phase: Phases can be 3 - 7 !!!");
            ImGui::BulletText("Long Zone: A zone doesnt close for a long time.");
            ImGui::BulletText("Custom loadout: This option makes only 1 Loadout for everyone in the game. You can add your own WID's to it.");
            ImGui::BulletText("Versionized Lootpool: fixed loadouts picked for this version.");
            ImGui::BulletText("Gamemode Lootpool: weapons come from the playlist's own loot pool.");
            ImGui::BulletText("Loot changes apply on the next (re)spawn.");
            ImGui::BulletText("Zone settings lock once the match starts.");
            ImGui::BulletText("Zone planner: click the map to place circle 1, drag a number to move it.");
            ImGui::BulletText("Zone planner: right click a number to unpin it, unpinned circles are rolled.");
            ImGui::BulletText("Free placement lets circles go anywhere, not just inside the last one.");
            ImGui::BulletText("Reroll path rolls the unpinned circles again, Clear all removes every pin.");
            ImGui::BulletText("Your planned zones are used from the next match.");
            EndPanel();
        }
        break;

        case 6:
        {
            auto& Players = MV.Players;
            static AFortPlayerControllerAthena* Selected = nullptr;
            static int PlayerSubTab = 0;
            static char giveItemBuf[500] = "/Game/Athena/Items/Weapons/WID_Shotgun_Standard_Athena_VR_Ore_T03.WID_Shotgun_Standard_Athena_VR_Ore_T03";
            static int GiveCount = 1;
            static char KickReason[128] = "You have been kicked!";
            static int LaunchPower = 2500;

            bool bFound = false;
            for (auto& P : Players)
                bFound |= P.PC == Selected;
            if (!bFound)
                Selected = Players.empty() ? nullptr : Players[0].PC;

            float AreaH = ImGui::GetContentRegionAvail().y;
            float ListW = 290 * S;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12 * S, 12 * S));
            ImGui::BeginChild("plist", ImVec2(ListW, AreaH), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PopStyleVar();
            {
                char Head[32];
                sprintf_s(Head, "PLAYERS  %d", (int)Players.size());
                ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 26.f : 18.f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4 * S);
                ImGui::TextUnformatted(Head);
                ImGui::PopFont();
                ImGui::Dummy(ImVec2(0, 2 * S));
                if (Players.empty())
                    ImGui::TextDisabled("No players connected.");
                for (size_t i = 0; i < Players.size(); i++)
                {
                    auto& P = Players[i];
                    ImGui::PushID((int)i);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    float RW = ImGui::GetContentRegionAvail().x;
                    float RH = 70 * S;
                    bool bSel = P.PC == Selected;
                    if (ImGui::InvisibleButton("##row", ImVec2(RW, RH)))
                        Selected = P.PC;
                    float t = Anim(ImGui::GetItemID(), bSel ? 1.f : (ImGui::IsItemHovered() ? 0.4f : 0.f));
                    auto LDl = ImGui::GetWindowDrawList();
                    LDl->AddRectFilled(p, ImVec2(p.x + RW, p.y + RH), U32(Mix(ImVec4(1, 1, 1, 0.03f), Alpha(kRed, 0.22f), t)), 6 * S);
                    if (bSel)
                        LDl->AddRectFilled(p, ImVec2(p.x + 4 * S, p.y + RH), U32(kRed), 6 * S, ImDrawFlags_RoundCornersLeft);
                    DrawAvatar(LDl, ImVec2(p.x + 32 * S, p.y + RH * 0.5f), 20 * S, P.Name.c_str(), P.bHasTeam ? P.Team : (int)i, P.bAlive);
                    float tx = p.x + 62 * S;
                    float tw = RW - 62 * S - 10 * S;
                    ImGui::PushFont(g_FontBold, 17.f);
                    LDl->PushClipRect(ImVec2(tx, p.y), ImVec2(tx + tw, p.y + RH), true);
                    LDl->AddText(ImVec2(tx, p.y + 9 * S), IM_COL32(255, 255, 255, 255), P.Name.c_str());
                    LDl->PopClipRect();
                    ImGui::PopFont();
                    if (P.bAlive)
                    {
                        StatBar(LDl, ImVec2(tx, p.y + 36 * S), tw, 7 * S, P.Shield, 100.f, kBlue, false);
                        StatBar(LDl, ImVec2(tx, p.y + 47 * S), tw, 9 * S, P.Health, 100.f, kGreen, false);
                        char Elims[24];
                        sprintf_s(Elims, P.Over > 0.f ? "%d  +%d" : "%d", P.Elims, (int)P.Over);
                        ImGui::PushFont(g_FontBold, 13.f);
                        ImVec2 es = ImGui::CalcTextSize(Elims);
                        LDl->AddText(ImVec2(tx + tw - es.x, p.y + 11 * S), U32(P.Over > 0.f ? ImVec4(0.6f, 0.88f, 1.f, 1.f) : kYellow), Elims);
                        ImGui::PopFont();
                    }
                    else
                    {
                        ImGui::PushFont(nullptr, 14.f);
                        LDl->AddText(ImVec2(tx, p.y + 38 * S), U32(kMuted), "Not spawned");
                        ImGui::PopFont();
                    }
                    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + RH + 6 * S));
                    ImGui::Dummy(ImVec2(0, 0));
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 14 * S);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("pview", ImVec2(0, AreaH));
            ImGui::PopStyleColor();
            FPlayerView* Sel = nullptr;
            for (auto& P : Players)
                if (P.PC == Selected)
                    Sel = &P;
            if (!Sel)
            {
                PageTitle("Players", "Select a player on the left");
                ImGui::EndChild();
                break;
            }

            bool bAlive = Sel->bAlive;
            auto SelPC = Sel->PC;
            int Team = Sel->bHasTeam ? Sel->Team : 0;

            BeginPanel("pheader");
            {
                ImVec2 hp = ImGui::GetCursorScreenPos();
                auto HDl = ImGui::GetWindowDrawList();
                DrawAvatar(HDl, ImVec2(hp.x + 36 * S, hp.y + 36 * S), 34 * S, Sel->Name.c_str(), Team, bAlive);
                ImGui::SetCursorScreenPos(ImVec2(hp.x + 86 * S, hp.y - 4 * S));
                ImGui::BeginGroup();
                ImGui::PushFont(g_FontDisplay, g_FontDisplay ? 44.f : 26.f);
                ImGui::TextUnformatted(Upper(Sel->Name.c_str()).c_str());
                ImGui::PopFont();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6 * S);
                ImGui::TextColored(kMuted, "%s", Sel->Skin.empty() ? (bAlive ? "Alive" : "Not spawned") : Sel->Skin.c_str());
                ImGui::EndGroup();
                ImGui::SetCursorScreenPos(ImVec2(hp.x, hp.y + 80 * S));
                ImGui::Dummy(ImVec2(0, 0));

                float BarW = ImGui::GetContentRegionAvail().x;
                ImVec2 bp = ImGui::GetCursorScreenPos();
                if (bAlive)
                {
                    float y = bp.y;
                    if (Sel->bHasOver && Sel->OverMax > 0.f)
                    {
                        HudBar(HDl, ImVec2(bp.x, y), BarW, 14 * S, Sel->Over, Sel->OverMax, ImVec4(0.55f, 0.85f, 1.f, 1.f), ImVec4(0.85f, 0.95f, 1.f, 1.f), 2);
                        y += 20 * S;
                    }
                    HudBar(HDl, ImVec2(bp.x, y), BarW, 18 * S, Sel->Shield, 100.f, ImVec4(0.10f, 0.55f, 0.95f, 1.f), ImVec4(0.15f, 0.80f, 1.f, 1.f), 0);
                    y += 24 * S;
                    HudBar(HDl, ImVec2(bp.x, y), BarW, 18 * S, Sel->Health, 100.f, ImVec4(0.15f, 0.75f, 0.20f, 1.f), ImVec4(0.45f, 0.92f, 0.25f, 1.f), 1);
                    y += 24 * S;
                    ImGui::Dummy(ImVec2(BarW, y - bp.y + 2 * S));
                }
                char Buf[64];
                sprintf_s(Buf, "%d ELIMS", Sel->Elims);
                Chip(IcoMedal, Buf, kYellow);
                ImGui::SameLine(0, 8 * S);
                sprintf_s(Buf, "TEAM %d", Team);
                Chip(IcoPeople, Buf, TeamColor(Team));
                ImGui::SameLine(0, 8 * S);
                Chip(IcoServer, bAlive ? "IN MATCH" : "NOT SPAWNED", bAlive ? kGreen : kMuted);
                ImGui::Dummy(ImVec2(0, 2 * S));
                if (NavTab("Main", PlayerSubTab == 0, 40 * S))
                    PlayerSubTab = 0;
                ImGui::SameLine(0, 2 * S);
                if (NavTab("Fun", PlayerSubTab == 1, 40 * S))
                    PlayerSubTab = 1;
            }
            EndPanel();

            if (PlayerSubTab == 0)
            {
                BeginPanel("pgive", "Give item", IcoGift);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##giveitempath", "Item path, e.g. /Game/.../WID_X.WID_X", giveItemBuf, 500);
                ImGui::SetNextItemWidth(130 * S);
                ImGui::InputInt("Count", &GiveCount);
                if (GiveCount < 1)
                    GiveCount = 1;
                if (SkewButton("##give", "Give", ImVec2(130 * S, 42 * S), kYellow, kInk, IcoGift))
                    GUI::RunOnGameThread([SelPC, Path = std::string(giveItemBuf), Count = GiveCount]
                                         {
                                             if (IsLivePC(SelPC))
                                                 GiveTo(SelPC, Path.c_str(), Count);
                                         });
                ImGui::SameLine(0, 10 * S);
                if (SkewButton("##giveall", "Give all", ImVec2(150 * S, 42 * S), ImVec4(0.93f, 0.95f, 1.f, 1.f), kInk, IcoPeople))
                    GUI::RunOnGameThread([Path = std::string(giveItemBuf), Count = GiveCount]
                                         {
                                             for (auto& P : CollectPlayers())
                                                 GiveTo(P.PC, Path.c_str(), Count);
                                         });
                Hint("Give this item to every connected player");
                EndPanel();

                BeginPanel("phealth", "Health", IcoHeart);
                ImGui::BeginDisabled(!bAlive);
                if (SkewButton("##heal", "Heal", ImVec2(130 * S, 42 * S), kGreen, kInk, IcoHeart) && bAlive)
                    GUI::RunOnGameThread([SelPC]
                                         {
                                             auto Pawn = IsLivePC(SelPC) ? (AFortPlayerPawnAthena*)SelPC->Pawn : nullptr;
                                             if (!Pawn)
                                                 return;
                                             Pawn->SetHealth(100.f);
                                             Pawn->SetShield(100.f);
                                         });
                Hint("Full health and shield");
                ImGui::SameLine(0, 10 * S);
                if (SkewButton("##kill", "Kill", ImVec2(130 * S, 42 * S), kRed, ImVec4(1, 1, 1, 1)) && bAlive)
                    GUI::RunOnGameThread([SelPC]
                                         {
                                             if (!IsLivePC(SelPC) || !SelPC->Pawn)
                                                 return;
                                             if (auto Fn = SelPC->GetFunction("ServerSuicide"))
                                                 SelPC->ProcessEvent(Fn, nullptr);
                                         });
                Hint("Same as the player self-eliminating");
                ImGui::EndDisabled();
                EndPanel();

                BeginPanel("pmove", "Movement", IcoPlace);
                static int TeleportTarget = 0;
                std::vector<FPlayerView*> Others;
                for (auto& P : Players)
                    if (P.PC != SelPC && P.bAlive)
                        Others.push_back(&P);
                if (TeleportTarget >= (int)Others.size())
                    TeleportTarget = 0;
                ImGui::BeginDisabled(!bAlive || Others.empty());
                ImGui::SetNextItemWidth(220 * S);
                if (ImGui::BeginCombo("##tptarget", Others.empty() ? "No other spawned players" : Others[TeleportTarget]->Name.c_str()))
                {
                    for (int i = 0; i < (int)Others.size(); i++)
                        if (ImGui::Selectable(Others[i]->Name.c_str(), i == TeleportTarget))
                            TeleportTarget = i;
                    ImGui::EndCombo();
                }
                ImGui::SameLine(0, 10 * S);
                if (SkewButton("##tp", "Teleport", ImVec2(160 * S, ImGui::GetFrameHeight() + 6 * S), kBlue, kInk, IcoPlace) && bAlive && !Others.empty())
                    GUI::RunOnGameThread([SelPC, TargetPC = Others[TeleportTarget]->PC]
                                         {
                                             auto Pawn = IsLivePC(SelPC) ? (AFortPlayerPawnAthena*)SelPC->Pawn : nullptr;
                                             auto Target = IsLivePC(TargetPC) ? (AFortPlayerPawnAthena*)TargetPC->Pawn : nullptr;
                                             if (!Pawn || !Target)
                                                 return;
                                             auto Loc = Target->K2_GetActorLocation();
                                             Loc.Z = Loc.Z + 200.0;
                                             Pawn->K2_TeleportTo(Loc, FRotator());
                                         });
                ImGui::EndDisabled();
                ImGui::BeginDisabled(!bAlive);
                if (ImGui::Button("Copy location") && bAlive)
                {
                    char LocBuf[96];
                    sprintf_s(LocBuf, "%.1f, %.1f, %.1f", Sel->Loc[0], Sel->Loc[1], Sel->Loc[2]);
                    ImGui::SetClipboardText(LocBuf);
                }
                ImGui::EndDisabled();
                EndPanel();

                BeginPanel("pkick", "Kick", IcoKick);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 190 * S);
                ImGui::InputTextWithHint("##kickreason", "Kick reason", KickReason, 128);
                ImGui::SameLine(0, 10 * S);
                if (SkewButton("##kick", "Kick", ImVec2(170 * S, ImGui::GetFrameHeight() + 6 * S), kRed, ImVec4(1, 1, 1, 1), IcoKick))
                    GUI::RunOnGameThread([SelPC, Reason = std::string(KickReason)]
                                         {
                                             if (IsLivePC(SelPC))
                                                 KickPlayer(SelPC, Reason.c_str());
                                         });
                EndPanel();
            }
            else
            {
                BeginPanel("plaunch", "Launch", IcoBus);
                ImGui::BeginDisabled(!bAlive);
                ImGui::SetNextItemWidth(260 * S);
                ImGui::SliderInt("##launchpower", &LaunchPower, 500, 10000, "Power %d");
                ImGui::SameLine(0, 10 * S);
                if (SkewButton("##launch", "Launch up", ImVec2(170 * S, ImGui::GetFrameHeight() + 6 * S), kYellow, kInk, IcoBus) && bAlive)
                    GUI::RunOnGameThread([SelPC, Power = LaunchPower]
                                         {
                                             auto Pawn = IsLivePC(SelPC) ? (AFortPlayerPawnAthena*)SelPC->Pawn : nullptr;
                                             if (Pawn)
                                                 Pawn->LaunchCharacterJump(FVector(0.0, 0.0, (double)Power), false, nullptr, true);
                                         });
                ImGui::EndDisabled();
                EndPanel();

                BeginPanel("pspawn", "Spawn at player", IcoGift);
                bool bLlama = g_LlamaClass != nullptr, bSupply = g_SupplyClass != nullptr;
                auto SpawnAt = [SelPC](bool bIsLlama)
                {
                    GUI::RunOnGameThread([SelPC, bIsLlama]
                                         {
                                             auto Pawn = IsLivePC(SelPC) ? (AFortPlayerPawnAthena*)SelPC->Pawn : nullptr;
                                             const UClass* Class = bIsLlama ? g_LlamaClass.load() : g_SupplyClass.load();
                                             if (!Pawn || !Class)
                                                 return;
                                             auto Loc = Pawn->K2_GetActorLocation();
                                             if (bIsLlama)
                                                 Loc.X = Loc.X + 300.0;
                                             else
                                                 Loc.Z = Loc.Z + 1500.0;
                                             UWorld::SpawnActor(Class, Loc);
                                         });
                };
                ImGui::BeginDisabled(!bAlive || !bLlama);
                if (SkewButton("##llama", "Llama", ImVec2(140 * S, 42 * S), ImVec4(0.75f, 0.45f, 1.f, 1.f), kInk) && bAlive && bLlama)
                    SpawnAt(true);
                ImGui::EndDisabled();
                ImGui::SameLine(0, 10 * S);
                ImGui::BeginDisabled(!bAlive || !bSupply);
                if (SkewButton("##supply", "Supply drop", ImVec2(170 * S, 42 * S), kBlue, kInk) && bAlive && bSupply)
                    SpawnAt(false);
                ImGui::EndDisabled();
                if (!bLlama || !bSupply)
                    ImGui::TextDisabled("Greyed out = not loaded on this version.");
                EndPanel();
            }
            ImGui::EndChild();
        }
        break;

        case 7:
        {
            ImGui::Text("Bots");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Bot controls coming soon.");
        }
        break;

        case 61:
        {
            PageTitle("Credits", "Made by Sarah (@ustruct on Discord)");

            BeginPanel("credits", "The people behind Boron", IcoMedal);
            ImGui::PushFont(g_FontBold, 17.f);
            ImGui::TextColored(kYellow, "Base / Erbium");
            ImGui::PopFont();
            ImGui::BulletText("Sarah (@ustruct on Discord)");
            ImGui::PushFont(g_FontBold, 17.f);
            ImGui::TextColored(kYellow, "Bosses on C2S4");
            ImGui::PopFont();
            ImGui::BulletText("Andreu");
            ImGui::PushFont(g_FontBold, 17.f);
            ImGui::TextColored(kYellow, "30.40 and other Hi-Season exes");
            ImGui::PopFont();
            ImGui::BulletText("Fmgnio");
            ImGui::PushFont(g_FontBold, 17.f);
            ImGui::TextColored(kYellow, "Building editing + looting reference");
            ImGui::PopFont();
            ImGui::BulletText("Lunar");
            EndPanel();

            BeginPanel("assetcredits", "GUI assets", IcoGift);
            ImGui::PushFont(g_FontBold, 17.f);
            ImGui::TextColored(kYellow, "Icons");
            ImGui::PopFont();
            ImGui::BulletText("Epic Games, Inc. (Fortnite Creative Icon Library)");
            ImGui::PushFont(g_FontBold, 17.f);
            ImGui::TextColored(kYellow, "Fonts");
            ImGui::PopFont();
            ImGui::BulletText("Barlow by The Barlow Project Authors (SIL Open Font License)");
            ImGui::TextDisabled("Fortnite is a trademark of Epic Games, Inc. Boron is not affiliated with or endorsed by Epic Games.");
            EndPanel();

        }

        }

        ImGui::EndChild();

        ImGui::End();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    for (auto& Icon : g_Icons)
        if (Icon)
            Icon->Release();
    g_pSwapChain->Release();
    g_pd3dDeviceContext->Release();
    g_pd3dDevice->Release();
    DestroyWindow(hWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    TerminateProcess(GetCurrentProcess(), 0);
}
