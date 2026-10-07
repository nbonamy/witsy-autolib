#ifndef MODIFIERMONITOR_H
#define MODIFIERMONITOR_H

#include <node_api.h>
#include <stdbool.h>

// Monitor modifier key changes (flagsChanged) only.
//
// Unlike StartKeyMonitor (CGEventTap), this uses an NSEvent global monitor,
// which macOS delivers for modifier changes without the Input Monitoring or
// Accessibility permission. Events have the same shape as key monitor events
// with type "flagsChanged". macOS only; other platforms return a non-zero code.
//
// The monitor must be started from the thread that runs the AppKit main run
// loop (the Electron main process).
//
// Returns: 0 on success, non-zero on error
int StartModifierMonitor(napi_env env, napi_value callback);
int StopModifierMonitor(void);
bool IsModifierMonitorRunning(void);

#endif // MODIFIERMONITOR_H
