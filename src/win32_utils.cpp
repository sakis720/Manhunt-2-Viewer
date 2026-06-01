#include "win32_utils.hpp"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void SetWin32WindowIcon(void* windowHandle) {
    HWND hwnd = (HWND)windowHandle;
    // Load the icon with ID 1 from the resources
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    if (hIcon) {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }
}
#else
void SetWin32WindowIcon(void* windowHandle) {
    // No-op for non-Windows platforms
}
#endif
