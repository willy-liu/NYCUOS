.text
.global _start

_start:

    // 避免 qemu 多核心 啟動問題，只讓 core 0 執行，在 樹莓派 3B+ 上測試時加不加這段都一樣
    mrs x0, MPIDR_EL1
    and x0, x0, #0xFF
    cbnz x0, .secondary

    // 設定 stack pointer
    ldr x0, =_stack_top
    mov sp, x0
    
    // 清空 .bss 段
    ldr x1, =_bss_start
    ldr x2, =_bss_end
    mov x3, #0

bss_clear:
    cmp x1, x2
    b.hs bss_done
    str x3, [x1], #8   // 寫 0 並往後移 8 bytes
    b bss_clear

bss_done:
    // 跳到 C 函式
    bl main

.secondary:
    // secondary core: 永遠睡死
1:  wfe
    b 1b