.section ".text"
.global _start

_start:
    // 1️⃣ 設定 stack pointer (例如放在 0x80000 之上)
    ldr x0, =_stack_top
    mov sp, x0

    // 2️⃣ 清空 .bss 段
    ldr x1, =_bss_start
    ldr x2, =_bss_end
    mov x3, #0

bss_clear:
    cmp x1, x2
    b.hs bss_done 
    str x3, [x1], #8   // 寫 0 並往後移 8 bytes
    b bss_clear

bss_done:
    // 3️⃣ 跳到 C 函式
    bl main

1:  b 1b               // main 回來就進入無限迴圈
