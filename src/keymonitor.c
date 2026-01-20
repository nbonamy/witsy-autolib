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

#ifdef __linux__
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <linux/input.h>
#include <sys/select.h>
#include <errno.h>
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
static bool g_keyState[256] = {false};

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
    BYTE vkCode = (BYTE)kbStruct->vkCode;

    // Filter out key repeats - only send initial keydown and keyup
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
      if (g_keyState[vkCode]) {
        // Key already down, this is a repeat - skip it
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
      }
      g_keyState[vkCode] = true;
    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
      g_keyState[vkCode] = false;
    }

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
      keyEvent->isRepeat = false;

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

  // Reset key state
  memset(g_keyState, 0, sizeof(g_keyState));

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

#elif defined(__linux__)

static struct libevdev *g_evdev = NULL;
static int g_fd = -1;
static pthread_t g_thread;
static volatile bool g_stop_requested = false;
static uint64_t g_modifier_flags = 0;

// Linux modifier flag bits (matching common conventions)
#define LINUX_FLAG_SHIFT     (1ULL << 0)
#define LINUX_FLAG_CTRL      (1ULL << 2)
#define LINUX_FLAG_ALT       (1ULL << 3)
#define LINUX_FLAG_META      (1ULL << 6)  // Super/Windows key
#define LINUX_FLAG_CAPSLOCK  (1ULL << 16)

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

// Check if a key code is a modifier key and return the flag bit
static uint64_t GetModifierFlag(int keycode) {
  switch (keycode) {
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
      return LINUX_FLAG_SHIFT;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
      return LINUX_FLAG_CTRL;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
      return LINUX_FLAG_ALT;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
      return LINUX_FLAG_META;
    case KEY_CAPSLOCK:
      return LINUX_FLAG_CAPSLOCK;
    default:
      return 0;
  }
}

// Find a keyboard device
static int FindKeyboardDevice(char *path, size_t path_size) {
  DIR *dir;
  struct dirent *entry;
  char device_path[512];
  int fd;
  struct libevdev *dev = NULL;

  // First try /dev/input/by-id/ for named devices
  dir = opendir("/dev/input/by-id");
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strstr(entry->d_name, "-kbd") != NULL ||
          strstr(entry->d_name, "keyboard") != NULL) {
        snprintf(device_path, sizeof(device_path), "/dev/input/by-id/%s", entry->d_name);
        fd = open(device_path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
          if (libevdev_new_from_fd(fd, &dev) == 0) {
            if (libevdev_has_event_type(dev, EV_KEY) &&
                libevdev_has_event_code(dev, EV_KEY, KEY_A)) {
              libevdev_free(dev);
              close(fd);
              closedir(dir);
              strncpy(path, device_path, path_size - 1);
              path[path_size - 1] = '\0';
              return 0;
            }
            libevdev_free(dev);
          }
          close(fd);
        }
      }
    }
    closedir(dir);
  }

  // Fallback: scan /dev/input/event* devices
  dir = opendir("/dev/input");
  if (dir == NULL) {
    return -1;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (strncmp(entry->d_name, "event", 5) != 0) {
      continue;
    }

    snprintf(device_path, sizeof(device_path), "/dev/input/%s", entry->d_name);
    fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      continue;
    }

    if (libevdev_new_from_fd(fd, &dev) == 0) {
      // Check if this device has keyboard keys
      if (libevdev_has_event_type(dev, EV_KEY) &&
          libevdev_has_event_code(dev, EV_KEY, KEY_A) &&
          libevdev_has_event_code(dev, EV_KEY, KEY_Z)) {
        libevdev_free(dev);
        close(fd);
        closedir(dir);
        strncpy(path, device_path, path_size - 1);
        path[path_size - 1] = '\0';
        return 0;
      }
      libevdev_free(dev);
    }
    close(fd);
  }

  closedir(dir);
  return -1;
}

// Thread function to read keyboard events
static void* KeyboardThread(void* arg) {
  (void)arg;

  struct input_event ev;
  int rc;
  fd_set fds;
  struct timeval tv;

  while (!g_stop_requested) {
    // Use select with timeout to allow checking g_stop_requested
    FD_ZERO(&fds);
    FD_SET(g_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;  // 100ms timeout

    int sel = select(g_fd + 1, &fds, NULL, NULL, &tv);
    if (sel < 0) {
      break;  // Error
    }
    if (sel == 0) {
      continue;  // Timeout, check g_stop_requested
    }

    rc = libevdev_next_event(g_evdev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

    if (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC) {
      if (ev.type == EV_KEY) {
        KeyEvent* keyEvent = (KeyEvent*)malloc(sizeof(KeyEvent));
        if (keyEvent != NULL) {
          uint64_t modFlag = GetModifierFlag(ev.code);

          // Determine event type
          if (ev.value == 1) {  // Key down
            keyEvent->type = KEY_EVENT_DOWN;
            if (modFlag) {
              g_modifier_flags |= modFlag;
            }
          } else if (ev.value == 0) {  // Key up
            keyEvent->type = KEY_EVENT_UP;
            if (modFlag) {
              g_modifier_flags &= ~modFlag;
            }
          } else if (ev.value == 2) {  // Key repeat
            keyEvent->type = KEY_EVENT_DOWN;
            keyEvent->isRepeat = true;
          }

          if (ev.value != 2) {
            keyEvent->isRepeat = false;
          }

          // For modifier keys, also send flagsChanged
          if (modFlag && ev.value != 2) {
            keyEvent->type = KEY_EVENT_FLAGS_CHANGED;
          }

          keyEvent->keyCode = (uint16_t)ev.code;
          keyEvent->flags = g_modifier_flags;

          if (g_tsfn != NULL) {
            napi_call_threadsafe_function(g_tsfn, keyEvent, napi_tsfn_nonblocking);
          } else {
            free(keyEvent);
          }
        }
      }
    } else if (rc < 0 && rc != -EAGAIN) {
      // Error reading event
      break;
    }
  }

  return NULL;
}

int StartKeyMonitor(napi_env env, napi_value callback) {
  fprintf(stderr, "[keymonitor] StartKeyMonitor called, g_running=%d\n", g_running); fflush(stderr);
  if (g_running) {
    fprintf(stderr, "[keymonitor] Already running, returning 1\n"); fflush(stderr);
    return 1; // Already running
  }

  // Find keyboard device
  char device_path[512];
  fprintf(stderr, "[keymonitor] Looking for keyboard device...\n"); fflush(stderr);
  if (FindKeyboardDevice(device_path, sizeof(device_path)) != 0) {
    fprintf(stderr, "[keymonitor] Failed to find keyboard device. Make sure you have permission to access /dev/input devices.\n"); fflush(stderr);
    return 3;
  }
  fprintf(stderr, "[keymonitor] Found keyboard device: %s\n", device_path); fflush(stderr);

  // Open the device (non-blocking for select-based reading)
  g_fd = open(device_path, O_RDONLY | O_NONBLOCK);
  if (g_fd < 0) {
    fprintf(stderr, "[keymonitor] Failed to open keyboard device: %s (errno=%d)\n", device_path, errno); fflush(stderr);
    return 3;
  }
  fprintf(stderr, "[keymonitor] Opened device fd=%d\n", g_fd); fflush(stderr);

  // Create libevdev instance
  if (libevdev_new_from_fd(g_fd, &g_evdev) != 0) {
    printf("Failed to create libevdev instance\n");
    close(g_fd);
    g_fd = -1;
    return 3;
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
    libevdev_free(g_evdev);
    g_evdev = NULL;
    close(g_fd);
    g_fd = -1;
    return 2;
  }

  g_stop_requested = false;
  g_modifier_flags = 0;

  // Start the thread
  if (pthread_create(&g_thread, NULL, KeyboardThread, NULL) != 0) {
    printf("Failed to create keyboard thread\n");
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = NULL;
    libevdev_free(g_evdev);
    g_evdev = NULL;
    close(g_fd);
    g_fd = -1;
    return 5;
  }

  g_running = true;
  printf("[keymonitor] Started successfully\n");
  return 0;
}

int StopKeyMonitor(void) {
  printf("[keymonitor] StopKeyMonitor called, g_running=%d\n", g_running);
  if (!g_running) {
    printf("[keymonitor] Not running, returning 1\n");
    return 1; // Not running
  }

  g_running = false;
  g_stop_requested = true;

  // Wait for thread to finish
  pthread_join(g_thread, NULL);

  // Clean up libevdev
  if (g_evdev != NULL) {
    libevdev_free(g_evdev);
    g_evdev = NULL;
  }

  // Close device
  if (g_fd >= 0) {
    close(g_fd);
    g_fd = -1;
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
