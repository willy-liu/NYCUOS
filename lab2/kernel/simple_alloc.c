#include "simple_alloc.h"

#ifndef SIMPLE_ALLOC_DEFAULT_POOL_SIZE
#define SIMPLE_ALLOC_DEFAULT_POOL_SIZE (64 * 1024)
#endif

static uint8_t default_pool[SIMPLE_ALLOC_DEFAULT_POOL_SIZE];
static uint8_t *pool_start = default_pool;
static size_t pool_size = SIMPLE_ALLOC_DEFAULT_POOL_SIZE;
static size_t pool_offset = 0;

static size_t align8(size_t value) {
    return (value + 7u) & ~((size_t)7u);
}

void simple_alloc_init(void *buffer, size_t size) {
    if (buffer && size > 0) {
        pool_start = (uint8_t *)buffer;
        pool_size = size;
    } else {
        pool_start = default_pool;
        pool_size = SIMPLE_ALLOC_DEFAULT_POOL_SIZE;
    }
    pool_offset = 0;
}

void simple_alloc_reset(void) {
    pool_offset = 0;
}

void *simple_alloc(size_t size) {
    size = align8(size);
    if (!size || !pool_start || pool_size <= pool_offset) {
        return NULL;
    }

    if (pool_size - pool_offset < size) {
        return NULL;
    }

    void *ptr = pool_start + pool_offset;
    pool_offset += size;
    return ptr;
}

size_t simple_alloc_bytes_used(void) {
    return pool_offset <= pool_size ? pool_offset : pool_size;
}

size_t simple_alloc_bytes_free(void) {
    if (pool_offset >= pool_size) {
        return 0;
    }
    return pool_size - pool_offset;
}
