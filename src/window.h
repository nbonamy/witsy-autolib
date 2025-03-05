#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Structure to hold window information
typedef struct {
  char* exePath;      // Path to the executable
  char* title;        // Window title
  char* productName;  // Product name from exe resources
  int processId;      // Process ID
} ForemostWindowInfo;

// Get information about the foremost window
// Returns a pointer to a WindowInfo struct that must be freed with FreeWindowInfo
ForemostWindowInfo* GetForemostWindow();

// Free the memory allocated for WindowInfo
void FreeWindowInfo(ForemostWindowInfo* info);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_H