#include "keysender.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#endif

uint32_t SendCtrlKey()
{
#ifdef _WIN32
  
  BYTE keyCode = 0x43;
  // if (strcmp(key, "C") == 0) {
  //   keyCode = 0x43; // VK_C
  // } else if (strcmp(key, "V") == 0) {
  //   keyCode = 0x56; // VK_V
  // } else {
  //   printf("Unrecognized key: %s\n", key);
  //   return 0; // Error: Unsupported key
  // }

  // before sending input, wait for existing key presses to be released
  const int MAX_ATTEMPTS = 50; // 50 * 10ms = 500ms timeout
  BOOL keysPressed = TRUE;
  int attempts = 0;
  
  while (keysPressed && attempts < MAX_ATTEMPTS) {
    keysPressed = FALSE;
    for (int i = 0; i < 256; i++) {
      if (GetAsyncKeyState(i) & 0x8000) {
        keysPressed = TRUE;
        break;
      }
    }
    
    if (keysPressed) {
      Sleep(10);
      attempts++;
    }
  }
  
  if (keysPressed) {
    printf("Failed: keys still pressed after timeout\n");
    return 0;  // Keys still pressed after timeout
  }

  // printf("Sending key: %s\n", key);

  // make sure window is active
  HWND hwnd = GetForegroundWindow();
  SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);

  // inputs
  INPUT inputs[4] = {0};

  // get virtual key mappings
  WORD scanCtrl = MapVirtualKey(VK_CONTROL, 0);
  WORD scanKey = MapVirtualKey(keyCode, 0);

  // key down control
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;
  inputs[0].ki.wScan = scanCtrl;
  inputs[0].ki.dwFlags = 0;
  inputs[0].ki.time = 0;
  inputs[0].ki.dwExtraInfo = 0;

  // key down key
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = keyCode;
  inputs[1].ki.wScan = scanKey;
  inputs[1].ki.dwFlags = 0;
  inputs[1].ki.time = 0;
  inputs[1].ki.dwExtraInfo = 0;

  // key up control
  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = VK_CONTROL;
  inputs[2].ki.wScan = scanCtrl;
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[2].ki.time = 0;
  inputs[2].ki.dwExtraInfo = 0;

  // key up key
  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = keyCode;
  inputs[3].ki.wScan = scanKey;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[3].ki.time = 0;
  inputs[3].ki.dwExtraInfo = 0;

  UINT numSent = SendInput(4, inputs, sizeof(INPUT));
  printf("Sent keys: %d\n", numSent);
  return numSent == 4 ? 1 : 0;

#elif defined(__APPLE__)
  CGKeyCode keyCode = 8;

  // // Define key codes for C and V on macOS
  // if (strcmp(key, "C") == 0) {
  //   keyCode = 8; // C key on macOS
  // } else if (strcmp(key, "V") == 0) {
  //   keyCode = 9; // V key on macOS
  // } else {
  //   printf("Unrecognized key: %s\n", key);
  //   return 0; // Error: Unsupported key
  // }

  // Get the current event source
  CGEventSourceRef sourceRef = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

  // Create events for Command + key
  CGEventRef keyDown = CGEventCreateKeyboardEvent(sourceRef, keyCode, true);
  CGEventRef keyUp = CGEventCreateKeyboardEvent(sourceRef, keyCode, false);

  // Add Command modifier
  CGEventSetFlags(keyDown, kCGEventFlagMaskCommand);
  CGEventSetFlags(keyUp, kCGEventFlagMaskCommand);

  // Post the events
  CGEventPost(kCGHIDEventTap, keyDown);
  usleep(20000); // Wait for 20ms
  CGEventPost(kCGHIDEventTap, keyUp);

  // Release the objects
  CFRelease(keyDown);
  CFRelease(keyUp);
  CFRelease(sourceRef);

  return 1; // Success

#else
  // For other platforms, just return success without doing anything
  return 1;
#endif
}