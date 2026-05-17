// maplink.cpp  —  Win32 MapLink GUI
// Build with: make   (requires MinGW-w64 / g++)

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>

// MSVC: auto-link required libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ── Colors ────────────────────────────────────────────────────────────────────
const COLORREF C_BG       = RGB(0x1e,0x23,0x30);
const COLORREF C_PANEL    = RGB(0x26,0x2d,0x3d);
const COLORREF C_ACCENT   = RGB(0x4a,0x9e,0xff);
const COLORREF C_ACCENT2  = RGB(0x2d,0x6b,0xbf);
const COLORREF C_TEXT     = RGB(0xe8,0xea,0xf0);
const COLORREF C_SUBTEXT  = RGB(0x8a,0x93,0xa8);
const COLORREF C_ENTRY_BG = RGB(0x2e,0x37,0x48);
const COLORREF C_SUCCESS  = RGB(0x3e,0xcf,0x8e);

// ── Control / Menu IDs ────────────────────────────────────────────────────────
enum { IDC_NAME=101, IDC_COORDS, IDC_CALTOPO, IDC_DIRS, IDC_BTN, IDC_STATUS };
enum { IDM_CUT=201, IDM_COPY, IDM_PASTE, IDM_SELALL };

// ── Globals ───────────────────────────────────────────────────────────────────
HWND g_hw, g_hName, g_hCoords, g_hCaltopo, g_hDirs, g_hBtn, g_hStatus;
HBRUSH g_brBg, g_brPanel, g_brEntry;
HFONT  g_fUI, g_fBold, g_fLarge;
HWND   g_ctxTarget   = NULL;
COLORREF g_statusColor = C_SUBTEXT;
UINT_PTR g_statusTimer = 0;
bool   g_btnHover    = false;

// Placeholder tracking: each input field stores its hint text and whether
// that hint is currently displayed.
struct PH { HWND h; std::wstring ph; bool active; };
std::vector<PH> g_phs;

// ── Small utilities ───────────────────────────────────────────────────────────
PH* GetPH(HWND h) {
    for (auto& p : g_phs) if (p.h == h) return &p;
    return nullptr;
}

std::wstring GetWText(HWND h) {
    int n = GetWindowTextLengthW(h);
    if (!n) return L"";
    std::wstring s(n + 1, 0);
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(n);
    return s;
}

// Returns the real user text, empty string if placeholder is showing.
std::wstring GetReal(HWND h) {
    PH* p = GetPH(h);
    return (p && p->active) ? L"" : GetWText(h);
}

void Trim(std::wstring& s) {
    auto f = s.find_first_not_of(L" \t\r\n");
    s = (f == std::wstring::npos) ? L"" : s.substr(f);
    auto l = s.find_last_not_of(L" \t\r\n");
    if (l != std::wstring::npos) s = s.substr(0, l + 1);
}

// ── Coordinate parser ─────────────────────────────────────────────────────────
bool ParseCoords(const std::wstring& raw, std::wstring& lat, std::wstring& lon) {
    auto pos = raw.find(L',');
    if (pos == std::wstring::npos) return false;
    lat = raw.substr(0, pos);
    lon = raw.substr(pos + 1);
    Trim(lat); Trim(lon);
    if (lat.empty() || lon.empty()) return false;
    try { std::stod(lat); std::stod(lon); }
    catch (...) { return false; }
    return true;
}

// ── Build clipboard text ──────────────────────────────────────────────────────
std::wstring BuildOutput(const std::wstring& name, const std::wstring& lat,
                         const std::wstring& lon,  const std::wstring& caltopo,
                         const std::wstring& dirs)
{
    std::wstring gmaps = L"https://www.google.com/maps/search/?api=1&query="
                       + lat + L"%2C" + lon;
    std::wostringstream o;
    if (!name.empty())    o << L"Location: "    << name    << L"\r\n";
                          o << L"Google Maps: " << gmaps   << L"\r\n";
    if (!caltopo.empty()) o << L"CalTopo:     " << caltopo << L"\r\n";
    if (!dirs.empty())    o << L"\r\nDirections:\r\n" << dirs;
    return o.str();
}

// ── Clipboard helper ──────────────────────────────────────────────────────────
void ToClipboard(const std::wstring& s) {
    if (!OpenClipboard(g_hw)) return;
    EmptyClipboard();
    size_t b = (s.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, b);
    if (h) {
        memcpy(GlobalLock(h), s.c_str(), b);
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
    }
    CloseClipboard();
}

// ── Font helper ───────────────────────────────────────────────────────────────
HFONT MakeFont(int pts, bool bold, const wchar_t* face) {
    return CreateFontW(
        -MulDiv(pts, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
}

// ── Paint a field label directly onto the window DC ───────────────────────────
void DLabel(HDC dc, const wchar_t* t, int x, int y, int w, COLORREF c, HFONT f) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    RECT r = {x, y, x + w, y + 20};
    DrawTextW(dc, t, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ── Edit-control subclass: placeholders + right-click menu ───────────────────
LRESULT CALLBACK EditSub(HWND h, UINT msg, WPARAM wp, LPARAM lp,
                          UINT_PTR /*id*/, DWORD_PTR /*ref*/)
{
    switch (msg) {

    case WM_SETFOCUS: {
        PH* p = GetPH(h);
        if (p && p->active) {
            SetWindowTextW(h, L"");
            p->active = false;
        }
        break;
    }

    case WM_KILLFOCUS: {
        PH* p = GetPH(h);
        if (p) {
            std::wstring t = GetWText(h);
            Trim(t);
            if (t.empty()) {
                SetWindowTextW(h, p->ph.c_str());
                p->active = true;
            }
        }
        break;
    }

    case WM_CONTEXTMENU: {
        g_ctxTarget = h;
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING,    IDM_CUT,    L"Cut");
        AppendMenuW(m, MF_STRING,    IDM_COPY,   L"Copy");
        AppendMenuW(m, MF_STRING,    IDM_PASTE,  L"Paste");
        AppendMenuW(m, MF_SEPARATOR, 0,           NULL);
        AppendMenuW(m, MF_STRING,    IDM_SELALL, L"Select All");
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hw, NULL);
        DestroyMenu(m);
        return 0;
    }

    }
    return DefSubclassProc(h, msg, wp, lp);
}

// ── Button subclass: hover tracking ──────────────────────────────────────────
LRESULT CALLBACK BtnSub(HWND h, UINT msg, WPARAM wp, LPARAM lp,
                         UINT_PTR /*id*/, DWORD_PTR /*ref*/)
{
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!g_btnHover) {
            g_btnHover = true;
            InvalidateRect(h, NULL, TRUE);
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, h, 0};
            TrackMouseEvent(&tme);
        }
        break;
    case WM_MOUSELEAVE:
        g_btnHover = false;
        InvalidateRect(h, NULL, TRUE);
        break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

// ── Helper: create an edit with placeholder ───────────────────────────────────
HWND MkEdit(HWND par, int id, int x, int y, int w, int h,
            const wchar_t* ph, bool multi = false)
{
    DWORD sty = WS_CHILD | WS_VISIBLE | WS_BORDER;
    sty |= multi
        ? (ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | WS_VSCROLL)
        : ES_AUTOHSCROLL;

    HWND hw = CreateWindowExW(0, L"EDIT", ph, sty,
        x, y, w, h, par, (HMENU)(intptr_t)id,
        GetModuleHandleW(NULL), NULL);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_fUI, TRUE);
    SetWindowSubclass(hw, EditSub, id, 0);
    g_phs.push_back({hw, std::wstring(ph), true});
    return hw;
}

// ── Generate and copy to clipboard ───────────────────────────────────────────
void OnGenerate() {
    std::wstring name = GetReal(g_hName),   coords = GetReal(g_hCoords),
                 cal  = GetReal(g_hCaltopo), dirs   = GetReal(g_hDirs);
    Trim(name); Trim(coords); Trim(cal); Trim(dirs);

    if (coords.empty()) {
        MessageBoxW(g_hw,
            L"Please enter coordinates before copying.",
            L"Missing Coordinates", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring lat, lon;
    if (!ParseCoords(coords, lat, lon)) {
        MessageBoxW(g_hw,
            L"Enter coordinates as: lat, lon\nExample: 35.09742, -106.33096",
            L"Invalid Coordinates", MB_OK | MB_ICONERROR);
        return;
    }
    ToClipboard(BuildOutput(name, lat, lon, cal, dirs));

    SetWindowTextW(g_hStatus, L"\u2713  Copied to clipboard!");
    g_statusColor = C_SUCCESS;
    InvalidateRect(g_hStatus, NULL, TRUE);
    if (g_statusTimer) KillTimer(g_hw, 1);
    g_statusTimer = SetTimer(g_hw, 1, 3000, NULL);
}

// ── Main window procedure ─────────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    // ── Setup ─────────────────────────────────────────────────────────────────
    case WM_CREATE: {
        g_hw      = hw;
        g_brBg    = CreateSolidBrush(C_BG);
        g_brPanel = CreateSolidBrush(C_PANEL);
        g_brEntry = CreateSolidBrush(C_ENTRY_BG);
        g_fUI     = MakeFont(10, false, L"Segoe UI");
        g_fBold   = MakeFont( 9, true,  L"Segoe UI");
        g_fLarge  = MakeFont(14, true,  L"Segoe UI");

        const int X = 24, W = 472;
        int y = 84;

        // Four input fields
        g_hName    = MkEdit(hw, IDC_NAME,    X, y+22, W, 30,
                            L"e.g. Trailhead Parking");             y += 64;
        g_hCoords  = MkEdit(hw, IDC_COORDS,  X, y+22, W, 30,
                            L"e.g. 35.09742, -106.33096");          y += 64;
        g_hCaltopo = MkEdit(hw, IDC_CALTOPO, X, y+22, W, 30,
                            L"e.g. https://caltopo.com/m/XXXX");    y += 64;
        g_hDirs    = MkEdit(hw, IDC_DIRS,    X, y+22, W, 110,
                            L"Optional turn-by-turn notes or landmarks\u2026",
                            true);                                   y += 152;

        // Status label
        g_hStatus = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            X, y, 280, 28, hw, (HMENU)IDC_STATUS,
            GetModuleHandleW(NULL), NULL);
        SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_fUI, TRUE);

        // Copy button (owner-drawn for custom color)
        g_hBtn = CreateWindowExW(0, L"BUTTON",
            L"Copy to Clipboard  \u29c9",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            316, y-4, 180, 36, hw, (HMENU)IDC_BTN,
            GetModuleHandleW(NULL), NULL);
        SendMessageW(g_hBtn, WM_SETFONT, (WPARAM)g_fBold, TRUE);
        SetWindowSubclass(g_hBtn, BtnSub, IDC_BTN, 0);

        return 0;
    }

    // ── Painting ──────────────────────────────────────────────────────────────
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hw, &ps);
        RECT rc; GetClientRect(hw, &rc);

        // Background
        FillRect(dc, &rc, g_brBg);

        // Header panel
        RECT hdr = {0, 0, rc.right, 64};
        FillRect(dc, &hdr, g_brPanel);

        // Title
        SelectObject(dc, g_fLarge);
        SetTextColor(dc, C_ACCENT);
        SetBkMode(dc, TRANSPARENT);
        RECT tr = {24, 12, 230, 52};
        DrawTextW(dc, L"MapLink", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Subtitle
        SelectObject(dc, g_fUI);
        SetTextColor(dc, C_SUBTEXT);
        RECT sr = {140, 12, rc.right - 24, 52};
        DrawTextW(dc, L"Generate tappable map links for email",
                  -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Field labels
        struct { int y; const wchar_t* t; } lbls[] = {
            { 84,  L"Location Name"                              },
            { 148, L"Coordinates  (lat, lon)"                   },
            { 212, L"CalTopo Link  (paste your existing map URL)"},
            { 276, L"Directions / Notes"                        },
        };
        for (auto& lb : lbls)
            DLabel(dc, lb.t, 24, lb.y, 472, C_SUBTEXT, g_fBold);

        EndPaint(hw, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   // prevent flicker; WM_PAINT handles everything

    // ── Control colors ────────────────────────────────────────────────────────
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp; HWND h = (HWND)lp;
        PH* p = GetPH(h);
        SetBkColor(dc, C_ENTRY_BG);
        SetTextColor(dc, (p && p->active) ? C_SUBTEXT : C_TEXT);
        return (LRESULT)g_brEntry;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; HWND h = (HWND)lp;
        SetBkMode(dc, TRANSPARENT);
        SetBkColor(dc, C_BG);
        SetTextColor(dc, (h == g_hStatus) ? g_statusColor : C_SUBTEXT);
        return (LRESULT)g_brBg;
    }

    // ── Owner-draw button ─────────────────────────────────────────────────────
    case WM_DRAWITEM: {
        auto* d = (DRAWITEMSTRUCT*)lp;
        if (d->CtlID != IDC_BTN) break;
        bool pressed = (d->itemState & ODS_SELECTED) != 0;
        COLORREF bg = (pressed || g_btnHover) ? C_ACCENT2 : C_ACCENT;
        HBRUSH br = CreateSolidBrush(bg);
        FillRect(d->hDC, &d->rcItem, br);
        DeleteObject(br);
        SelectObject(d->hDC, g_fBold);
        SetTextColor(d->hDC, RGB(255, 255, 255));
        SetBkMode(d->hDC, TRANSPARENT);
        DrawTextW(d->hDC, L"Copy to Clipboard  \u29c9", -1,
                  &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }

    // ── Commands ──────────────────────────────────────────────────────────────
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN:
            OnGenerate();
            break;
        case IDM_CUT: {
            PH* p = g_ctxTarget ? GetPH(g_ctxTarget) : nullptr;
            if (g_ctxTarget && !(p && p->active))
                SendMessageW(g_ctxTarget, WM_CUT, 0, 0);
            break;
        }
        case IDM_COPY: {
            PH* p = g_ctxTarget ? GetPH(g_ctxTarget) : nullptr;
            if (g_ctxTarget && !(p && p->active))
                SendMessageW(g_ctxTarget, WM_COPY, 0, 0);
            break;
        }
        case IDM_PASTE:
            if (g_ctxTarget) {
                PH* p = GetPH(g_ctxTarget);
                if (p && p->active) {
                    SetWindowTextW(g_ctxTarget, L"");
                    p->active = false;
                }
                SendMessageW(g_ctxTarget, WM_PASTE, 0, 0);
            }
            break;
        case IDM_SELALL:
            if (g_ctxTarget)
                SendMessageW(g_ctxTarget, EM_SETSEL, 0, -1);
            break;
        }
        return 0;

    // ── Status reset timer ────────────────────────────────────────────────────
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(hw, 1);
            g_statusTimer = 0;
            SetWindowTextW(g_hStatus, L"");
            g_statusColor = C_SUBTEXT;
            InvalidateRect(g_hStatus, NULL, TRUE);
        }
        return 0;

    // ── Cleanup ───────────────────────────────────────────────────────────────
    case WM_DESTROY:
        DeleteObject(g_brBg);    DeleteObject(g_brPanel); DeleteObject(g_brEntry);
        DeleteObject(g_fUI);     DeleteObject(g_fBold);   DeleteObject(g_fLarge);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

// ── Entry point ───────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&ic);

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"MapLinkWnd";
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // Let Windows calculate the exact window size for a 520x500 client area
    DWORD sty = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT wr = {0, 0, 520, 500};
    AdjustWindowRect(&wr, sty, FALSE);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    int cx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int cy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;

    HWND hw = CreateWindowExW(WS_EX_APPWINDOW, L"MapLinkWnd", L"MapLink",
                              sty, cx, cy, ww, wh,
                              NULL, NULL, hInst, NULL);
    ShowWindow(hw, nShow);
    UpdateWindow(hw);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
