// ClickFix — фильтр «дребезга» кнопок мыши (ложные двойные клики)
// + показ заряда батареи Bluetooth-устройств.
// Настройки (порог, автозапуск) — в меню значка в трее, хранятся в HKCU\Software\ClickFix.
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <shellapi.h>
#include <setupapi.h>
#include <devpropdef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define WM_TRAY      (WM_APP + 1)
#define WM_BLOCKED   (WM_APP + 2)
#define ID_EXIT      1001
#define ID_RESET     1002
#define ID_AUTOSTART 1003
#define ID_BATREFR   1004
#define ID_THR_BASE  2000
#define TIMER_BAT    1
#define TIMER_UP_L   2
#define TIMER_UP_R   3
#define ID_DRAG      1005
#define REG_KEY      L"Software\\ClickFix"
#define RUN_KEY      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

// Заряд батареи Bluetooth-устройства (то же значение, что показывает Windows в «Параметрах»)
DEFINE_DEVPROPKEY(PKEY_BtBattery, 0x104EA319, 0x6EE2, 0x4701, 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5, 2);
DEFINE_DEVPROPKEY(PKEY_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_DEVPROPKEY(PKEY_DeviceDesc, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);

static const DWORD THRESHOLDS[] = {30, 40, 50, 60, 70, 80, 100, 120, 150, 200};
#define NTHR (sizeof(THRESHOLDS) / sizeof(THRESHOLDS[0]))

static volatile DWORD g_threshold = 70;
static BOOL g_dragProtect = TRUE;  // защита удержания/перетаскивания
static HHOOK g_hook;
static HWND g_wnd;
static NOTIFYICONDATAW g_nid;
static volatile LONG g_blockedL = 0, g_blockedR = 0;

typedef struct { DWORD lastUp; BOOL blockNextUp; BOOL pendingUp; UINT timer; DWORD upFlag; } BtnState;
static BtnState g_left  = {0, FALSE, FALSE, TIMER_UP_L, MOUSEEVENTF_LEFTUP};
static BtnState g_right = {0, FALSE, FALSE, TIMER_UP_R, MOUSEEVENTF_RIGHTUP};

#define MAXBAT 16
typedef struct { wchar_t name[80]; int pct; } BatInfo;
static BatInfo g_bat[MAXBAT];
static int g_nbat = 0;
static BOOL g_lowWarned = FALSE;

// ---------- настройки ----------
static void LoadSettings(void) {
    DWORD v, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, REG_KEY, L"Threshold", RRF_RT_REG_DWORD, NULL, &v, &sz) == ERROR_SUCCESS
        && v >= 10 && v <= 500) g_threshold = v;
    sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, REG_KEY, L"DragProtect", RRF_RT_REG_DWORD, NULL, &v, &sz) == ERROR_SUCCESS)
        g_dragProtect = v != 0;
}
static void SaveSettings(void) {
    DWORD v = g_threshold;
    RegSetKeyValueW(HKEY_CURRENT_USER, REG_KEY, L"Threshold", REG_DWORD, &v, sizeof(v));
    v = g_dragProtect;
    RegSetKeyValueW(HKEY_CURRENT_USER, REG_KEY, L"DragProtect", REG_DWORD, &v, sizeof(v));
}
static BOOL IsAutostart(void) {
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, L"ClickFix", RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
}
static void SetAutostart(BOOL on) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t path[MAX_PATH], val[MAX_PATH + 4];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        swprintf(val, MAX_PATH + 4, L"\"%ls\"", path);
        RegSetValueExW(k, L"ClickFix", 0, REG_SZ, (BYTE *)val, (DWORD)((wcslen(val) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, L"ClickFix");
    }
    RegCloseKey(k);
}

// ---------- батарея ----------
static void RefreshBattery(void) {
    g_nbat = 0;
    HDEVINFO set = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) return;
    SP_DEVINFO_DATA d = {sizeof(d)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &d) && g_nbat < MAXBAT; i++) {
        DEVPROPTYPE t; BYTE pct = 0;
        if (!SetupDiGetDevicePropertyW(set, &d, &PKEY_BtBattery, &t, &pct, sizeof(pct), NULL, 0)) continue;
        if (pct > 100) continue;
        wchar_t name[80] = L"";
        if (!SetupDiGetDevicePropertyW(set, &d, &PKEY_FriendlyName, &t, (BYTE *)name, sizeof(name), NULL, 0))
            SetupDiGetDevicePropertyW(set, &d, &PKEY_DeviceDesc, &t, (BYTE *)name, sizeof(name), NULL, 0);
        if (!name[0]) wcscpy(name, L"Устройство");
        BOOL dup = FALSE;  // одно устройство может встречаться несколько раз
        for (int j = 0; j < g_nbat; j++) if (!wcscmp(g_bat[j].name, name)) { dup = TRUE; break; }
        if (dup) continue;
        wcsncpy(g_bat[g_nbat].name, name, 79);
        g_bat[g_nbat].pct = pct;
        g_nbat++;
    }
    SetupDiDestroyDeviceInfoList(set);
}

static void UpdateTip(void) {
    wchar_t bat[64] = L"";
    if (g_nbat > 0) {
        // В подсказке — первое устройство (или с самым низким зарядом)
        int lo = 0;
        for (int i = 1; i < g_nbat; i++) if (g_bat[i].pct < g_bat[lo].pct) lo = i;
        swprintf(bat, 64, L"\nБатарея: %d%% (%.20ls)", g_bat[lo].pct, g_bat[lo].name);
    }
    swprintf(g_nid.szTip, 128, L"ClickFix, порог %lu мс\nЛевая: %ld  Правая: %ld%ls",
             g_threshold, g_blockedL, g_blockedR, bat);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void CheckLowBattery(void) {
    int lo = 101; const wchar_t *who = L"";
    for (int i = 0; i < g_nbat; i++) if (g_bat[i].pct < lo) { lo = g_bat[i].pct; who = g_bat[i].name; }
    if (lo <= 15 && !g_lowWarned) {
        g_lowWarned = TRUE;
        g_nid.uFlags = NIF_INFO;
        g_nid.dwInfoFlags = NIIF_WARNING;
        wcscpy(g_nid.szInfoTitle, L"Низкий заряд батареи");
        swprintf(g_nid.szInfo, 256, L"%ls: осталось %d%%", who, lo);
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    } else if (lo > 20) {
        g_lowWarned = FALSE;
    }
}

// ---------- фильтр кликов ----------
// Отложенное отпускание: отправить в систему, если за время порога кнопка не нажалась снова
static void FlushUp(BtnState *b) {
    KillTimer(g_wnd, b->timer);
    if (!b->pendingUp) return;
    b->pendingUp = FALSE;
    INPUT in = {0};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = b->upFlag;
    SendInput(1, &in, sizeof(in));  // помечено как injected — наш перехват его пропустит
}

static BOOL Filter(BtnState *b, BOOL down, DWORD t, volatile LONG *counter) {
    if (down) {
        if (b->pendingUp) {
            // Кнопка «отскочила» во время удержания: отпускание и повторное нажатие
            // выбрасываем — для Windows кнопка так и осталась зажатой (перетаскивание не рвётся)
            KillTimer(g_wnd, b->timer);
            b->pendingUp = FALSE;
            InterlockedIncrement(counter);
            PostMessageW(g_wnd, WM_BLOCKED, 0, 0);
            return TRUE;
        }
        if (b->lastUp && (t - b->lastUp) < g_threshold) {
            b->blockNextUp = TRUE;
            InterlockedIncrement(counter);
            PostMessageW(g_wnd, WM_BLOCKED, 0, 0);
            return TRUE;
        }
        return FALSE;
    }
    if (b->blockNextUp) { b->blockNextUp = FALSE; return TRUE; }
    b->lastUp = t;
    if (g_dragProtect) {
        b->pendingUp = TRUE;
        SetTimer(g_wnd, b->timer, g_threshold, NULL);
        return TRUE;  // отпускание придержано, отправим по таймеру
    }
    return FALSE;
}

static LRESULT CALLBACK MouseProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        MSLLHOOKSTRUCT *m = (MSLLHOOKSTRUCT *)lp;
        if (!(m->flags & LLMHF_INJECTED)) {
            BOOL block = FALSE;
            switch (wp) {
                case WM_LBUTTONDOWN: block = Filter(&g_left,  TRUE,  m->time, &g_blockedL); break;
                case WM_LBUTTONUP:   block = Filter(&g_left,  FALSE, m->time, &g_blockedL); break;
                case WM_RBUTTONDOWN: block = Filter(&g_right, TRUE,  m->time, &g_blockedR); break;
                case WM_RBUTTONUP:   block = Filter(&g_right, FALSE, m->time, &g_blockedR); break;
            }
            if (block) return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

// ---------- меню ----------
static void ShowMenu(HWND h) {
    POINT p; GetCursorPos(&p);
    HMENU menu = CreatePopupMenu(), thr = CreatePopupMenu();
    wchar_t s[160];

    swprintf(s, 160, L"Заблокировано: левая %ld, правая %ld", g_blockedL, g_blockedR);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, s);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    if (g_nbat == 0) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Батарея: нет данных (нет Bluetooth-устройств)");
    } else {
        for (int i = 0; i < g_nbat; i++) {
            swprintf(s, 160, L"🔋 %ls — %d%%", g_bat[i].name, g_bat[i].pct);
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, s);
        }
    }
    AppendMenuW(menu, MF_STRING, ID_BATREFR, L"Обновить заряд");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    for (UINT i = 0; i < NTHR; i++) {
        swprintf(s, 160, L"%lu мс%ls", THRESHOLDS[i],
                 THRESHOLDS[i] == 70 ? L" (по умолчанию)" :
                 THRESHOLDS[i] <= 40 ? L" (мягко)" : THRESHOLDS[i] >= 120 ? L" (жёстко)" : L"");
        AppendMenuW(thr, MF_STRING | (THRESHOLDS[i] == g_threshold ? MF_CHECKED : 0), ID_THR_BASE + i, s);
    }
    swprintf(s, 160, L"Порог фильтра: %lu мс", g_threshold);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)thr, s);
    AppendMenuW(menu, MF_STRING | (g_dragProtect ? MF_CHECKED : 0), ID_DRAG, L"Защита перетаскивания (удержание кнопки)");
    AppendMenuW(menu, MF_STRING | (IsAutostart() ? MF_CHECKED : 0), ID_AUTOSTART, L"Запускать вместе с Windows");
    AppendMenuW(menu, MF_STRING, ID_RESET, L"Сбросить счётчик");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_EXIT, L"Выход");

    SetForegroundWindow(h);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, h, NULL);
    DestroyMenu(menu);  // уничтожает и подменю
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_BLOCKED: UpdateTip(); return 0;
    case WM_TIMER:
        if (wp == TIMER_BAT) { RefreshBattery(); CheckLowBattery(); UpdateTip(); }
        else if (wp == TIMER_UP_L) FlushUp(&g_left);
        else if (wp == TIMER_UP_R) FlushUp(&g_right);
        return 0;
    case WM_TRAY:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP) ShowMenu(h);
        return 0;
    case WM_COMMAND: {
        UINT id = LOWORD(wp);
        if (id == ID_EXIT) DestroyWindow(h);
        else if (id == ID_RESET) { g_blockedL = g_blockedR = 0; UpdateTip(); }
        else if (id == ID_AUTOSTART) SetAutostart(!IsAutostart());
        else if (id == ID_DRAG) { g_dragProtect = !g_dragProtect; FlushUp(&g_left); FlushUp(&g_right); SaveSettings(); }
        else if (id == ID_BATREFR) { RefreshBattery(); UpdateTip(); }
        else if (id >= ID_THR_BASE && id < ID_THR_BASE + NTHR) {
            g_threshold = THRESHOLDS[id - ID_THR_BASE];
            SaveSettings(); UpdateTip();
        }
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h, TIMER_BAT);
        FlushUp(&g_left); FlushUp(&g_right);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev; (void)show;
    LoadSettings();
    if (cmd && *cmd) { long v = wcstol(cmd, NULL, 10); if (v >= 10 && v <= 500) g_threshold = (DWORD)v; }

    HANDLE mtx = CreateMutexW(NULL, TRUE, L"ClickFix_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"ClickFix уже запущен (значок в трее).", L"ClickFix", MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"ClickFixWnd";
    RegisterClassW(&wc);
    g_wnd = CreateWindowW(L"ClickFixWnd", L"ClickFix", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hi, NULL);

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_wnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_SHIELD);
    wcscpy(g_nid.szTip, L"ClickFix");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, hi, 0);
    if (!g_hook) { MessageBoxW(NULL, L"Не удалось установить перехват мыши.", L"ClickFix", MB_ICONERROR); return 1; }

    RefreshBattery(); CheckLowBattery(); UpdateTip();
    SetTimer(g_wnd, TIMER_BAT, 60 * 1000, NULL);  // обновлять заряд раз в минуту

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    UnhookWindowsHookEx(g_hook);
    CloseHandle(mtx);
    return 0;
}
