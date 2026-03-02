.text
.global _start

_start:
    // x0 = DTB base address (由 QEMU 或 bootloader 傳入)
    mov x19, x0            // 保存 DTB 位址到 callee-saved register

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
    mov x0, x19            // 將 DTB 位址作為 main() 的第一個參數
    bl main