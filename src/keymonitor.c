#include "keymonitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

// Thread-safe function for calling back to JavaScript
static napi_threadsafe_function g_tsfn = NULL;
static bool g_running = false;

#ifdef __APPLE__

static CFMachPortRef g_eventTap = NULL;
static CFRunLoopSourceRef g_runLoopSource = NULL;
static CFRunLoopRef g_runLoop = NULL;
static pthread_t g_thread;

// Callback that runs on the main JS thread
static void CallJS(napi_env env, napi_value js_callback, void* context, void* data) {
  if (env == NULL || js_callback == NULL) {
    if (data) free(data);
    return;
  }

  KeyEvent* event = (KeyEvent*)data;
  if (event == NULL) return;

  napi_status status;

  // Create the event object to pass to JavaScript
  napi_value eventObj;
  status = napi_create_object(env, &eventObj);
  if (status != napi_ok) {
    free(event);
    return;
  }

  // Add type property
  napi_value typeVal;
  const char* typeStr;
  switch (event->type) {
    case KEY_EVENT_DOWN: typeStr = "down"; break;
    case KEY_EVENT_UP: typeStr = "up"; break;
    case KEY_EVENT_FLAGS_CHANGED: typeStr = "flagsChanged"; break;
    default: typeStr = "unknown"; break;
  }
  napi_create_string_utf8(env, typeStr, NAPI_AUTO_LENGTH, &typeVal);
  napi_set_named_property(env, eventObj, "type", typeVal);

  // Add keyCode property
  napi_value keyCodeVal;
  napi_create_uint32(env, event->keyCode, &keyCodeVal);
  napi_set_named_property(env, eventObj, "keyCode", keyCodeVal);

  // Add flags property (as BigInt since it's 64-bit)
  napi_value flagsVal;
  napi_create_int64(env, (int64_t)event->flags, &flagsVal);
  napi_set_named_property(env, eventObj, "flags", flagsVal);

  // Add isRepeat property
  napi_value isRepeatVal;
  napi_get_boolean(env, event->isRepeat, &isRepeatVal);
  napi_set_named_property(env, eventObj, "isRepeat", isRepeatVal);

  // Call the JavaScript callback
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, js_callback, 1, &eventObj, NULL);

  free(event);
}

// CGEventTap callback - runs on the event tap thread
static CGEventRef EventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon) {
  (void)proxy;
  (void)refcon;

  // Handle tap being disabled (system can disable it if it takes too long)
  if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
    if (g_eventTap) {
      CGEventTapEnable(g_eventTap, true);
    }
    return event;
  }

  // Only process key events and flags changes
  if (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged) {
    return event;
  }

  // Create event data to send to JavaScript
  KeyEvent* keyEvent = (KeyEvent*)malloc(sizeof(KeyEvent));
  if (keyEvent == NULL) {
    return event;
  }

  switch (type) {
    case kCGEventKeyDown:
      keyEvent->type = KEY_EVENT_DOWN;
      break;
    case kCGEventKeyUp:
      keyEvent->type = KEY_EVENT_UP;
      break;
    case kCGEventFlagsChanged:
      keyEvent->type = KEY_EVENT_FLAGS_CHANGED;
      break;
    default:
      keyEvent->type = 0;
      break;
  }

  keyEvent->keyCode = (uint16_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
  keyEvent->flags = (uint64_t)CGEventGetFlags(event);
  keyEvent->isRepeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

  // Queue the call to JavaScript
  if (g_tsfn != NULL) {
    napi_call_threadsafe_function(g_tsfn, keyEvent, napi_tsfn_nonblocking);
  } else {
    free(keyEvent);
  }

  return event;
}

// Thread function to run the event tap run loop
static void* EventTapThread(void* arg) {
  (void)arg;

  g_runLoop = CFRunLoopGetCurrent();
  CFRunLoopAddSource(g_runLoop, g_runLoopSource, kCFRunLoopCommonModes);

  // Run until stopped
  CFRunLoopRun();

  return NULL;
}

int StartKeyMonitor(napi_env env, napi_value callback) {
  if (g_running) {
    return 1; // Already running
  }

  // Create threadsafe function
  napi_value resourceName;
  napi_create_string_utf8(env, "KeyMonitorCallback", NAPI_AUTO_LENGTH, &resourceName);

  napi_status status = napi_create_threadsafe_function(
    env,
    callback,
    NULL,                    // async_resource
    resourceName,            // async_resource_name
    0,                       // max_queue_size (0 = unlimited)
    1,                       // initial_thread_count
    NULL,                    // thread_finalize_data
    NULL,                    // thread_finalize_cb
    NULL,                    // context
    CallJS,                  // call_js_cb
    &g_tsfn
  );

  if (status != napi_ok) {
    printf("Failed to create threadsafe function\n");
    return 2;
  }

  // Create event tap for key events
  CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) |
                          CGEventMaskBit(kCGEventKeyUp) |
                          CGEventMaskBit(kCGEventFlagsChanged);

  g_eventTap = CGEventTapCreate(
    kCGSessionEventTap,           // Tap at session level
    kCGHeadInsertEventTap,        // Insert at head
    kCGEventTapOptionListenOnly,  // Listen only, don't modify events
    eventMask,
    EventTapCallback,
    NULL
  );

  if (g_eventTap == NULL) {
    printf("Failed to create event tap. Make sure accessibility permissions are granted.\n");
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = NULL;
    return 3;
  }

  // Create run loop source
  g_runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_eventTap, 0);
  if (g_runLoopSource == NULL) {
    printf("Failed to create run loop source\n");
    CFRelease(g_eventTap);
    g_eventTap = NULL;
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = NULL;
    return 4;
  }

  // Enable the event tap
  CGEventTapEnable(g_eventTap, true);

  // Start the thread
  if (pthread_create(&g_thread, NULL, EventTapThread, NULL) != 0) {
    printf("Failed to create event tap thread\n");
    CFRelease(g_runLoopSource);
    g_runLoopSource = NULL;
    CFRelease(g_eventTap);
    g_eventTap = NULL;
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = NULL;
    return 5;
  }

  g_running = true;
  return 0;
}

int StopKeyMonitor(void) {
  if (!g_running) {
    return 1; // Not running
  }

  g_running = false;

  // Stop the run loop
  if (g_runLoop != NULL) {
    CFRunLoopStop(g_runLoop);
    g_runLoop = NULL;
  }

  // Wait for thread to finish
  pthread_join(g_thread, NULL);

  // Clean up event tap
  if (g_eventTap != NULL) {
    CGEventTapEnable(g_eventTap, false);
    CFRelease(g_eventTap);
    g_eventTap = NULL;
  }

  // Clean up run loop source
  if (g_runLoopSource != NULL) {
    CFRelease(g_runLoopSource);
    g_runLoopSource = NULL;
  }

  // Release threadsafe function
  if (g_tsfn != NULL) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_release);
    g_tsfn = NULL;
  }

  return 0;
}

bool IsKeyMonitorRunning(void) {
  return g_running;
}

#elif defined(_WIN32)

static HHOOK g_hook = NULL;
static HANDLE g_thread = NULL;
static DWORD g_threadId = 0;

// Callback that runs on the main JS thread
static void CALLBACK CallJS(napi_env env, napi_value js_callback, void* context, void* data) {
  if (env == NULL || js_callback == NULL) {
    if (data) free(data);
    return;
  }

  KeyEvent* event = (KeyEvent*)data;
  if (event == NULL) return;

  napi_status status;

  // Create the event object to pass to JavaScript
  napi_value eventObj;
  status = napi_create_object(env, &eventObj);
  if (status != napi_ok) {
    free(event);
    return;
  }

  // Add type property
  napi_value typeVal;
  const char* typeStr;
  switch (event->type) {
    case KEY_EVENT_DOWN: typeStr = "down"; break;
    case KEY_EVENT_UP: typeStr = "up"; break;
    case KEY_EVENT_FLAGS_CHANGED: typeStr = "flagsChanged"; break;
    default: typeStr = "unknown"; break;
  }
  napi_create_string_utf8(env, typeStr, NAPI_AUTO_LENGTH, &typeVal);
  napi_set_named_property(env, eventObj, "type", typeVal);

  // Add keyCode property
  napi_value keyCodeVal;
  napi_create_uint32(env, event->keyCode, &keyCodeVal);
  napi_set_named_property(env, eventObj, "keyCode", keyCodeVal);

  // Add flags property
  napi_value flagsVal;
  napi_create_int64(env, (int64_t)event->flags, &flagsVal);
  napi_set_named_property(env, eventObj, "flags", flagsVal);

  // Add isRepeat property
  napi_value isRepeatVal;
  napi_get_boolean(env, event->isRepeat, &isRepeatVal);
  napi_set_named_property(env, eventObj, "isRepeat", isRepeatVal);

  // Call the JavaScript callback
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, js_callback, 1, &eventObj, NULL);

  free(event);
}

// Low-level keyboard hook callback
static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode >= 0 && g_tsfn != NULL) {
    KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;

    KeyEvent* keyEvent = (KeyEvent*)malloc(sizeof(KeyEvent));
    if (keyEvent != NULL) {
      switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
          keyEvent->type = KEY_EVENT_DOWN;
          break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
          keyEvent->type = KEY_EVENT_UP;
          break;
        default:
          keyEvent->type = 0;
          break;
      }

      keyEvent->keyCode = (uint16_t)kbStruct->vkCode;
      keyEvent->flags = kbStruct->flags;
      keyEvent->isRepeat = false; // Windows doesn't easily provide this

      napi_call_threadsafe_function(g_tsfn, keyEvent, napi_tsfn_nonblocking);
    }
  }

  return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// Thread function to run the message loop
static DWORD WINAPI HookThread(LPVOID lpParam) {
  (void)lpParam;

  // Install the hook on this thread
  g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (g_hook == NULL) {
    printf("Failed to install keyboard hook\n");
    return 1;
  }

  // Run message loop
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Unhook when done
  if (g_hook != NULL) {
    UnhookWindowsHookEx(g_hook);
    g_hook = NULL;
  }

  return 0;
}

int StartKeyMonitor(napi_env env, napi_value callback) {
  if (g_running) {
    return 1; // Already running
  }

  // Create threadsafe function
  napi_value resourceName;
  napi_create_string_utf8(env, "KeyMonitorCallback", NAPI_AUTO_LENGTH, &resourceName);

  napi_status status = napi_create_threadsafe_function(
    env,
    callback,
    NULL,                    // async_resource
    resourceName,            // async_resource_name
    0,                       // max_queue_size (0 = unlimited)
    1,                       // initial_thread_count
    NULL,                    // thread_finalize_data
    NULL,                    // thread_finalize_cb
    NULL,                    // context
    CallJS,                  // call_js_cb
    &g_tsfn
  );

  if (status != napi_ok) {
    printf("Failed to create threadsafe function\n");
    return 2;
  }

  // Create thread for message loop
  g_thread = CreateThread(NULL, 0, HookThread, NULL, 0, &g_threadId);
  if (g_thread == NULL) {
    printf("Failed to create hook thread\n");
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = NULL;
    return 3;
  }

  g_running = true;
  return 0;
}

int StopKeyMonitor(void) {
  if (!g_running) {
    return 1; // Not running
  }

  g_running = false;

  // Post quit message to the thread
  if (g_threadId != 0) {
    PostThreadMessage(g_threadId, WM_QUIT, 0, 0);
  }

  // Wait for thread to finish
  if (g_thread != NULL) {
    WaitForSingleObject(g_thread, 5000);
    CloseHandle(g_thread);
    g_thread = NULL;
    g_threadId = 0;
  }

  // Release threadsafe function
  if (g_tsfn != NULL) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_release);
    g_tsfn = NULL;
  }

  return 0;
}

bool IsKeyMonitorRunning(void) {
  return g_running;
}

#else

// Stub implementations for unsupported platforms
int StartKeyMonitor(napi_env env, napi_value callback) {
  (void)env;
  (void)callback;
  return 1; // Not supported
}

int StopKeyMonitor(void) {
  return 1; // Not supported
}

bool IsKeyMonitorRunning(void) {
  return false;
}

#endif
