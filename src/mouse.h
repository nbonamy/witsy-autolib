#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simulates a left mouse button click at the specified coordinates.
 * 
 * @param x The x-coordinate for the mouse click
 * @param y The y-coordinate for the mouse click
 * @return 1 if successful, 0 if failed
 */
uint32_t MouseClick(int x, int y);

#ifdef __cplusplus
}
#endif

#endif // MOUSE_H
