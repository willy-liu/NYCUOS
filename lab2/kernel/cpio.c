#include "cpio.h"
#include "uart.h"
// cpio base address
static void *cpio_base = NULL;

static int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c1 = s1[i];
        unsigned char c2 = s2[i];

        if (c1 != c2)
            return (int)c1 - (int)c2;

        if (c1 == '\0')  // 提早結束
            return 0;
    }
    return 0;
}

// =============================================================
// =============== Core CPIO Parser API ========================
// =============================================================
void cpio_set_base(void *addr) {
    cpio_base = addr;
}

void* cpio_get_entry_start() {
    return cpio_base;
}

void* cpio_get_next(void *current) {
    struct cpio_newc_header *header = (struct cpio_newc_header *)current;
    size_t namesize = cpio_hex_to_uint(header->c_namesize, 8);
    size_t filesize = cpio_hex_to_uint(header->c_filesize, 8);

    size_t name_start = (uintptr_t)current + sizeof(struct cpio_newc_header);
    size_t file_start = cpio_align4(name_start + namesize);
    size_t next_entry  = cpio_align4(file_start + filesize);

    return (void*)next_entry;
}

void* cpio_get_file(const char *filename, size_t *out_size) {
    void *current = cpio_get_entry_start();

    while (1) {
        struct cpio_newc_header *h = (struct cpio_newc_header *)current;

        size_t namesize = cpio_hex_to_uint(h->c_namesize, 8);
        size_t filesize = cpio_hex_to_uint(h->c_filesize, 8);

        char *name = (char *)current + sizeof(struct cpio_newc_header);

        // Check trailer
        if (strncmp(name, "TRAILER!!!", namesize) == 0)
            return NULL;

        // Compare filename safely (namesize may include '\0')
        if (strncmp(name, filename, namesize) == 0) {

            // --- find file content address ---
            void *file_addr = (void *)cpio_align4(
                (uintptr_t)name + namesize
            );
            
            // --- output filesize ---
            if (out_size)
                *out_size = filesize;

            return file_addr;
        }

        // next entry
        current = cpio_get_next(current);
    }
}

void cpio_list_all() {
    void *current = cpio_get_entry_start();

    while (1) {
        struct cpio_newc_header *h = (struct cpio_newc_header *)current;

        size_t namesize = cpio_hex_to_uint(h->c_namesize, 8);

        char *name = (char *)current + sizeof(struct cpio_newc_header);

        // Check trailer
        if (strncmp(name, "TRAILER!!!", namesize) == 0)
            break;

        // Print filename
        uart_puts(name);
        uart_puts("\n");

        // next entry
        current = cpio_get_next(current);
    }
}

// =============================================================
// =============== Utilities for header parsing =================
// =============================================================

uint32_t cpio_hex_to_uint(const char *hex, int len) {
    uint32_t result = 0;
    for (int i = 0; i < len; i++) {
        result <<= 4; // 左移 4 位元（乘以 16）
        char c = hex[i];
        if (c >= '0' && c <= '9') {
            result += c - '0';
        } else if (c >= 'A' && c <= 'F') {
            result += c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            result += c - 'a' + 10;
        }
    }
    return result;
}

size_t cpio_align4(size_t x) {
    return (x + 3) & ~((size_t)3);
}

int cpio_is_trailer(const char *name) {
    return strncmp(name, "TRAILER!!!", 10) == 0;
}

