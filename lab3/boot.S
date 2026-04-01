.text
.global _start

_start:
    // x0 = DTB base address (由 QEMU 或 bootloader 傳入)
    mov x19, x0            // 保存 DTB 位址到 callee-saved register

    // 設定目前 EL 的 stack pointer (開機通常在 EL2)
    ldr x0, =_stack_top
    mov sp, x0
    // 也先準備好 EL1 的 stack pointer，避免切換後使用未初始化的 SP_EL1。
    msr sp_el1, x0
    
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
    // Raspberry Pi 3 boots in EL2 by default; drop to EL1 for the kernel.
    mrs x0, CurrentEL   // 讀取當前 Exception Level
    lsr x0, x0, #2      // 右移 2 位以得到 EL 級別 (0, 1, 2, or 3)
    cmp x0, #2          // 如果已經在 EL1，則直接跳轉到 main
    b.ne 1f
    bl from_el2_to_el1  // 否則從 EL2 切換到 EL1
1:
    mov x0, x19         // 將 DTB 位址作為 main() 的第一個參數
    bl main

from_el2_to_el1:
    mov x0, (1 << 31)   // HCR_EL2.RW(bit31) = 1 (1<<31 = 0x80000000) -> EL1 runs in AArch64
    msr hcr_el2, x0
    mov x0, #0x3c5      // EL1h, interrupts masked while switching level
    msr spsr_el2, x0
    msr elr_el2, lr     // 設定 ELR_EL2 為返回地址，lr是link register，保存了從EL2調用這個函數的返回地址
    eret                // 執行 Exception Return 後會自動切換到 EL1 並跳轉到 main