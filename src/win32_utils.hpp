#pragma once

/**
 * Sets the window icon using the Win32 API from the embedded resource (ID 1).
 * This is a helper to avoid header collisions between raylib.h and windows.h.
 */
void SetWin32WindowIcon(void* windowHandle);
