#include "mouse.h"

#ifdef _WIN32
#include <windows.h>

uint32_t MouseClick(int x, int y)
{
    // Get current mouse position
    POINT originalPos;
    GetCursorPos(&originalPos);

    // Convert screen coordinates to mouse event coordinates
    double fScreenWidth = GetSystemMetrics(SM_CXSCREEN) - 1;
    double fScreenHeight = GetSystemMetrics(SM_CYSCREEN) - 1;
    
    // Prepare input structure for mouse movement and click
    INPUT input;
    UINT result;
    
    // Move mouse to the specified position
    input.type = INPUT_MOUSE;
    input.mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    input.mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) return 0;
    
    // Left button down
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
    result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) return 0;

    // Left button up
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
    result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) return 0;

    // // Move back to original position
    // input.mi.dx = (LONG)((originalPos.x * 65535.0f) / fScreenWidth);
    // input.mi.dy = (LONG)((originalPos.y * 65535.0f) / fScreenHeight);
    // input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    // result = SendInput(1, &input, sizeof(INPUT));
    // if (result != 1) return 0;

    // Success
    return 1;
}

#else

uint32_t MouseClick(int x, int y)
{
    // Not implemented for platforms other than Windows
    return 0;
}

#endif
