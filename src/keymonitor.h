#ifndef KEYMONITOR_H
#define KEYMONITOR_H

#include <node_api.h>
#include <stdbool.h>
#include <stdint.h>

// Key event types
#define KEY_EVENT_DOWN 1
#define KEY_EVENT_UP 2
#define KEY_EVENT_FLAGS_CHANGED 3

// Key event structure passed to JavaScript callback
typedef struct {
  int type;           // KEY_EVENT_DOWN, KEY_EVENT_UP, or KEY_EVENT_FLAGS_CHANGED
  uint16_t keyCode;   // Virtual key code
  uint64_t flags;     // Modifier flags
  bool isRepeat;      // True if this is a key repeat
} KeyEvent;

// Start monitoring keyboard events
// callback: JavaScript function to call when events occur
// Returns: 0 on success, non-zero on error
int StartKeyMonitor(napi_env env, napi_value callback);

// Stop monitoring keyboard events
// Returns: 0 on success, non-zero on error
int StopKeyMonitor(void);

// Check if monitor is running
bool IsKeyMonitorRunning(void);

#endif // KEYMONITOR_H
