/* linker.ld — Raspberry Pi 3, AArch64, bootloader.img at 0x60000 */
SECTIONS {
    . = 0x60000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }

    _stack_top = 0x100000;
}
