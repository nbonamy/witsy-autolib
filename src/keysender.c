#include "keysender.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#endif

uint32_t SendKey(const char *key, bool useModifier)
{
#ifdef _WIN32
  
  BYTE keyCode = 0;
  if (strcmp(key, "A") == 0) {
    keyCode = 0x41; // VK_A
  } else if (strcmp(key, "C") == 0) {
    keyCode = 0x43; // VK_C
  } else if (strcmp(key, "V") == 0) {
    keyCode = 0x56; // VK_V
  } else if (strcmp(key, "Delete") == 0) {
    keyCode = VK_DELETE; // VK_DELETE
  } else if (strcmp(key, "Enter") == 0) {
    keyCode = VK_RETURN; // VK_RETURN
  } else if (strcmp(key, "Down") == 0) {
    keyCode = VK_DOWN; // VK_DOWN
  } else {
    printf("Unrecognized key: %s\n", key);
    return 1; // Error: Unsupported key
  }

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
    return 2;  // Keys still pressed after timeout
  }

  printf("Sending key: %s%s\n", useModifier ? "Ctrl+" : "", key);

  // make sure window is active
  HWND hwnd = GetForegroundWindow();
  SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);

  // Determine number of inputs needed
  int numInputs = useModifier ? 4 : 2;
  INPUT* inputs = (INPUT*)malloc(numInputs * sizeof(INPUT));
  if (inputs == NULL) {
    printf("Failed to allocate memory for inputs\n");
    return 9;
  }
  memset(inputs, 0, numInputs * sizeof(INPUT));

  // get virtual key mappings
  WORD scanCtrl = MapVirtualKey(VK_CONTROL, 0);
  WORD scanKey = MapVirtualKey(keyCode, 0);
  
  int index = 0;
  
  if (useModifier) {
    // key down control
    inputs[index].type = INPUT_KEYBOARD;
    inputs[index].ki.wVk = VK_CONTROL;
    inputs[index].ki.wScan = scanCtrl;
    inputs[index].ki.dwFlags = 0;
    inputs[index].ki.time = 0;
    inputs[index].ki.dwExtraInfo = 0;
    index++;
  }

  // key down key
  inputs[index].type = INPUT_KEYBOARD;
  inputs[index].ki.wVk = keyCode;
  inputs[index].ki.wScan = scanKey;
  inputs[index].ki.dwFlags = 0;
  inputs[index].ki.time = 0;
  inputs[index].ki.dwExtraInfo = 0;
  index++;

  if (useModifier) {
    // key up control
    inputs[index].type = INPUT_KEYBOARD;
    inputs[index].ki.wVk = VK_CONTROL;
    inputs[index].ki.wScan = scanCtrl;
    inputs[index].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[index].ki.time = 0;
    inputs[index].ki.dwExtraInfo = 0;
    index++;
  }

  // key up key
  inputs[index].type = INPUT_KEYBOARD;
  inputs[index].ki.wVk = keyCode;
  inputs[index].ki.wScan = scanKey;
  inputs[index].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[index].ki.time = 0;
  inputs[index].ki.dwExtraInfo = 0;

  UINT numSent = SendInput(numInputs, inputs, sizeof(INPUT));
  printf("Sent keys: %d\n", numSent);
  
  free(inputs);
  return numSent == numInputs ? 0 : 3;

#elif defined(__APPLE__)
  CGKeyCode keyCode;

  // Define key codes for macOS
  if (strcmp(key, "A") == 0) {
    keyCode = 0; // A key on macOS
  } else if (strcmp(key, "C") == 0) {
    keyCode = 8; // C key on macOS
  } else if (strcmp(key, "V") == 0) {
    keyCode = 9; // V key on macOS
  } else if (strcmp(key, "Delete") == 0) {
    keyCode = 51; // Delete key on macOS
  } else if (strcmp(key, "Enter") == 0) {
    keyCode = 36; // Return key on macOS
  } else if (strcmp(key, "Down") == 0) {
    keyCode = 125; // Down arrow key on macOS
  } else {
    printf("Unrecognized key: %s\n", key);
    return 1; // Error: Unsupported key
  }
  
  // Get the current event source
  CGEventSourceRef sourceRef = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

  // Create events for key (with or without Command)
  CGEventRef keyDown = CGEventCreateKeyboardEvent(sourceRef, keyCode, true);
  CGEventRef keyUp = CGEventCreateKeyboardEvent(sourceRef, keyCode, false);

  // Add Command modifier if requested
  if (useModifier) {
    CGEventSetFlags(keyDown, kCGEventFlagMaskCommand);
    CGEventSetFlags(keyUp, kCGEventFlagMaskCommand);
  }

  // Post the events
  CGEventPost(kCGHIDEventTap, keyDown);
  usleep(20000); // Wait for 20ms
  CGEventPost(kCGHIDEventTap, keyUp);

  // Release the objects
  CFRelease(keyDown);
  CFRelease(keyUp);
  CFRelease(sourceRef);

  return 0; // Success

#else
  // For other platforms, just return success without doing anything
  return 1;
#endif
}
