#ifndef KEYSENDER_H
#define KEYSENDER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Send a key with optional modifier (Ctrl on Windows, Command on macOS)
uint32_t SendKey(const char *key, bool useModifier);

#ifdef __cplusplus
}
#endif

#endif // KEYSENDER_H