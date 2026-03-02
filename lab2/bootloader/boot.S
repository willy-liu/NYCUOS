.text
.global _start

_start:

    // x0 = DTB base address（由 QEMU / RPi firmware 傳入）
    mov x19, x0            // 保存 DTB 位址到 callee-saved register

    // 避免 qemu 多核心 啟動問題，只讓 core 0 執行，在 樹莓派 3B+ 上測試時加不加這段都一樣
    mrs x0, MPIDR_EL1
    and x0, x0, #0xFF
    cbnz x0, .secondary

    bl relocate_to_runtime
    b .

relocate_to_runtime:
    ldr x0, =_bootloader_runtime_start
    ldr x1, =_bootloader_start
    ldr x2, =_bootloader_end
    mov x3, x0

copy_loop:
    cmp x1, x2
    b.hs copy_done
    ldrb w4, [x1], #1
    strb w4, [x3], #1
    b copy_loop

copy_done:
    ldr x4, =relocated_entry
    ldr x5, =_bootloader_start
    sub x4, x4, x5
    add x4, x4, x0
    br x4

relocated_entry:

    // 設定 stack pointer
    ldr x0, =_stack_top
    mov sp, x0
    
    // 清空 .bss 段（使用 runtime 位址）
    ldr x1, =_bootloader_runtime_start
    ldr x2, =_bootloader_start
    sub x3, x1, x2
    ldr x4, =_bss_start
    add x4, x4, x3
    ldr x5, =_bss_end
    add x5, x5, x3
    mov x6, #0

bss_clear:
    cmp x4, x5
    b.hs bss_done
    str x6, [x4], #8   // 寫 0 並往後移 8 bytes
    b bss_clear

bss_done:
    mov x0, x19            // 將 DTB 位址作為 main() 的第一個參數
    bl main

.secondary:
    // secondary core: 永遠睡死
1:  wfe
    b 1b