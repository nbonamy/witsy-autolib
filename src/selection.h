#ifndef SELECTION_H
#define SELECTION_H

#include <stdint.h>
#include <stddef.h>

/**
 * Get the currently selected text from the active application
 * 
 * @param buffer Buffer to store the selected text
 * @param buffer_size Size of the provided buffer
 * @return 1 on success, 0 on failure
 */
uint32_t GetSelectedText(char* buffer, size_t buffer_size);

#endif // SELECTION_H