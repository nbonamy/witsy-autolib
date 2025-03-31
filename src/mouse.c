#include "mouse.h"
#include <stdio.h>

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
    INPUT input[4] = {0};
    UINT result;
    
    // Move mouse to the specified position
    input[0].type = INPUT_MOUSE;
    input[0].mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    input[0].mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    input[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input[0].mi.mouseData = 0;
    input[0].mi.time = 0;
    input[0].mi.dwExtraInfo = 0;

    // Left button down
    input[1].type = INPUT_MOUSE;
    input[1].mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    input[1].mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
    input[1].mi.mouseData = 0;
    input[1].mi.time = 0;
    input[1].mi.dwExtraInfo = 0;

    // Left button up
    input[2].type = INPUT_MOUSE;
    input[2].mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    input[2].mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    input[2].mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
    input[2].mi.mouseData = 0;
    input[2].mi.time = 0;
    input[2].mi.dwExtraInfo = 0;

    // Move back to original position
    input[3].type = INPUT_MOUSE;
    input[3].mi.dx = (LONG)((originalPos.x * 65535.0f) / fScreenWidth);
    input[3].mi.dy = (LONG)((originalPos.y * 65535.0f) / fScreenHeight);
    input[3].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input[3].mi.mouseData = 0;
    input[3].mi.time = 0;
    input[3].mi.dwExtraInfo = 0;

    // Now send
    result = SendInput(4, input, sizeof(INPUT));
    return (result == 4) ? 1 : 0;

}

#else

uint32_t MouseClick(int x, int y)
{
    // Not implemented for platforms other than Windows
    return 0;
}

#endif
