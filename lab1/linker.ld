/* link.ld — Raspberry Pi 3, AArch64, kernel8.img at 0x80000 */
SECTIONS {
    . = 0x80000;                /* set the start address where the linked image will be placed */

    .text : {
        *(.text*)               /* all code sections from input object files */
    }                           /* .text - executable code */

    .rodata : {
        *(.rodata*)             /* read-only data (constants, literal pools) */
    }                           /* .rodata - read-only data */

    .data : {
        *(.data*)               /* initialized writable data */
    }                           /* .data - initialized data */

    .bss : {
        _bss_start = .;        /* symbol: start of BSS (uninitialized data) */
        *(.bss*)               /* uninitialized data sections */
        *(COMMON)              /* common symbols (uninitialized globals) */
        _bss_end = .;          /* symbol: end of BSS */
    }                           /* .bss - zero-initialized data */

    _stack_top = 0x100000;      /* define a symbol for the top of the stack */
}
