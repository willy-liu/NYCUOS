# Lab 1: Hello World

## Introduction

In Lab 1, you will practice bare metal programming by implementing a
simple shell. You need to set up mini UART, and let your host and rpi3
can communicate through it.

## Goals of this lab

- Practice bare metal programming.
- Understand how to access rpi3’s peripherals.
- Set up mini UART.
- Set up mailbox.

## Basic Exercises

### Basic Exercise 1 - Basic Initialization - 20%

When a program is loaded, it requires,

- All it's data is presented at correct memory address.
- The program counter is set to correct memory address.
- The bss segment are initialized to 0.
- The stack pointer is set to a proper address.

After rpi3 booting, its booloader loads kernel8.img to physical address
0x80000, and start executing the loaded program. If the linker script is
correct, the above two requirements are met.

However, both bss segment and stack pointer are not properly
initialized. Hence, you need to initialize them at very beginning.
Otherwise, it may lead to undefined behaviors.

> [!TIP] Todo
>
> Initialize rpi3 after booted by bootloader.

<details open>
    <summary>Implementation</summary>

`boot.S`
```asm
.section ".text"
.global _start

_start:
    // 設定 stack pointer
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
    // 跳到 C 函式
    bl main

1:  b 1b               // main 回來就進入無限迴圈

```

`linker.ld`
```ld
/* linker.ld — Raspberry Pi 3, AArch64, kernel8.img at 0x80000 */
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

```
    
</details>

### Basic Exercise 2 - Mini UART - 20%

You'll use UART as a bridge between rpi3 and host computer for all the
labs. Rpi3 has 2 different UARTs, mini UART and PL011 UART. In this lab,
you need to set up the mini UART.

> [!TIP] Todo
>
> Follow `uart` to set up mini UART.


<details open>
    <summary>Implementation</summary>

`gpio.h`
```c
#pragma once

#define MMIO_BASE   0x3F000000

// GPIO registers
#define GPFSEL1     ((volatile unsigned int*)(MMIO_BASE + 0x200004))
#define GPPUD       ((volatile unsigned int*)(MMIO_BASE + 0x200094))
#define GPPUDCLK0   ((volatile unsigned int*)(MMIO_BASE + 0x200098))

// GPIO function codes
#define GPIO_FUNC_INPUT   0
#define GPIO_FUNC_OUTPUT  1
#define GPIO_FUNC_ALT0    4
#define GPIO_FUNC_ALT5    2

// Function prototypes
void gpio_set_func(int pin, int func);
void gpio_disable_pud(int pin);
void gpio_init_mini_uart(void);
```

`gpio.c`
```c
#include "gpio.h"

void gpio_set_func(int pin, int func) {
    volatile unsigned int *reg = GPFSEL1 + (pin / 10) - 1;
    int shift = (pin % 10) * 3;
    unsigned int val = *reg;
    val &= ~(7 << shift);
    val |=  (func << shift);
    *reg = val;
}

void gpio_disable_pud(int pin) {
    *GPPUD = 0;
    for (int i = 0; i < 150; i++) asm volatile("nop");
    *GPPUDCLK0 = (1 << pin);
    for (int i = 0; i < 150; i++) asm volatile("nop");
    *GPPUDCLK0 = 0;
}

void gpio_init_mini_uart(void) {
    gpio_set_func(14, GPIO_FUNC_ALT5); // TXD1
    gpio_set_func(15, GPIO_FUNC_ALT5); // RXD1
    gpio_disable_pud(14);
    gpio_disable_pud(15);
}
```
    
`mini_uart.c`
```c
// uart.c

#include "gpio.h"

#define MMIO_BASE        0x3F000000
#define AUX_BASE         (MMIO_BASE + 0x215000)

// =============== Register definitions ===============
#define AUX_IRQ          ((volatile unsigned int*)(AUX_BASE + 0x00))
#define AUX_ENABLES      ((volatile unsigned int*)(AUX_BASE + 0x04))

#define AUX_MU_IO_REG    ((volatile unsigned int*)(AUX_BASE + 0x40))
#define AUX_MU_IER_REG   ((volatile unsigned int*)(AUX_BASE + 0x44))
#define AUX_MU_IIR_REG   ((volatile unsigned int*)(AUX_BASE + 0x48))
#define AUX_MU_LCR_REG   ((volatile unsigned int*)(AUX_BASE + 0x4C))
#define AUX_MU_MCR_REG   ((volatile unsigned int*)(AUX_BASE + 0x50))
#define AUX_MU_LSR_REG   ((volatile unsigned int*)(AUX_BASE + 0x54))
#define AUX_MU_MSR_REG   ((volatile unsigned int*)(AUX_BASE + 0x58))
#define AUX_MU_SCRATCH   ((volatile unsigned int*)(AUX_BASE + 0x5C))
#define AUX_MU_CNTL_REG  ((volatile unsigned int*)(AUX_BASE + 0x60))
#define AUX_MU_STAT_REG  ((volatile unsigned int*)(AUX_BASE + 0x64))
#define AUX_MU_BAUD_REG  ((volatile unsigned int*)(AUX_BASE + 0x68))

// =====================================================

void mini_uart_init(void) {
    gpio_init_mini_uart();

    *AUX_ENABLES |= 1;          // Enable mini UART
    *AUX_MU_CNTL_REG = 0;       // Disable TX/RX
    *AUX_MU_LCR_REG = 3;        // 8-bit mode
    *AUX_MU_MCR_REG = 0;        // No auto flow control
    *AUX_MU_IER_REG = 0;        // Disable interrupts
    *AUX_MU_IIR_REG = 0xC6;     // Clear FIFO
    *AUX_MU_BAUD_REG = 270;     // Baud = 115200 bps (250MHz clock)
    *AUX_MU_CNTL_REG = 3;       // Enable TX/RX
}

void mini_uart_send(char c) {
    while (!(*AUX_MU_LSR_REG & 0x20)); // Wait until TX ready
    *AUX_MU_IO_REG = c;
}

char mini_uart_recv(void) {
    while (!(*AUX_MU_LSR_REG & 0x01)); // Wait until RX has data
    return *AUX_MU_IO_REG;
}

void mini_uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') mini_uart_send('\r');
        mini_uart_send(*s++);
    }
}
```
    
`main.c`
```c
extern void gpio_init_mini_uart(void);
extern void mini_uart_init(void);
extern void mini_uart_puts(const char *s);

void main(void) {
    mini_uart_init();
    mini_uart_puts("mini UART initialized successfully!\n");
    while (1);
}
```
   
</details>

### Basic Exercise 3 - Simple Shell - 20%

After setting up UART, you should implement a simple shell to let rpi3
interact with the host computer. The shell should be able to execute the
following commands.

| command | Description                  |
|---------|------------------------------|
| help    | print all available commands |
| hello   | print Hello World!           |

> [!IMPORTANT]
> There may be some text alignment issue on screen IO, think about \r\n
> on both input and output.

> [!TIP] Todo
>
> Implement a simple shell supporting the listed commands.

<details open>
    <summary>Implementation</summary>
要注意bare-metal沒有那些string.h之類的要手動實作

```c
#define MAX_CMD_LEN 64

void shell(void);
int strcmp(const char *s1, const char *s2);
int strlen(const char *s);

void main(void) {
    mini_uart_init();
    mini_uart_puts("Welcome to mini UART shell!\r\n");
    shell();   // 進入互動模式
}

void shell(void) {
    char buffer[MAX_CMD_LEN];
    int index = 0;
    mini_uart_puts("# ");  // 提示符

    while (1) {
        char c = mini_uart_recv();

        // 處理 Enter
        if (c == '\r' || c == '\n') {
            mini_uart_puts("\r\n");
            buffer[index] = '\0';  // 結尾字元

            // ===== 指令解析 =====
            if (index == 0) {
                // 空輸入，直接顯示新提示符
            } else if (strcmp(buffer, "help") == 0) {
                mini_uart_puts("Available commands:\r\n");
                mini_uart_puts("  help  : print all available commands\r\n");
                mini_uart_puts("  hello : print Hello World!\r\n");
                mini_uart_puts("  info : print system information\r\n");
                mini_uart_puts("  reboot: reboot the device\r\n");
            } else if (strcmp(buffer, "hello") == 0) {
                mini_uart_puts("Hello World!\r\n");
            
            } else if (strcmp(buffer, "info") == 0) {
            } else if (strcmp(buffer, "reboot") == 0) {
            } else {
                mini_uart_puts("Unknown command: ");
                mini_uart_puts(buffer);
                mini_uart_puts("\r\n");
            }

            // 重置狀態
            index = 0;
            mini_uart_puts("# ");
        }

        // 處理 Backspace
        else if (c == 127 || c == '\b') {
            if (index > 0) {
                index--;
                mini_uart_puts("\b \b"); // 往後刪除
            }
        }

        // 一般輸入字元
        else {
            if (index < MAX_CMD_LEN - 1) {
                buffer[index++] = c;
                mini_uart_send(c);  // Echo 顯示
            }
        }
    }
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strlen(const char *s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

```
    
</details>

![image](https://hackmd.io/_uploads/B1d6hR6J-x.png)

### Basic Exercise 4 - Mailbox - 20%

ARM CPU is able to configure peripherals such as clock rate and
framebuffer by calling VideoCoreIV(GPU) routines. Mailboxes are the
communication bridge between them.

You can refer to `mailbox` for more information.

#### Get the hardware's information

Get the hardware's information is the easiest thing could be done by
mailbox. Check if you implement mailbox communication correctly by
verifying the hardware information's correctness.

> [!TIP] Todo
>
> Get the hardware's information by mailbox and print them, you should at
> least print **board revision** and **ARM memory base address and size**.
>
    
<details open>
    <summary>Implementation</summary>

```c

#define MMIO_BASE        0x3F000000
#define MAILBOX_BASE     (MMIO_BASE + 0xB880)

#define MAILBOX_READ     ((volatile unsigned int*)(MAILBOX_BASE + 0x00))
#define MAILBOX_STATUS   ((volatile unsigned int*)(MAILBOX_BASE + 0x18))
#define MAILBOX_WRITE    ((volatile unsigned int*)(MAILBOX_BASE + 0x20))

#define MAILBOX_FULL     0x80000000
#define MAILBOX_EMPTY    0x40000000

// 通道號 (channel)
#define MBOX_CH_PROP     8    // Property channel for GPU queries

// Tags (property messages)
#define MBOX_TAG_GET_BOARD_REVISION  0x00010002
#define MBOX_TAG_GET_ARM_MEMORY      0x00010005
#define MBOX_TAG_LAST                0x00000000

// 等待 mailbox 可寫入 / 讀取
int mailbox_call(unsigned char ch, unsigned int *mailbox_msg) {
    unsigned int addr = (unsigned int)((unsigned long)mailbox_msg);
    if (addr & 0xF) return 0;  // 必須 16-byte 對齊

    // 寫入 (等待非滿)
    while (*MAILBOX_STATUS & MAILBOX_FULL);
    *MAILBOX_WRITE = addr | (ch & 0xF);

    // 讀取回覆
    while (1) {
        while (*MAILBOX_STATUS & MAILBOX_EMPTY);
        unsigned int resp = *MAILBOX_READ;
        if ((resp & 0xF) == ch && (resp & ~0xF) == addr)
            return mailbox_msg[1] == 0x80000000; // 成功標誌
    }
}

int get_arm_memory_info(unsigned int *mailbox) {

    // 取得 ARM memory
    mailbox[0] = 8 * 4;
    mailbox[1] = 0;
    mailbox[2] = MBOX_TAG_GET_ARM_MEMORY;
    mailbox[3] = 8;
    mailbox[4] = 0;
    mailbox[5] = 0; // base
    mailbox[6] = 0; // size
    mailbox[7] = MBOX_TAG_LAST;

    return mailbox_call(MBOX_CH_PROP, mailbox);
}

int get_board_revision(unsigned int *mailbox) {

    // 取得板子版本
    mailbox[0] = 7 * 4;
    mailbox[1] = 0;
    mailbox[2] = MBOX_TAG_GET_BOARD_REVISION;
    mailbox[3] = 4;
    mailbox[4] = 0;
    mailbox[5] = 0; // board revision
    mailbox[6] = MBOX_TAG_LAST;

    return mailbox_call(MBOX_CH_PROP, mailbox);
}

```
    
</details>

## Advanced Exercises

### Advanced Exercise 1 - Reboot - 30%

Rpi3 doesn't originally provide an on board reset button.

You can follow this example code to reset your rpi3.

> [!IMPORTANT]
> This snippet of code only works on real rpi3, not on QEMU.

``` c
#define PM_PASSWORD 0x5a000000
#define PM_RSTC 0x3F10001c
#define PM_WDOG 0x3F100024

void set(long addr, unsigned int value) {
    volatile unsigned int* point = (unsigned int*)addr;
    *point = value;
}

void reset(int tick) {                 // reboot after watchdog timer expire
    set(PM_RSTC, PM_PASSWORD | 0x20);  // full reset
    set(PM_WDOG, PM_PASSWORD | tick);  // number of watchdog tick
}

void cancel_reset() {
    set(PM_RSTC, PM_PASSWORD | 0);  // full reset
    set(PM_WDOG, PM_PASSWORD | 0);  // number of watchdog tick
}
```

> [!TIP] Todo
>
> Add a \<reboot\> command.
>
    
<details open>
    <summary>Implementation</summary>

```c
#include "reboot.h"

#define PM_PASSWORD 0x5a000000
#define PM_RSTC     0x3F10001c
#define PM_WDOG     0x3F100024

static inline void set(long addr, unsigned int value) {
    volatile unsigned int* point = (unsigned int*)addr;
    *point = value;
}

void reset(int tick) {                 // reboot after watchdog timer expire
    set(PM_RSTC, PM_PASSWORD | 0x20);  // full reset
    set(PM_WDOG, PM_PASSWORD | tick);  // number of watchdog tick
}

void cancel_reset(void) {
    set(PM_RSTC, PM_PASSWORD | 0);
    set(PM_WDOG, PM_PASSWORD | 0);
}
```

</details>
