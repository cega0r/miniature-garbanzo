#pragma once
#include <windows.h>
#include <string>

inline HWND FindMinecraftWindow() {
    const wchar_t* titles[] = {
        L"Minecraft 1.21.4",
        L"Minecraft 1.21",
        L"Minecraft",
    };
    for (auto& t : titles) {
        HWND h = FindWindowW(nullptr, t);
        if (h) return h;
    }
    HWND found = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!proc) return TRUE;
        wchar_t name[MAX_PATH]{};
        DWORD sz = MAX_PATH;
        QueryFullProcessImageNameW(proc, 0, name, &sz);
        CloseHandle(proc);
        std::wstring n(name);
        if (n.find(L"javaw") != std::wstring::npos && IsWindowVisible(hwnd)) {
            wchar_t title[256]{};
            GetWindowTextW(hwnd, title, 256);
            if (wcslen(title) > 0) {
                *(HWND*)lp = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, (LPARAM)&found);
    return found;
}

inline void SyncOverlayToGame(HWND overlay, HWND game,
                               float& outW, float& outH,
                               bool& outMinimized) {
    if (!game || !IsWindow(game)) return;
    outMinimized = IsIconic(game);
    if (outMinimized) { ShowWindow(overlay, SW_HIDE); return; }
    ShowWindow(overlay, SW_SHOW);
    RECT r; GetWindowRect(game, &r);
    outW = (float)(r.right  - r.left);
    outH = (float)(r.bottom - r.top);
    SetWindowPos(overlay, HWND_TOPMOST,
                 r.left, r.top, (int)outW, (int)outH,
                 SWP_NOACTIVATE);
}
