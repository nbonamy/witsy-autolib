#include "process.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32

#define ICON_BUFFER_SIZE 65536

// Get product name from executable file
BOOL GetProductNameFromExe(const char* exePath, char* productName, DWORD bufferSize) {
  DWORD versionInfoSize = GetFileVersionInfoSizeA(exePath, NULL);
  if (versionInfoSize == 0) {
    return FALSE;
  }

  void* versionInfo = malloc(versionInfoSize);
  if (!versionInfo) {
    return FALSE;
  }

  if (!GetFileVersionInfoA(exePath, 0, versionInfoSize, versionInfo)) {
    free(versionInfo);
    return FALSE;
  }

  // Try multiple language codepages to find product name
  struct LANGANDCODEPAGE {
    WORD language;
    WORD codePage;
  } *lpTranslate;
  
  UINT cbTranslate = 0;
  BOOL success = FALSE;

  // First get the language info
  if (VerQueryValueA(versionInfo, "\\VarFileInfo\\Translation",
                    (LPVOID*)&lpTranslate, &cbTranslate)) {
                    
    // For each language
    for (UINT i = 0; i < (cbTranslate / sizeof(struct LANGANDCODEPAGE)); i++) {
      char subBlock[128];
      
      // First try ProductName
      sprintf(subBlock, 
              "\\StringFileInfo\\%04x%04x\\ProductName",
              lpTranslate[i].language,
              lpTranslate[i].codePage);
      
      LPVOID valuePtr;
      UINT valueLen;
      
      if (VerQueryValueA(versionInfo, subBlock, &valuePtr, &valueLen) && valueLen > 0) {
        strncpy(productName, (char*)valuePtr, bufferSize - 1);
        productName[bufferSize - 1] = '\0';
        success = TRUE;
        break;
      }
      
      // If ProductName not found, try FileDescription
      sprintf(subBlock, 
              "\\StringFileInfo\\%04x%04x\\FileDescription",
              lpTranslate[i].language,
              lpTranslate[i].codePage);
      
      if (VerQueryValueA(versionInfo, subBlock, &valuePtr, &valueLen) && valueLen > 0) {
        strncpy(productName, (char*)valuePtr, bufferSize - 1);
        productName[bufferSize - 1] = '\0';
        success = TRUE;
        break;
      }
    }
  }
  
  free(versionInfo);
  
  // If no product name found, use filename without extension
  if (!success) {
    // Extract filename without path
    const char* filename = exePath + strlen(exePath);
    while (filename > exePath && *(filename - 1) != '\\' && *(filename - 1) != '/')
      filename--;
    
    strncpy(productName, filename, bufferSize - 1);
    productName[bufferSize - 1] = '\0';
    
    // Remove extension if present
    char* dot = strrchr(productName, '.');
    if (dot) *dot = '\0';
    
    success = TRUE;
  }
  
  return success;
}

IconData* GetIconFromExePath(const char* exePath) {
  if (!exePath) return NULL;
  
  IconData* result = (IconData*)malloc(sizeof(IconData));
  if (!result) return NULL;
  
  // Initialize
  result->data = NULL;
  result->size = 0;
  result->width = 0;
  result->height = 0;
  
  // Extract icon from executable
  HICON hIcon = NULL;
  if (ExtractIconExA(exePath, 0, &hIcon, NULL, 1) > 0 && hIcon) {
    ICONINFO iconInfo;
    if (GetIconInfo(hIcon, &iconInfo)) {
      BITMAP bm;
      if (GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bm)) {
        // Create compatible DC
        HDC hdc = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
        SelectObject(hdcMem, hBitmap);
        
        // Draw icon onto bitmap
        DrawIcon(hdcMem, 0, 0, hIcon);
        
        // Create BITMAPINFO structure for getting DIB bits
        BITMAPINFOHEADER bi;
        ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bm.bmWidth;
        bi.biHeight = -bm.bmHeight; // Negative for top-down
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        
        // Calculate pixel data size
        int pixelDataSize = bm.bmWidth * bm.bmHeight * 4; // 4 bytes per pixel (RGBA)
        
        // We need both color and mask data for ICO
        unsigned char* pixelData = (unsigned char*)malloc(pixelDataSize);
        if (!pixelData) {
          DeleteObject(hBitmap);
          DeleteDC(hdcMem);
          ReleaseDC(NULL, hdc);
          if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
          if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
          DestroyIcon(hIcon);
          free(result);
          return NULL;
        }
        
        // Get bitmap bits
        GetDIBits(hdcMem, hBitmap, 0, bm.bmHeight, pixelData, 
                  (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        
        // Generate mask data (1 bit per pixel)
        int maskRowSize = ((bm.bmWidth + 31) / 32) * 4; // 32-bit aligned rows
        int maskDataSize = maskRowSize * bm.bmHeight;
        unsigned char* maskData = (unsigned char*)calloc(maskDataSize, 1);
        if (!maskData) {
          free(pixelData);
          DeleteObject(hBitmap);
          DeleteDC(hdcMem);
          ReleaseDC(NULL, hdc);
          if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
          if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
          DestroyIcon(hIcon);
          free(result);
          return NULL;
        }
        
        // Build mask from alpha channel
        for (int y = 0; y < bm.bmHeight; y++) {
          for (int x = 0; x < bm.bmWidth; x++) {
            int pixelPos = (y * bm.bmWidth + x) * 4;
            int alpha = pixelData[pixelPos + 3];
            
            if (alpha < 128) { // If pixel is mostly transparent
              int bytePos = y * maskRowSize + (x / 8);
              int bitPos = 7 - (x % 8); // MS bit first
              maskData[bytePos] |= (1 << bitPos);
            }
          }
        }
        
        // Now create ICO file structure
        // ICO header (6 bytes) + 1 directory entry (16 bytes) + BMP info header + pixel data + mask data
        int iconDirSize = 6;
        int iconDirEntrySize = 16;
        int dibHeaderSize = sizeof(BITMAPINFOHEADER);
        int totalSize = iconDirSize + iconDirEntrySize + dibHeaderSize + pixelDataSize + maskDataSize;
        
        // Allocate memory for the ICO file
        if (totalSize <= ICON_BUFFER_SIZE) {
          result->data = malloc(totalSize);
          if (result->data) {
            unsigned char* ptr = (unsigned char*)result->data;
            
            // ICONDIR structure
            // idReserved (must be 0)
            *(WORD*)ptr = 0;
            ptr += 2;
            
            // idType (1 for ICO)
            *(WORD*)ptr = 1;
            ptr += 2;
            
            // idCount (1 image)
            *(WORD*)ptr = 1;
            ptr += 2;
            
            // ICONDIRENTRY structure
            // bWidth
            *ptr++ = (unsigned char)bm.bmWidth;
            
            // bHeight
            *ptr++ = (unsigned char)bm.bmHeight;
            
            // bColorCount (0 means 256 or more colors)
            *ptr++ = 0;
            
            // bReserved (must be 0)
            *ptr++ = 0;
            
            // wPlanes (should be 1)
            *(WORD*)ptr = 1;
            ptr += 2;
            
            // wBitCount (32 for RGBA)
            *(WORD*)ptr = 32;
            ptr += 2;
            
            // dwBytesInRes (size of image data)
            *(DWORD*)ptr = dibHeaderSize + pixelDataSize + maskDataSize;
            ptr += 4;
            
            // dwImageOffset (offset to the image data from the beginning of the file)
            *(DWORD*)ptr = iconDirSize + iconDirEntrySize;
            ptr += 4;
            
            // BITMAPINFOHEADER (for the icon image)
            BITMAPINFOHEADER iconBi = bi;
            iconBi.biHeight = bm.bmHeight * 2; // In ICO format, height includes both image and mask
            memcpy(ptr, &iconBi, dibHeaderSize);
            ptr += dibHeaderSize;
            
            // Copy pixel data (RGBA)
            memcpy(ptr, pixelData, pixelDataSize);
            ptr += pixelDataSize;
            
            // Copy mask data (1bpp)
            memcpy(ptr, maskData, maskDataSize);
            
            // Set result properties
            result->size = totalSize;
            result->width = bm.bmWidth;
            result->height = bm.bmHeight;
          }
        }
        
        // Clean up
        free(pixelData);
        free(maskData);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdc);
      }
      
      // Cleanup icon resources
      if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
      if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
    }
    
    DestroyIcon(hIcon);
  }
  
  // If we couldn't get an icon, free the struct and return NULL
  if (!result->data) {
    free(result);
    return NULL;
  }
  
  return result;
}

void FreeIconData(IconData* iconData) {
  if (!iconData) return;
  
  if (iconData->data) free(iconData->data);
  free(iconData);
}

#elif defined(__APPLE__)

pid_t GetForemostApplicationPID(void)
{
  ProcessSerialNumber psn = {0, kCurrentProcess};
  if (GetFrontProcess(&psn) != noErr) {
    printf("Error: Unable to get front process\n");
    return 0;
  }

  pid_t pid = 0;
  if (GetProcessPID(&psn, &pid) != noErr) {
    printf("Error: Unable to get process PID\n");
    return 0;
  }

  // done
  return pid;

}

#endif
