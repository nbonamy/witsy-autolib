#include "window.h"
#include "process.h"

#ifdef WIN32

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PATH_LENGTH 260
#define ICON_BUFFER_SIZE 65536
#define NAME_BUFFER_SIZE 256

typedef struct {
  char exePath[MAX_PATH_LENGTH];
  char title[256];
  char productName[NAME_BUFFER_SIZE];
  char iconData[ICON_BUFFER_SIZE];
  int iconSize;
  int processId;
} WindowInfo;

static BOOL GetWindoWinfo(HWND hwnd, WindowInfo* info) {
  
  // Skip windows with no title
  char title[256];
  if (GetWindowTextA(hwnd, title, sizeof(title)) == 0) {
    printf("GetWindowTextA failed\n");
    return FALSE;
  }
  
  // Get the process ID
  DWORD processId;
  GetWindowThreadProcessId(hwnd, &processId);
  info->processId = processId;
  
  // Get the process executable path
  HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
  if (process) {
    if (GetModuleFileNameExA(process, NULL, info->exePath, MAX_PATH_LENGTH) > 0) {
      // Copy the window title
      strncpy(info->title, title, sizeof(info->title) - 1);
      info->title[sizeof(info->title) - 1] = '\0';
      
      // Get product name from executable resources
      if (!GetProductNameFromExe(info->exePath, info->productName, NAME_BUFFER_SIZE)) {
        // If getting product name fails, use filename as fallback
        const char* filename = strrchr(info->exePath, '\\');
        if (filename) {
          filename++; // Skip the backslash
          strncpy(info->productName, filename, NAME_BUFFER_SIZE - 1);
          info->productName[NAME_BUFFER_SIZE - 1] = '\0';
          
          // Remove extension if present
          char* dot = strrchr(info->productName, '.');
          if (dot) *dot = '\0';
        } else {
          strcpy(info->productName, "Unknown");
        }
      }
      
    }
    CloseHandle(process);
  }

  // done
  return TRUE;

}

ForemostWindowInfo* GetForemostWindow() {

  HWND hwnd = GetForegroundWindow();
  if (!hwnd) {
    printf("GetForegroundWindow failed\n");
    return NULL;
  }

  WindowInfo info;
  ZeroMemory(&info, sizeof(WindowInfo));
  if (!GetWindoWinfo(hwnd, &info)) {
    printf("GetForegroundWindow failed\n");
    return NULL;
  }

  ForemostWindowInfo* result = (ForemostWindowInfo*)malloc(sizeof(ForemostWindowInfo));
  if (!result) return NULL;
  
  result->exePath = _strdup(info.exePath);
  result->title = _strdup(info.title);
  result->productName = _strdup(info.productName);  // Added product name
  result->processId = info.processId;
  
  return result;
}

void FreeWindowInfo(ForemostWindowInfo* info) {
  if (!info) return;
  
  if (info->exePath) free(info->exePath);
  if (info->title) free(info->title);
  if (info->productName) free(info->productName);  // Free product name
  
  free(info);
}

BOOL ActivateWindow(HWND hwnd) {
  if (!hwnd) {
    printf("Invalid window handle\n");
    return FALSE;
  }
  
  // Send messages
  SendMessage(hwnd, WM_ACTIVATEAPP, TRUE, 0);  
  SendMessage(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
  SendMessage(hwnd, WM_SETFOCUS, 0, 0);
  SetForegroundWindow(hwnd);

  // Done
  return TRUE;
}

#endif
