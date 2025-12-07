#ifndef SIMPLE_ALLOC_H
#define SIMPLE_ALLOC_H

#include <stddef.h>
#include <stdint.h>

/**
 * Initialize the simple allocator with a custom buffer.
 * Pass NULL for buffer to use the built-in static pool.
 */
void simple_alloc_init(void *buffer, size_t size);

/** Reset the bump-pointer to the beginning of the active pool. */
void simple_alloc_reset(void);

/** Allocate a block of memory with 8-byte alignment. */
void *simple_alloc(size_t size);

size_t simple_alloc_bytes_used(void);
size_t simple_alloc_bytes_free(void);

#endif /* SIMPLE_ALLOC_H */
