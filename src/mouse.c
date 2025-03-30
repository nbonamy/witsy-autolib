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
    INPUT input[5] = {0};
    
    // Move mouse to the specified position
    input[0].type = INPUT_MOUSE;
    input[0].mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    input[0].mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    input[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    
    // Left button down
    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    
    // Left button up
    input[2].type = INPUT_MOUSE;
    input[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    // Move back to original position
    input[3].type = INPUT_MOUSE;
    input[3].mi.dx = (LONG)((originalPos.x * 65535.0f) / fScreenWidth);
    input[3].mi.dy = (LONG)((originalPos.y * 65535.0f) / fScreenHeight);
    input[3].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    
    // Send the input events
    UINT result = SendInput(4, input, sizeof(INPUT));
    
    // Return success if all inputs were sent
    return (result == 4) ? 1 : 0;
}

#else

uint32_t MouseClick(int x, int y)
{
    // Not implemented for platforms other than Windows
    return 0;
}

#endif
