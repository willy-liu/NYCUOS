#ifndef FDT_H
#define FDT_H

#include <stdint.h>
#include <stddef.h>

/* ================================================================
 * Flattened Device Tree (FDT) header — all fields are big-endian
 * Spec: https://www.devicetree.org/specifications/  (Chapter 5)
 * ================================================================ */
struct fdt_header {
    uint32_t magic;              /* 0xD00DFEED */
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

/* Structure-block tokens */
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

#define FDT_MAGIC       0xD00DFEED

/**
 * Callback type used by fdt_traverse().
 *
 * @param node_name  Name of the current device-tree node (e.g. "chosen").
 * @param prop_name  Name of the property (e.g. "linux,initrd-start").
 * @param data       Pointer to the raw property value (big-endian).
 * @param len        Length of the property value in bytes.
 */
typedef void (*fdt_callback)(const char *node_name, const char *prop_name,
                              const void *data, uint32_t len);

/** Store the DTB base address for later use. */
void fdt_init(void *dtb_addr);

/** Walk the entire device tree and invoke @p cb for every property. */
void fdt_traverse(fdt_callback cb);

/** Convert a big-endian 32-bit value to host (little-endian) byte order. */
uint32_t fdt_be32(uint32_t v);

/** Read a big-endian 32-bit word from a (possibly unaligned) address. */
uint32_t fdt_read_be32(const void *p);

#endif /* FDT_H */
