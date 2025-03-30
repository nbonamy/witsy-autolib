#include "mouse.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

uint32_t MouseClick(int x, int y)
{
    // Get current mouse position
    POINT originalPos;
    GetCursorPos(&originalPos);

    printf("Original Mouse Position: %d, %d\n", originalPos.x, originalPos.y);

    // Convert screen coordinates to mouse event coordinates
    double fScreenWidth = GetSystemMetrics(SM_CXSCREEN) - 1;
    double fScreenHeight = GetSystemMetrics(SM_CYSCREEN) - 1;

    printf("Screen Width: %f, Screen Height: %f\n", fScreenWidth, fScreenHeight);
    
    // Prepare input structure for mouse movement and click
    UINT result;
    INPUT input;
    input.type = INPUT_MOUSE;
    input.mi.mouseData = 0;
    input.mi.time = 0;
    input.mi.dwExtraInfo = 0;
    
    // // Move mouse to the specified position
    // input.type = INPUT_MOUSE;
    // input.mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    // input.mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    // input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    // input.mi.mouseData = 0;
    // input.mi.time = 0;
    // input.mi.dwExtraInfo = 0;
    // result = SendInput(1, &input, sizeof(INPUT));
    // if (result != 1) return 0;

    // printf("Mouse moved to: %d, %d\n", input.mi.dx, input.mi.dy);
    
    // Small delay to ensure mouse has moved
    Sleep(10);
    
    // Left button down
    input.mi.dx = (LONG)((x * 65535.0f) / fScreenWidth);
    input.mi.dy = (LONG)((y * 65535.0f) / fScreenHeight);
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) return 0;

    Sleep(10);

    // Left button up
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    result = SendInput(1, &input, sizeof(INPUT));
    if (result != 1) return 0;
    
    printf("Mouse clicked at: %d, %d\n", input.mi.dx, input.mi.dy);

    // // Small delay between click and moving back
    // Sleep(10);

    // // // Move back to original position
    // input.mi.dx = (LONG)((originalPos.x * 65535.0f) / fScreenWidth);
    // input.mi.dy = (LONG)((originalPos.y * 65535.0f) / fScreenHeight);
    // input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    // // result = SendInput(1, &input, sizeof(INPUT));
    // // if (result != 1) return 0;

    // printf("Mouse moved back to: %d, %d\n", input.mi.dx, input.mi.dy);

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
