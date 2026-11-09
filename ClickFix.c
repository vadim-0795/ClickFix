// ClickFix 1.3 — mouse double-click chatter filter for Windows
// https://github.com/vadim-0795/ClickFix   (MIT License)
//
// Если кнопка нажата снова слишком быстро после отпускания (меньше порога), это
// «дребезг» изношенного переключателя: такое нажатие и его отпускание блокируются.
// Settings live in the tray menu and are stored in HKCU\Software\ClickFix.
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <windows.h>
#include <shellapi.h>
#include <setupapi.h>
#include <devpropdef.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define APP_VERSION  L"1.3"
#define UPD_API      L"https://api.github.com/repos/vadim-0795/ClickFix/releases/latest"
#define UPD_PAGE     L"https://github.com/vadim-0795/ClickFix/releases/latest"
#define APP_URL      L"https://github.com/vadim-0795/ClickFix"

#define WM_TRAY      (WM_APP + 1)
#define WM_BLOCKED   (WM_APP + 2)
#define WM_UPD_CHECKED (WM_APP + 3)   // wParam: 0 latest, 1 new, 2 error; lParam: manual
#define WM_UPD_DOWNLOADED (WM_APP + 4) // wParam: 1 ok
#ifndef NIN_BALLOONUSERCLICK
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif

#define ID_EXIT      1001
#define ID_RESET     1002
#define ID_AUTOSTART 1003
#define ID_BATREFR   1004
#define ID_PAUSE     1005
#define ID_ABOUT     1006
#define ID_THR_L     2000
#define ID_THR_R     2100
#define ID_THR_M     2200
#define ID_DRAG_L    3000
#define ID_DRAG_R    3001
#define ID_MIDDLE_ON 3002
#define ID_LANG      3100   // +0 auto, +1 ru, +2 en
#define ID_SIDE_GUARD 3200
#define ID_SIDE_OFF   3201
#define ID_UPD_CHECK  3300
#define ID_UPD_AUTO   3301
#define ID_UPD_INSTALL 3302

#define TIMER_BAT    1
#define TIMER_UP_L   2
#define TIMER_UP_R   3
#define TIMER_UP_M   4
#define TIMER_X1     5
#define TIMER_X2     6
#define TIMER_UPD    7

#define SIDE_GUARD_MS 150  // side-button press this close to a right click is treated as false
#define SIDE_HOLD_MS  60   // side-button press is held this long to see if a right click follows

#define REG_KEY      L"Software\\ClickFix"
#define RUN_KEY      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

// Bluetooth battery level (the same value Windows shows in Settings)
DEFINE_DEVPROPKEY(PKEY_BtBattery, 0x104EA319, 0x6EE2, 0x4701, 0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5, 2);
DEFINE_DEVPROPKEY(PKEY_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);
DEFINE_DEVPROPKEY(PKEY_DeviceDesc, 0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 2);

static const DWORD THRESHOLDS[] = {30, 40, 50, 60, 70, 80, 100, 120, 150, 200};
#define NTHR (sizeof(THRESHOLDS) / sizeof(THRESHOLDS[0]))

// ---------------------------------------------------------------- localisation
enum {
    TX_BLOCKED, TX_LEFT, TX_RIGHT, TX_MIDDLE, TX_BAT_NONE, TX_BAT_REFRESH, TX_THRESHOLD,
    TX_DEFAULT, TX_SOFT, TX_HARD, TX_DRAG, TX_MIDDLE_ON, TX_PAUSE, TX_AUTOSTART, TX_LANGUAGE,
    TX_LANG_AUTO, TX_RESET, TX_ABOUT, TX_EXIT, TX_ALREADY, TX_HOOKFAIL, TX_LOWBAT_T, TX_LOWBAT,
    TX_TIP_BAT, TX_PAUSED, TX_ABOUT_TEXT, TX_DEVICE, TX_MS,
    TX_SIDE, TX_SIDE_GUARD, TX_SIDE_OFF, TX_SIDE_BLOCKED,
    TX_UPD_MENU, TX_UPD_CHECK, TX_UPD_AUTO, TX_UPD_INSTALL, TX_UPD_AVAIL_T, TX_UPD_AVAIL,
    TX_UPD_NONE, TX_UPD_ERR, TX_UPD_ASK, TX_UPD_FAIL, TX_UPD_DONE_T, TX_UPD_DONE, TX_UPD_BUSY, TX_COUNT
};
static const wchar_t *STR[2][TX_COUNT] = {
  { // Русский
    L"Заблокировано: левая %ld, правая %ld, средняя %ld",
    L"Левая кнопка", L"Правая кнопка", L"Средняя кнопка (колесо)",
    L"Батарея: нет данных (нет Bluetooth-устройств)", L"Обновить заряд",
    L"Порог: %lu мс", L" (по умолчанию)", L" (мягко)", L" (жёстко)",
    L"Защита перетаскивания (удержание)", L"Фильтровать среднюю кнопку",
    L"Пауза (фильтр выключен)", L"Запускать вместе с Windows", L"Язык / Language",
    L"Автоматически", L"Сбросить счётчик", L"О программе / GitHub", L"Выход",
    L"ClickFix уже запущен (значок в трее).", L"Не удалось установить перехват мыши.",
    L"Низкий заряд батареи", L"%ls: осталось %d%%",
    L"\nБатарея: %d%%", L" — ПАУЗА",
    L"ClickFix " APP_VERSION L"\nФильтр ложных двойных кликов мыши.\n\nОткрыть страницу проекта на GitHub?",
    L"Устройство", L"мс",
    L"Боковые кнопки (назад/вперёд)", L"Блокировать «назад», если он срабатывает от правого клика",
    L"Отключить боковые кнопки полностью", L"Заблокировано боковых (назад/вперёд): %ld",
    L"Обновления", L"Проверить обновления сейчас", L"Проверять обновления автоматически",
    L"Установить обновление %ls", L"Доступна новая версия ClickFix",
    L"Версия %ls. Нажмите здесь, чтобы установить.",
    L"У вас последняя версия (%ls).",
    L"Не удалось проверить обновления. Проверьте подключение к интернету.",
    L"Скачать и установить ClickFix %ls?\nПрограмма перезапустится сама.",
    L"Не удалось установить обновление автоматически.\nОткрыть страницу загрузки, чтобы скачать вручную?",
    L"ClickFix обновлён", L"Установлена версия %ls.", L"Загрузка обновления…"
  },
  { // English
    L"Blocked: left %ld, right %ld, middle %ld",
    L"Left button", L"Right button", L"Middle button (wheel)",
    L"Battery: no data (no Bluetooth devices)", L"Refresh battery",
    L"Threshold: %lu ms", L" (default)", L" (soft)", L" (strict)",
    L"Drag protection (hold)", L"Filter middle button",
    L"Pause (filter off)", L"Start with Windows", L"Язык / Language",
    L"Automatic", L"Reset counter", L"About / GitHub", L"Exit",
    L"ClickFix is already running (tray icon).", L"Failed to install the mouse hook.",
    L"Low battery", L"%ls: %d%% left",
    L"\nBattery: %d%%", L" — PAUSED",
    L"ClickFix " APP_VERSION L"\nMouse double-click chatter filter.\n\nOpen the project page on GitHub?",
    L"Device", L"ms",
    L"Side buttons (back/forward)", L"Block \"back\" when it fires from a right click",
    L"Disable side buttons completely", L"Blocked side (back/forward): %ld",
    L"Updates", L"Check for updates now", L"Check for updates automatically",
    L"Install update %ls", L"A new ClickFix version is available",
    L"Version %ls. Click here to install.",
    L"You have the latest version (%ls).",
    L"Could not check for updates. Please check your internet connection.",
    L"Download and install ClickFix %ls?\nThe program will restart by itself.",
    L"Could not install the update automatically.\nOpen the download page to get it manually?",
    L"ClickFix updated", L"Version %ls installed.", L"Downloading update…"
  }
};
static DWORD g_langSetting = 0;  // 0 auto, 1 ru, 2 en
static int g_lang = 0;           // 0 ru, 1 en
#define T(id) STR[g_lang][id]

static void ApplyLanguage(void) {
    if (g_langSetting == 1) g_lang = 0;
    else if (g_langSetting == 2) g_lang = 1;
    else {
        LANGID l = PRIMARYLANGID(GetUserDefaultUILanguage());
        // Russian UI for Russian and neighbouring languages, English for the rest
        g_lang = (l == LANG_RUSSIAN || l == LANG_UKRAINIAN || l == LANG_BELARUSIAN ||
                  l == LANG_UZBEK || l == LANG_KAZAK || l == LANG_KYRGYZ ||
                  l == LANG_TAJIK || l == LANG_TURKMEN || l == LANG_AZERI ||
                  l == LANG_ARMENIAN || l == LANG_GEORGIAN) ? 0 : 1;
    }
}

// ---------------------------------------------------------------- state
typedef struct {
    DWORD threshold;     // ms
    BOOL  dragProtect;   // hold the release back for `threshold` ms
    BOOL  enabled;
    DWORD lastUp;
    BOOL  blockNextUp;
    BOOL  pendingUp;
    UINT  timer;
    DWORD upFlag;
    volatile LONG blocked;
} BtnState;

static BtnState g_left   = {70, TRUE,  TRUE,  0, FALSE, FALSE, TIMER_UP_L, MOUSEEVENTF_LEFTUP,   0};
static BtnState g_right  = {70, FALSE, TRUE,  0, FALSE, FALSE, TIMER_UP_R, MOUSEEVENTF_RIGHTUP,  0};
static BtnState g_middle = {70, FALSE, FALSE, 0, FALSE, FALSE, TIMER_UP_M, MOUSEEVENTF_MIDDLEUP, 0};

static volatile BOOL g_paused = FALSE;

// Side buttons (XBUTTON1 = back, XBUTTON2 = forward)
typedef struct {
    WORD  xbtn;
    UINT  timer;
    BOOL  pendingDown;   // press held back, waiting to see whether a right click follows
    BOOL  pendingUpAlso; // the release also arrived while held back
    BOOL  blockNextUp;
} SideState;
static SideState g_x1 = {XBUTTON1, TIMER_X1, FALSE, FALSE, FALSE};
static SideState g_x2 = {XBUTTON2, TIMER_X2, FALSE, FALSE, FALSE};
static BOOL g_sideGuard = TRUE;   // block side buttons that fire together with a right click
static BOOL g_sideOff = FALSE;    // disable side buttons entirely
static volatile LONG g_sideBlocked = 0;

// Updates
static BOOL g_updAuto = TRUE;
static wchar_t g_updTag[32] = L"";      // newest version found, e.g. "v1.4"
static wchar_t g_updUrl[1024] = L"";    // its ClickFix.exe download URL
static wchar_t g_updNotified[32] = L"";
static volatile LONG g_updBusy = 0;
static int g_balloonKind = 0;           // 1 = update balloon
static HANDLE g_mutex;
static BOOL  g_rightPhysDown = FALSE;
static DWORD g_rightLastT = 0;
static HHOOK g_hook;
static HWND g_wnd;
static NOTIFYICONDATAW g_nid;
static HICON g_iconOn, g_iconOff;

#define MAXBAT 16
typedef struct { wchar_t name[80]; int pct; } BatInfo;
static BatInfo g_bat[MAXBAT];
static int g_nbat = 0;
static BOOL g_lowWarned = FALSE;

// ---------------------------------------------------------------- settings
static BOOL RegReadDword(LPCWSTR name, DWORD *out) {
    DWORD v, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, REG_KEY, name, RRF_RT_REG_DWORD, NULL, &v, &sz) != ERROR_SUCCESS) return FALSE;
    *out = v; return TRUE;
}
static void RegWriteDword(LPCWSTR name, DWORD v) {
    RegSetKeyValueW(HKEY_CURRENT_USER, REG_KEY, name, REG_DWORD, &v, sizeof(v));
}
static BOOL ValidThr(DWORD v) { return v >= 10 && v <= 500; }

static void LoadSettings(void) {
    DWORD v;
    if (RegReadDword(L"Threshold", &v) && ValidThr(v)) g_left.threshold = g_right.threshold = v;  // v1.0
    if (RegReadDword(L"DragProtect", &v)) g_left.dragProtect = v != 0;                          // v1.0
    if (RegReadDword(L"ThresholdLeft", &v) && ValidThr(v))   g_left.threshold = v;
    if (RegReadDword(L"ThresholdRight", &v) && ValidThr(v))  g_right.threshold = v;
    if (RegReadDword(L"ThresholdMiddle", &v) && ValidThr(v)) g_middle.threshold = v;
    if (RegReadDword(L"DragLeft", &v))     g_left.dragProtect = v != 0;
    if (RegReadDword(L"DragRight", &v))    g_right.dragProtect = v != 0;
    if (RegReadDword(L"FilterMiddle", &v)) g_middle.enabled = v != 0;
    if (RegReadDword(L"Language", &v) && v <= 2) g_langSetting = v;
    if (RegReadDword(L"SideGuard", &v)) g_sideGuard = v != 0;
    if (RegReadDword(L"SideOff", &v))   g_sideOff = v != 0;
    if (RegReadDword(L"AutoUpdate", &v)) g_updAuto = v != 0;
}
static void SaveSettings(void) {
    RegWriteDword(L"ThresholdLeft", g_left.threshold);
    RegWriteDword(L"ThresholdRight", g_right.threshold);
    RegWriteDword(L"ThresholdMiddle", g_middle.threshold);
    RegWriteDword(L"DragLeft", g_left.dragProtect);
    RegWriteDword(L"DragRight", g_right.dragProtect);
    RegWriteDword(L"FilterMiddle", g_middle.enabled);
    RegWriteDword(L"Language", g_langSetting);
    RegWriteDword(L"SideGuard", g_sideGuard);
    RegWriteDword(L"SideOff", g_sideOff);
    RegWriteDword(L"AutoUpdate", g_updAuto);
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

// ---------------------------------------------------------------- battery
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
        name[79] = 0;
        if (!name[0]) wcscpy(name, T(TX_DEVICE));
        BOOL dup = FALSE;
        for (int j = 0; j < g_nbat; j++) if (!wcscmp(g_bat[j].name, name)) { dup = TRUE; break; }
        if (dup) continue;
        wcsncpy(g_bat[g_nbat].name, name, 79);
        g_bat[g_nbat].name[79] = 0;
        g_bat[g_nbat].pct = pct;
        g_nbat++;
    }
    SetupDiDestroyDeviceInfoList(set);
}

static void UpdateTip(void) {
    wchar_t bat[48] = L"";
    if (g_nbat > 0) {
        int lo = 0;
        for (int i = 1; i < g_nbat; i++) if (g_bat[i].pct < g_bat[lo].pct) lo = i;
        swprintf(bat, 48, T(TX_TIP_BAT), g_bat[lo].pct);
    }
    swprintf(g_nid.szTip, 128, L"ClickFix %ls%ls\nL: %ld  R: %ld%ls",
             APP_VERSION, g_paused ? T(TX_PAUSED) : L"",
             g_left.blocked, g_right.blocked + g_middle.blocked + g_sideBlocked, bat);
    g_nid.hIcon = g_paused ? g_iconOff : g_iconOn;
    g_nid.uFlags = NIF_TIP | NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void CheckLowBattery(void) {
    int lo = 101; const wchar_t *who = L"";
    for (int i = 0; i < g_nbat; i++) if (g_bat[i].pct < lo) { lo = g_bat[i].pct; who = g_bat[i].name; }
    if (lo <= 15 && !g_lowWarned) {
        g_lowWarned = TRUE;
        g_balloonKind = 0;
        g_nid.uFlags = NIF_INFO;
        g_nid.dwInfoFlags = NIIF_WARNING;
        wcsncpy(g_nid.szInfoTitle, T(TX_LOWBAT_T), 63);
        swprintf(g_nid.szInfo, 256, T(TX_LOWBAT), who, lo);
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    } else if (lo > 20) {
        g_lowWarned = FALSE;
    }
}

// ---------------------------------------------------------------- click filter
// Send a held-back release to the system
static void FlushUp(BtnState *b) {
    KillTimer(g_wnd, b->timer);
    if (!b->pendingUp) return;
    b->pendingUp = FALSE;
    INPUT in = {0};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = b->upFlag;
    SendInput(1, &in, sizeof(in));  // marked as injected, so our hook lets it through
}
static void FlushSide(SideState *x);
static void FlushAll(void) { FlushUp(&g_left); FlushUp(&g_right); FlushUp(&g_middle); FlushSide(&g_x1); FlushSide(&g_x2); }

static BOOL CountBlocked(BtnState *b) {
    InterlockedIncrement(&b->blocked);
    PostMessageW(g_wnd, WM_BLOCKED, 0, 0);
    return TRUE;
}

static BOOL Filter(BtnState *b, BOOL down, DWORD t) {
    if (!b->enabled) return FALSE;
    if (down) {
        if (b->pendingUp) {
            // The button bounced while being held: drop both the release and this press,
            // so for Windows the button simply stays pressed.
            KillTimer(g_wnd, b->timer);
            b->pendingUp = FALSE;
            return CountBlocked(b);
        }
        if (b->lastUp && (t - b->lastUp) < b->threshold) {
            b->blockNextUp = TRUE;      // swallow the matching release too
            return CountBlocked(b);
        }
        return FALSE;
    }
    if (b->blockNextUp) { b->blockNextUp = FALSE; return TRUE; }
    b->lastUp = t;
    if (b->dragProtect) {
        b->pendingUp = TRUE;
        SetTimer(g_wnd, b->timer, b->threshold, NULL);
        return TRUE;  // release held back; sent by the timer
    }
    return FALSE;
}

static void InjectX(WORD xbtn, BOOL down) {
    INPUT in = {0};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    in.mi.mouseData = xbtn;
    SendInput(1, &in, sizeof(in));
}
// Held-back side press turned out to be real: send it now
static void FlushSide(SideState *x) {
    KillTimer(g_wnd, x->timer);
    if (!x->pendingDown) return;
    x->pendingDown = FALSE;
    InjectX(x->xbtn, TRUE);
    if (x->pendingUpAlso) { x->pendingUpAlso = FALSE; InjectX(x->xbtn, FALSE); }
}
// A right click came while a side press was held back: it was a false "back"
static void CancelSide(SideState *x) {
    if (!x->pendingDown) return;
    KillTimer(g_wnd, x->timer);
    x->pendingDown = FALSE;
    if (x->pendingUpAlso) x->pendingUpAlso = FALSE; else x->blockNextUp = TRUE;
    InterlockedIncrement(&g_sideBlocked);
    PostMessageW(g_wnd, WM_BLOCKED, 0, 0);
}
static BOOL FilterSide(SideState *x, BOOL down, DWORD t) {
    if (down) {
        if (g_sideOff) { x->blockNextUp = TRUE; return TRUE; }
        if (!g_sideGuard) return FALSE;
        if (g_rightPhysDown || (g_rightLastT && t - g_rightLastT < SIDE_GUARD_MS)) {
            x->blockNextUp = TRUE;
            InterlockedIncrement(&g_sideBlocked);
            PostMessageW(g_wnd, WM_BLOCKED, 0, 0);
            return TRUE;
        }
        x->pendingDown = TRUE;
        x->pendingUpAlso = FALSE;
        SetTimer(g_wnd, x->timer, SIDE_HOLD_MS, NULL);
        return TRUE;
    }
    if (x->blockNextUp) { x->blockNextUp = FALSE; return TRUE; }
    if (x->pendingDown) { x->pendingUpAlso = TRUE; return TRUE; }
    return FALSE;
}

static LRESULT CALLBACK MouseProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && !g_paused) {
        MSLLHOOKSTRUCT *m = (MSLLHOOKSTRUCT *)lp;
        if (!(m->flags & LLMHF_INJECTED)) {
            BOOL block = FALSE;
            if (wp == WM_RBUTTONDOWN || wp == WM_RBUTTONUP) {
                g_rightPhysDown = (wp == WM_RBUTTONDOWN);
                g_rightLastT = m->time;
                if (g_sideGuard && wp == WM_RBUTTONDOWN) { CancelSide(&g_x1); CancelSide(&g_x2); }
            }
            switch (wp) {
                case WM_LBUTTONDOWN: block = Filter(&g_left,   TRUE,  m->time); break;
                case WM_LBUTTONUP:   block = Filter(&g_left,   FALSE, m->time); break;
                case WM_RBUTTONDOWN: block = Filter(&g_right,  TRUE,  m->time); break;
                case WM_RBUTTONUP:   block = Filter(&g_right,  FALSE, m->time); break;
                case WM_MBUTTONDOWN: block = Filter(&g_middle, TRUE,  m->time); break;
                case WM_MBUTTONUP:   block = Filter(&g_middle, FALSE, m->time); break;
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP: {
                    SideState *x = HIWORD(m->mouseData) == XBUTTON2 ? &g_x2 : &g_x1;
                    block = FilterSide(x, wp == WM_XBUTTONDOWN, m->time);
                    break;
                }
            }
            if (block) return 1;
        }
    }
    return CallNextHookEx(g_hook, code, wp, lp);
}

// ---------------------------------------------------------------- updates
// Network work runs in a background thread: the mouse hook lives on the main thread
// and must never be blocked.
typedef struct { char *data; DWORD len; } Buf;

static BOOL HttpGet(const wchar_t *url, Buf *out, DWORD maxLen) {
    BOOL ok = FALSE;
    out->data = NULL; out->len = 0;
    URL_COMPONENTSW uc = {sizeof(uc)};
    wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;  uc.dwUrlPathLength = 2048;
    wchar_t extra[1024]; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return FALSE;
    wcsncat(path, extra, 2047 - wcslen(path));

    HINTERNET ses = WinHttpOpen(L"ClickFix/" APP_VERSION, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return FALSE;
    WinHttpSetTimeouts(ses, 10000, 10000, 15000, 30000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER,
                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                          uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : NULL;
    if (req && WinHttpSendRequest(req, L"Accept: application/vnd.github+json, */*\r\n", (DWORD)-1L,
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            && WinHttpReceiveResponse(req, NULL)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            DWORD cap = 65536, avail;
            out->data = (char *)malloc(cap + 1);
            ok = out->data != NULL;
            while (ok && WinHttpQueryDataAvailable(req, &avail) && avail > 0) {
                if (out->len + avail > maxLen) { ok = FALSE; break; }
                if (out->len + avail > cap) {
                    while (out->len + avail > cap) cap *= 2;
                    char *n = (char *)realloc(out->data, cap + 1);
                    if (!n) { ok = FALSE; break; }
                    out->data = n;
                }
                DWORD rd = 0;
                if (!WinHttpReadData(req, out->data + out->len, avail, &rd)) { ok = FALSE; break; }
                out->len += rd;
            }
            if (ok) out->data[out->len] = 0;
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    if (!ok) { free(out->data); out->data = NULL; out->len = 0; }
    return ok;
}

// Read the JSON string value that follows `from` (very small, enough for GitHub's answer)
static BOOL JsonString(const char *from, char *out, size_t cap) {
    const char *p = strchr(from, ':');
    if (!p) return FALSE;
    p = strchr(p, '"');
    if (!p) return FALSE;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) out[i++] = *p++;
    out[i] = 0;
    return *p == '"';
}

static void ParseVersion(const wchar_t *v, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    while (*v && (*v < L'0' || *v > L'9')) v++;
    swscanf(v, L"%d.%d.%d", &out[0], &out[1], &out[2]);
}
static BOOL IsNewer(const wchar_t *tag) {
    int a[3], b[3];
    ParseVersion(tag, a); ParseVersion(APP_VERSION, b);
    for (int i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] > b[i];
    return FALSE;
}

static DWORD WINAPI CheckThread(LPVOID manual) {
    Buf b; int result = 2;
    if (HttpGet(UPD_API, &b, 4 * 1024 * 1024)) {
        char tag[32] = "", url[1024] = "";
        const char *t = strstr(b.data, "\"tag_name\"");
        if (t && JsonString(t + 10, tag, sizeof(tag))) {
            // the first asset whose URL ends with .exe
            const char *p = b.data;
            while ((p = strstr(p, "\"browser_download_url\"")) != NULL) {
                char u[1024];
                if (JsonString(p + 22, u, sizeof(u))) {
                    size_t n = strlen(u);
                    if (n > 4 && !_stricmp(u + n - 4, ".exe")) { strcpy(url, u); break; }
                }
                p += 22;
            }
            MultiByteToWideChar(CP_UTF8, 0, tag, -1, g_updTag, 32);
            MultiByteToWideChar(CP_UTF8, 0, url, -1, g_updUrl, 1024);
            result = IsNewer(g_updTag) ? 1 : 0;
        }
        free(b.data);
    }
    InterlockedExchange(&g_updBusy, 0);
    PostMessageW(g_wnd, WM_UPD_CHECKED, result, (LPARAM)manual);
    return 0;
}

static void StartCheck(BOOL manual) {
    if (InterlockedCompareExchange(&g_updBusy, 1, 0) != 0) return;
    HANDLE th = CreateThread(NULL, 0, CheckThread, (LPVOID)(INT_PTR)manual, 0, NULL);
    if (th) CloseHandle(th); else InterlockedExchange(&g_updBusy, 0);
}

static void ExePaths(wchar_t *exe, wchar_t *neu, wchar_t *old) {
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    swprintf(neu, MAX_PATH + 8, L"%ls.new", exe);
    swprintf(old, MAX_PATH + 8, L"%ls.old", exe);
}

static DWORD WINAPI DownloadThread(LPVOID unused) {
    (void)unused;
    int ok = 0;
    Buf b;
    if (g_updUrl[0] && HttpGet(g_updUrl, &b, 32 * 1024 * 1024)) {
        // must look like a Windows program
        if (b.len > 32 * 1024 && b.data[0] == 'M' && b.data[1] == 'Z') {
            wchar_t exe[MAX_PATH], neu[MAX_PATH + 8], old[MAX_PATH + 8];
            ExePaths(exe, neu, old);
            HANDLE f = CreateFileW(neu, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (f != INVALID_HANDLE_VALUE) {
                DWORD wr = 0;
                ok = WriteFile(f, b.data, b.len, &wr, NULL) && wr == b.len;
                CloseHandle(f);
                if (!ok) DeleteFileW(neu);
            }
        }
        free(b.data);
    }
    InterlockedExchange(&g_updBusy, 0);
    PostMessageW(g_wnd, WM_UPD_DOWNLOADED, ok, 0);
    return 0;
}

static void StartDownload(void) {
    if (InterlockedCompareExchange(&g_updBusy, 1, 0) != 0) return;
    HANDLE th = CreateThread(NULL, 0, DownloadThread, NULL, 0, NULL);
    if (th) CloseHandle(th); else InterlockedExchange(&g_updBusy, 0);
}

static void ShowBalloon(int kind, const wchar_t *title, const wchar_t *text, DWORD flags) {
    g_balloonKind = kind;
    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = flags;
    wcsncpy(g_nid.szInfoTitle, title, 63); g_nid.szInfoTitle[63] = 0;
    wcsncpy(g_nid.szInfo, text, 255); g_nid.szInfo[255] = 0;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void AskInstall(void) {
    wchar_t s[256];
    swprintf(s, 256, T(TX_UPD_ASK), g_updTag);
    if (MessageBoxW(NULL, s, L"ClickFix", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if (!g_updUrl[0]) {  // release has no .exe attached
        ShellExecuteW(NULL, L"open", UPD_PAGE, NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    ShowBalloon(0, L"ClickFix", T(TX_UPD_BUSY), NIIF_INFO);
    StartDownload();
}

static void FailInstall(void) {
    if (MessageBoxW(NULL, T(TX_UPD_FAIL), L"ClickFix", MB_YESNO | MB_ICONWARNING) == IDYES)
        ShellExecuteW(NULL, L"open", UPD_PAGE, NULL, NULL, SW_SHOWNORMAL);
}

// Swap the exe files and start the new version
static void FinishInstall(HWND h) {
    wchar_t exe[MAX_PATH], neu[MAX_PATH + 8], old[MAX_PATH + 8];
    ExePaths(exe, neu, old);
    DeleteFileW(old);
    if (!MoveFileExW(exe, old, MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(neu); FailInstall(); return; }
    if (!MoveFileExW(neu, exe, MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old, exe, MOVEFILE_REPLACE_EXISTING);
        DeleteFileW(neu); FailInstall(); return;
    }
    wchar_t cmd[MAX_PATH + 32];
    swprintf(cmd, MAX_PATH + 32, L"\"%ls\" /updated", exe);
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        // roll back
        MoveFileExW(exe, neu, MOVEFILE_REPLACE_EXISTING);
        MoveFileExW(old, exe, MOVEFILE_REPLACE_EXISTING);
        DeleteFileW(neu); FailInstall(); return;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (g_mutex) { CloseHandle(g_mutex); g_mutex = NULL; }
    DestroyWindow(h);  // the new instance takes over
}

static void OnUpdateChecked(int result, BOOL manual) {
    wchar_t s[256];
    if (result == 1) {
        if (manual) { AskInstall(); return; }
        if (wcscmp(g_updNotified, g_updTag) != 0) {
            wcscpy(g_updNotified, g_updTag);
            swprintf(s, 256, T(TX_UPD_AVAIL), g_updTag);
            ShowBalloon(1, T(TX_UPD_AVAIL_T), s, NIIF_INFO);
        }
    } else if (manual) {
        if (result == 0) {
            swprintf(s, 256, T(TX_UPD_NONE), APP_VERSION);
            MessageBoxW(NULL, s, L"ClickFix", MB_ICONINFORMATION);
        } else {
            MessageBoxW(NULL, T(TX_UPD_ERR), L"ClickFix", MB_ICONWARNING);
        }
    }
}

// ---------------------------------------------------------------- menu
static HMENU ThresholdMenu(BtnState *b, UINT idBase, UINT idExtra, int extraStr, BOOL extraChecked) {
    HMENU m = CreatePopupMenu();
    wchar_t s[96];
    if (idExtra) {
        AppendMenuW(m, MF_STRING | (extraChecked ? MF_CHECKED : 0), idExtra, T(extraStr));
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    for (UINT i = 0; i < NTHR; i++) {
        DWORD v = THRESHOLDS[i];
        swprintf(s, 96, L"%lu %ls%ls", v, T(TX_MS),
                 v == 70 ? T(TX_DEFAULT) : v <= 40 ? T(TX_SOFT) : v >= 120 ? T(TX_HARD) : L"");
        AppendMenuW(m, MF_STRING | (v == b->threshold ? MF_CHECKED : 0), idBase + i, s);
    }
    return m;
}

static void AddButtonMenu(HMENU menu, BtnState *b, int nameStr, UINT idBase, UINT idExtra, int extraStr, BOOL extraChecked) {
    wchar_t s[128], thr[48];
    swprintf(thr, 48, T(TX_THRESHOLD), b->threshold);
    swprintf(s, 128, L"%ls — %ls", T(nameStr), b->enabled ? thr : L"—");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)ThresholdMenu(b, idBase, idExtra, extraStr, extraChecked), s);
}

static void ShowMenu(HWND h) {
    POINT p; GetCursorPos(&p);
    HMENU menu = CreatePopupMenu();
    wchar_t s[160];

    swprintf(s, 160, T(TX_BLOCKED), g_left.blocked, g_right.blocked, g_middle.blocked);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, s);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    AddButtonMenu(menu, &g_left,   TX_LEFT,   ID_THR_L, ID_DRAG_L,    TX_DRAG,      g_left.dragProtect);
    AddButtonMenu(menu, &g_right,  TX_RIGHT,  ID_THR_R, ID_DRAG_R,    TX_DRAG,      g_right.dragProtect);
    AddButtonMenu(menu, &g_middle, TX_MIDDLE, ID_THR_M, ID_MIDDLE_ON, TX_MIDDLE_ON, g_middle.enabled);
    HMENU side = CreatePopupMenu();
    swprintf(s, 160, T(TX_SIDE_BLOCKED), g_sideBlocked);
    AppendMenuW(side, MF_STRING | MF_GRAYED, 0, s);
    AppendMenuW(side, MF_SEPARATOR, 0, NULL);
    AppendMenuW(side, MF_STRING | (g_sideGuard ? MF_CHECKED : 0), ID_SIDE_GUARD, T(TX_SIDE_GUARD));
    AppendMenuW(side, MF_STRING | (g_sideOff ? MF_CHECKED : 0), ID_SIDE_OFF, T(TX_SIDE_OFF));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)side, T(TX_SIDE));
    AppendMenuW(menu, MF_STRING | (g_paused ? MF_CHECKED : 0), ID_PAUSE, T(TX_PAUSE));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    if (g_nbat == 0) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, T(TX_BAT_NONE));
    } else {
        for (int i = 0; i < g_nbat; i++) {
            swprintf(s, 160, L"%ls — %d%%", g_bat[i].name, g_bat[i].pct);
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, s);
        }
    }
    AppendMenuW(menu, MF_STRING, ID_BATREFR, T(TX_BAT_REFRESH));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    HMENU lang = CreatePopupMenu();
    AppendMenuW(lang, MF_STRING | (g_langSetting == 0 ? MF_CHECKED : 0), ID_LANG + 0, T(TX_LANG_AUTO));
    AppendMenuW(lang, MF_STRING | (g_langSetting == 1 ? MF_CHECKED : 0), ID_LANG + 1, L"Русский");
    AppendMenuW(lang, MF_STRING | (g_langSetting == 2 ? MF_CHECKED : 0), ID_LANG + 2, L"English");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)lang, T(TX_LANGUAGE));
    AppendMenuW(menu, MF_STRING | (IsAutostart() ? MF_CHECKED : 0), ID_AUTOSTART, T(TX_AUTOSTART));
    AppendMenuW(menu, MF_STRING, ID_RESET, T(TX_RESET));
    if (g_updTag[0] && IsNewer(g_updTag)) {
        swprintf(s, 160, T(TX_UPD_INSTALL), g_updTag);
        AppendMenuW(menu, MF_STRING | MF_DEFAULT, ID_UPD_INSTALL, s);
    }
    HMENU upd = CreatePopupMenu();
    AppendMenuW(upd, MF_STRING, ID_UPD_CHECK, T(TX_UPD_CHECK));
    AppendMenuW(upd, MF_STRING | (g_updAuto ? MF_CHECKED : 0), ID_UPD_AUTO, T(TX_UPD_AUTO));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)upd, T(TX_UPD_MENU));
    AppendMenuW(menu, MF_STRING, ID_ABOUT, T(TX_ABOUT));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_EXIT, T(TX_EXIT));

    SetForegroundWindow(h);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, h, NULL);
    PostMessageW(h, WM_NULL, 0, 0);
    DestroyMenu(menu);  // also destroys the submenus
}

static void OnCommand(HWND h, UINT id) {
    if (id == ID_EXIT) { DestroyWindow(h); return; }
    if (id == ID_RESET) { g_left.blocked = g_right.blocked = g_middle.blocked = 0; g_sideBlocked = 0; }
    else if (id == ID_UPD_CHECK) { StartCheck(TRUE); return; }
    else if (id == ID_UPD_INSTALL) { AskInstall(); return; }
    else if (id == ID_UPD_AUTO) {
        g_updAuto = !g_updAuto;
        if (g_updAuto) { SetTimer(g_wnd, TIMER_UPD, 24 * 3600 * 1000, NULL); StartCheck(FALSE); }
        else KillTimer(g_wnd, TIMER_UPD);
    }
    else if (id == ID_SIDE_GUARD) { FlushSide(&g_x1); FlushSide(&g_x2); g_sideGuard = !g_sideGuard; }
    else if (id == ID_SIDE_OFF) { FlushSide(&g_x1); FlushSide(&g_x2); g_sideOff = !g_sideOff; }
    else if (id == ID_AUTOSTART) SetAutostart(!IsAutostart());
    else if (id == ID_BATREFR) RefreshBattery();
    else if (id == ID_PAUSE) { FlushAll(); g_paused = !g_paused; }
    else if (id == ID_ABOUT) {
        if (MessageBoxW(NULL, T(TX_ABOUT_TEXT), L"ClickFix", MB_YESNO | MB_ICONINFORMATION) == IDYES)
            ShellExecuteW(NULL, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
    }
    else if (id == ID_DRAG_L) { FlushUp(&g_left);  g_left.dragProtect  = !g_left.dragProtect; }
    else if (id == ID_DRAG_R) { FlushUp(&g_right); g_right.dragProtect = !g_right.dragProtect; }
    else if (id == ID_MIDDLE_ON) { FlushUp(&g_middle); g_middle.enabled = !g_middle.enabled; }
    else if (id >= ID_THR_L && id < ID_THR_L + NTHR) g_left.threshold   = THRESHOLDS[id - ID_THR_L];
    else if (id >= ID_THR_R && id < ID_THR_R + NTHR) g_right.threshold  = THRESHOLDS[id - ID_THR_R];
    else if (id >= ID_THR_M && id < ID_THR_M + NTHR) g_middle.threshold = THRESHOLDS[id - ID_THR_M];
    else if (id >= ID_LANG && id <= ID_LANG + 2) { g_langSetting = id - ID_LANG; ApplyLanguage(); }
    else return;
    SaveSettings();
    UpdateTip();
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_BLOCKED: UpdateTip(); return 0;
    case WM_UPD_CHECKED: OnUpdateChecked((int)wp, (BOOL)lp); return 0;
    case WM_UPD_DOWNLOADED:
        if (wp) FinishInstall(h); else FailInstall();
        return 0;
    case WM_TIMER:
        if (wp == TIMER_BAT) { RefreshBattery(); CheckLowBattery(); UpdateTip(); }
        else if (wp == TIMER_UP_L) FlushUp(&g_left);
        else if (wp == TIMER_UP_R) FlushUp(&g_right);
        else if (wp == TIMER_UP_M) FlushUp(&g_middle);
        else if (wp == TIMER_X1) FlushSide(&g_x1);
        else if (wp == TIMER_X2) FlushSide(&g_x2);
        else if (wp == TIMER_UPD) {
            SetTimer(h, TIMER_UPD, 24 * 3600 * 1000, NULL);  // then once a day
            if (g_updAuto) StartCheck(FALSE);
        }
        return 0;
    case WM_TRAY:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP) ShowMenu(h);
        else if (LOWORD(lp) == NIN_BALLOONUSERCLICK && g_balloonKind == 1) { g_balloonKind = 0; AskInstall(); }
        return 0;
    case WM_COMMAND:
        OnCommand(h, LOWORD(wp));
        return 0;
    case WM_DESTROY:
        KillTimer(h, TIMER_BAT);
        FlushAll();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev; (void)show;
    LoadSettings();
    ApplyLanguage();
    BOOL updated = cmd && wcsstr(cmd, L"/updated") != NULL;
    if (cmd && *cmd && !updated) {
        long v = wcstol(cmd, NULL, 10);
        if (ValidThr((DWORD)v)) g_left.threshold = g_right.threshold = (DWORD)v;
    }

    // After an update the old instance may still be closing: wait for it a little
    for (int tries = 0;; tries++) {
        g_mutex = CreateMutexW(NULL, TRUE, L"ClickFix_SingleInstance");
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
        CloseHandle(g_mutex); g_mutex = NULL;
        if (!updated || tries >= 50) {
            MessageBoxW(NULL, T(TX_ALREADY), L"ClickFix", MB_ICONINFORMATION);
            return 0;
        }
        Sleep(200);
    }
    {   // remove the previous version left after an update
        wchar_t exe[MAX_PATH], neu[MAX_PATH + 8], old[MAX_PATH + 8];
        ExePaths(exe, neu, old);
        DeleteFileW(old); DeleteFileW(neu);
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"ClickFixWnd";
    RegisterClassW(&wc);
    g_wnd = CreateWindowW(L"ClickFixWnd", L"ClickFix", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hi, NULL);

    g_iconOn  = LoadIconW(NULL, (LPCWSTR)IDI_SHIELD);
    g_iconOff = LoadIconW(NULL, (LPCWSTR)IDI_WARNING);

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_wnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_iconOn;
    wcscpy(g_nid.szTip, L"ClickFix");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, hi, 0);
    if (!g_hook) { MessageBoxW(NULL, T(TX_HOOKFAIL), L"ClickFix", MB_ICONERROR); return 1; }

    RefreshBattery(); CheckLowBattery(); UpdateTip();
    SetTimer(g_wnd, TIMER_BAT, 60 * 1000, NULL);
    if (updated) {
        wchar_t s[128];
        swprintf(s, 128, T(TX_UPD_DONE), APP_VERSION);
        ShowBalloon(0, T(TX_UPD_DONE_T), s, NIIF_INFO);
    }
    // first update check 30 s after start (the network may not be up yet at logon)
    if (g_updAuto) SetTimer(g_wnd, TIMER_UPD, 30 * 1000, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    UnhookWindowsHookEx(g_hook);
    if (g_mutex) CloseHandle(g_mutex);
    return 0;
}
