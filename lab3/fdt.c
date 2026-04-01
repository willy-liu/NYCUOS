#include "fdt.h"
#include "mini_uart.h"

static void *dtb_base = NULL;

/* ----------------------------------------------------------------
 * Helper utilities
 * ---------------------------------------------------------------- */

static size_t fdt_strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

uint32_t fdt_be32(uint32_t v)
{
    return ((v >> 24) & 0xFFu)       |
           ((v >>  8) & 0xFF00u)     |
           ((v <<  8) & 0xFF0000u)   |
           ((v << 24) & 0xFF000000u);
}

uint32_t fdt_read_be32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) |
           (uint32_t)b[3];
}

static uint32_t align4(uint32_t v)
{
    return (v + 3u) & ~3u;
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

void fdt_init(void *dtb_addr)
{
    dtb_base = dtb_addr;
}

/**
 * Walk the entire FDT structure block.
 * For every FDT_PROP token encountered, invoke cb(node_name, prop_name, data, len).
 *
 * A simple node-name stack is maintained so we always know which node
 * a property belongs to, even for nested nodes.
 */
void fdt_traverse(fdt_callback cb)
{
    if (!dtb_base || !cb)
        return;

    struct fdt_header *hdr = (struct fdt_header *)dtb_base;

    /* Verify magic number */
    if (fdt_be32(hdr->magic) != FDT_MAGIC) {
        uart_puts("[fdt] bad magic number:");
        print_hex(fdt_be32(hdr->magic));
        uart_puts("\n");
        return;
    }

    uint32_t struct_off  = fdt_be32(hdr->off_dt_struct);
    uint32_t strings_off = fdt_be32(hdr->off_dt_strings);

    uint8_t    *p         = (uint8_t *)dtb_base + struct_off;
    const char *str_block = (const char *)dtb_base + strings_off;

    /* Node-name stack so we track the "current" node at any depth. */
#define FDT_MAX_DEPTH 64
    const char *node_stack[FDT_MAX_DEPTH];
    int depth = 0;

    for (;;) {
        uint32_t token = fdt_read_be32(p);
        p += 4;

        switch (token) {
        case FDT_BEGIN_NODE: {
            const char *name = (const char *)p;
            uint32_t name_len = (uint32_t)fdt_strlen(name) + 1; /* +1 for '\0' */
            p += align4(name_len);

            if (depth < FDT_MAX_DEPTH)
                node_stack[depth] = name;
            depth++;
            break;
        }

        case FDT_END_NODE:
            if (depth > 0) depth--;
            break;

        case FDT_PROP: {
            uint32_t len     = fdt_read_be32(p); p += 4;
            uint32_t nameoff = fdt_read_be32(p); p += 4;

            const char *prop_name = str_block + nameoff;
            const void *data      = p;
            p += align4(len);

            /* Determine which node this property belongs to */
            const char *cur_node = (depth > 0) ? node_stack[depth - 1] : "";
            cb(cur_node, prop_name, data, len);
            break;
        }

        case FDT_NOP:
            break;

        case FDT_END:
            return;

        default:
            uart_puts("[fdt] unknown token: ");
            print_hex(token);
            uart_puts("\n");
            return;
        }
    }
}
