#include "modifiermonitor.h"
#include "keymonitor.h"
#include <stdlib.h>

#import <AppKit/AppKit.h>

static napi_threadsafe_function g_tsfn = NULL;
static id g_monitor = nil;

// Runs on the JS thread
static void CallJS(napi_env env, napi_value js_callback, void* context, void* data) {
  (void)context;
  KeyEvent* event = (KeyEvent*)data;
  if (event == NULL) return;
  if (env == NULL || js_callback == NULL) {
    free(event);
    return;
  }

  napi_value eventObj;
  if (napi_create_object(env, &eventObj) != napi_ok) {
    free(event);
    return;
  }

  napi_value typeVal;
  napi_create_string_utf8(env, "flagsChanged", NAPI_AUTO_LENGTH, &typeVal);
  napi_set_named_property(env, eventObj, "type", typeVal);

  napi_value keyCodeVal;
  napi_create_uint32(env, event->keyCode, &keyCodeVal);
  napi_set_named_property(env, eventObj, "keyCode", keyCodeVal);

  napi_value flagsVal;
  napi_create_int64(env, (int64_t)event->flags, &flagsVal);
  napi_set_named_property(env, eventObj, "flags", flagsVal);

  napi_value isRepeatVal;
  napi_get_boolean(env, false, &isRepeatVal);
  napi_set_named_property(env, eventObj, "isRepeat", isRepeatVal);

  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, js_callback, 1, &eventObj, NULL);

  free(event);
}

int StartModifierMonitor(napi_env env, napi_value callback) {
  if (g_monitor != nil) {
    return 1; // Already running
  }

  napi_value resourceName;
  napi_create_string_utf8(env, "ModifierMonitorCallback", NAPI_AUTO_LENGTH, &resourceName);
  if (napi_create_threadsafe_function(env, callback, NULL, resourceName, 0, 1, NULL, NULL, NULL, CallJS, &g_tsfn) != napi_ok) {
    return 2;
  }

  g_monitor = [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskFlagsChanged
                                                     handler:^(NSEvent* event) {
    KeyEvent* keyEvent = (KeyEvent*)malloc(sizeof(KeyEvent));
    if (keyEvent == NULL || g_tsfn == NULL) {
      free(keyEvent);
      return;
    }
    keyEvent->type = KEY_EVENT_FLAGS_CHANGED;
    keyEvent->keyCode = (uint16_t)[event keyCode];
    keyEvent->flags = (uint64_t)[event modifierFlags];
    keyEvent->isRepeat = false;
    napi_call_threadsafe_function(g_tsfn, keyEvent, napi_tsfn_nonblocking);
  }];

  if (g_monitor == nil) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
    g_tsfn = NULL;
    return 3;
  }

  [g_monitor retain];
  return 0;
}

int StopModifierMonitor(void) {
  if (g_monitor == nil) {
    return 1; // Not running
  }

  [NSEvent removeMonitor:g_monitor];
  [g_monitor release];
  g_monitor = nil;

  if (g_tsfn != NULL) {
    napi_release_threadsafe_function(g_tsfn, napi_tsfn_release);
    g_tsfn = NULL;
  }
  return 0;
}

bool IsModifierMonitorRunning(void) {
  return g_monitor != nil;
}
