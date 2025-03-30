#include "selection.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include "process.h"
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

uint32_t GetSelectedText(char *buffer, size_t buffer_size)
{
  if (buffer == NULL || buffer_size == 0)
  {
    return 0;
  }

#ifdef _WIN32
  // Dummy implementation for Windows
  const char *dummyText = "This is a dummy selected text on Windows";
  size_t textLength = strlen(dummyText);

  if (textLength >= buffer_size)
  {
    strncpy(buffer, dummyText, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
  }
  else
  {
    strcpy(buffer, dummyText);
  }

  return 1;

#elif defined(__APPLE__)

  // // // Get the system-wide AXUIElementRef for the foreground application
  // AXUIElementRef systemWideElement = AXUIElementCreateSystemWide();

  // // force accessibility features to be enabled
  // AXUIElementSetAttributeValue(systemWideElement, CFSTR("AXManualAccessibility"), kCFBooleanTrue);
  // AXUIElementSetAttributeValue(systemWideElement, CFSTR("AXEnhancedUserInterface"), kCFBooleanTrue);

  // AXUIElementRef focusedApp = NULL;
  // AXError result = AXUIElementCopyAttributeValue(
  //     systemWideElement,
  //     kAXFocusedApplicationAttribute,
  //     (CFTypeRef *)&focusedApp);

  // if (result != kAXErrorSuccess || focusedApp == NULL)
  // {
  //   printf("Error: Unable to get focused application %d\n", result);
  //   CFRelease(systemWideElement);
  //   buffer[0] = '\0';
  //   return 0;
  // }

  // Get the frontmost application
  pid_t pid = GetForemostApplicationPID();
  if (pid == 0)
  {
    buffer[0] = '\0';
    return 0;
  }

  // now create an AXUIElementRef for the focused application
  AXUIElementRef focusedApp = AXUIElementCreateApplication(pid);
  if (focusedApp == NULL)
  {
    printf("Error: Unable to get focused application\n");
    // CFRelease(systemWideElement);
    buffer[0] = '\0';
    return 0;
  }

  // force accessibility features to be enabled
  AXUIElementSetAttributeValue(focusedApp, CFSTR("AXManualAccessibility"), kCFBooleanTrue);
  AXUIElementSetAttributeValue(focusedApp, CFSTR("AXEnhancedUserInterface"), kCFBooleanTrue);

  // Get the focused element in the application
  AXUIElementRef focusedElement = NULL;
  AXError result = AXUIElementCopyAttributeValue(
      focusedApp,
      kAXFocusedUIElementAttribute,
      (CFTypeRef *)&focusedElement);

  if (result != kAXErrorSuccess || focusedElement == NULL)
  {
    printf("Error: Unable to get focused UI element: %d\n", result);
    CFRelease(focusedApp);
    // CFRelease(systemWideElement);
    buffer[0] = '\0';
    return 0;
  }

  // Get the selected text from the focused element
  CFStringRef selectedText = NULL;
  result = AXUIElementCopyAttributeValue(
      focusedElement,
      kAXSelectedTextAttribute,
      (CFTypeRef *)&selectedText);

  // if (result != kAXErrorSuccess || selectedText == NULL || CFStringGetLength(selectedText) == 0)
  // {
  //   printf("Warning: Getting value\n");

  //   // If kAXSelectedTextAttribute failed, try kAXValueAttribute as fallback
  //   result = AXUIElementCopyAttributeValue(
  //       focusedElement,
  //       kAXValueAttribute,
  //       (CFTypeRef *)&selectedText);
  // }

  if (result != kAXErrorSuccess || selectedText == NULL || CFStringGetLength(selectedText) == 0)
  {
    printf("Warning: Trying Webkit compatibility\n");

    CFTypeRef range;
    result = AXUIElementCopyAttributeValue(focusedElement, CFSTR("AXSelectedTextMarkerRange"), &range);
    if (result == kAXErrorSuccess)
    {
      result = AXUIElementCopyParameterizedAttributeValue(focusedElement, CFSTR("AXStringForTextMarkerRange"), range,
                                                          (CFTypeRef *)&selectedText);
      CFRelease(range);
    }
  }

  if (result != kAXErrorSuccess || selectedText == NULL)
  {
    printf("Error: Unable to get selected text or value\n");
    CFRelease(focusedElement);
    CFRelease(focusedApp);
    // CFRelease(systemWideElement);
    buffer[0] = '\0';
    return 0;
  }

  // Convert the CFStringRef to a C string
  if (selectedText != NULL)
  {

    // Get the length of the selected text
    CFIndex textLength = CFStringGetLength(selectedText);

    if (textLength == 0)
    {
      // Selected text is empty
      buffer[0] = '\0';
      printf("Info: Selected text is empty\n");

      // Clean up
      CFRelease(selectedText);
      CFRelease(focusedElement);
      CFRelease(focusedApp);
      // CFRelease(systemWideElement);

      return 1; // Success but empty
    }

    // Calculate buffer requirements
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(textLength, kCFStringEncodingUTF8) + 1;

    if (maxSize > (long)buffer_size)
    {
      printf("Warning: Selected text length (%ld bytes) exceeds buffer size (%zu bytes), text may be truncated\n",
             (long)maxSize, buffer_size);
    }

    // Convert the string
    Boolean success = CFStringGetCString(
        selectedText,
        buffer,
        buffer_size,
        kCFStringEncodingUTF8);

    if (!success)
    {
      printf("Error: Unable to convert selected text to C string\n");
      buffer[0] = '\0';
    }

    // Clean up
    CFRelease(selectedText);
    CFRelease(focusedElement);
    CFRelease(focusedApp);
    // CFRelease(systemWideElement);

    return success ? 1 : 0;
  }
  else
  {
    // This shouldn't happen due to earlier checks, but just in case
    printf("Error: Selected text reference is NULL\n");
    buffer[0] = '\0';

    // Clean up
    CFRelease(focusedElement);
    CFRelease(focusedApp);
    // CFRelease(systemWideElement);

    return 0;
  }

#else
  // Fallback for unsupported platforms
  strcpy(buffer, "Platform not supported");
  return 0;
#endif
}
