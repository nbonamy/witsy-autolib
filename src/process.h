#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Structure to hold icon data
typedef struct {
  void* data;      // Raw RGBA bitmap data
  int size;        // Size of icon data in bytes
  int width;       // Icon width
  int height;      // Icon height
} IconData;


// Get product name from executable resources
BOOL GetProductNameFromExe(const char* exePath, char* productName, DWORD bufferSize);

// Get icon from executable path
// Returns a dynamically allocated IconData structure that must be freed with FreeIconData
IconData* GetIconFromExePath(const char* exePath);

// Free the memory allocated for IconData
void FreeIconData(IconData* iconData);

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H