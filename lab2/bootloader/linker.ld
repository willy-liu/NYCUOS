/* linker.ld — Raspberry Pi 3, AArch64, bootloader.img at 0x80000 */
SECTIONS {
    . = 0x80000;
    _bootloader_start = .;

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

    _bootloader_end = .;
    _stack_top = 0x100000;
}
PROVIDE(_bootloader_runtime_start = 0x60000);
