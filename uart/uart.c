#include "gpio.h"

#define MMIO_BASE       0x3F000000
#define UART0_BASE      (MMIO_BASE + 0x201000)

// PL011 registers
#define UART0_DR        ((volatile unsigned int*)(UART0_BASE + 0x00))
#define UART0_FR        ((volatile unsigned int*)(UART0_BASE + 0x18))
#define UART0_IBRD      ((volatile unsigned int*)(UART0_BASE + 0x24))
#define UART0_FBRD      ((volatile unsigned int*)(UART0_BASE + 0x28))
#define UART0_LCRH      ((volatile unsigned int*)(UART0_BASE + 0x2C))
#define UART0_CR        ((volatile unsigned int*)(UART0_BASE + 0x30))
#define UART0_IMSC      ((volatile unsigned int*)(UART0_BASE + 0x38))
#define UART0_ICR       ((volatile unsigned int*)(UART0_BASE + 0x44))

void uart_init(void) {
    gpio_init_uart();              // Initialize GPIO pins for UART
    
    *UART0_CR = 0x00000000;        // 關閉 UART
    *UART0_ICR = 0x7FF;            // 清除所有中斷

    // 波特率設定：115200 bps (UARTCLK = 48 MHz)
    *UART0_IBRD = 26;              // 整數部分
    *UART0_FBRD = 3;               // 小數部分

    *UART0_LCRH = (3 << 5);        // 8 bits，無 parity，1 stop bit
    *UART0_CR = 0x301;             // 啟用 UART、TX、RX
}

void uart_send(char c) {
    while (*UART0_FR & (1 << 5));   // TXFF = 1 表示滿
    *UART0_DR = c;
}

char uart_recv(void) {
    while (*UART0_FR & (1 << 4));   // RXFE = 1 表示空
    return *UART0_DR;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_send('\r');
        uart_send(*s++);
    }
}