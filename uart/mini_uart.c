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
    gpio_init_mini_uart();      // Initialize GPIO pins for mini UART

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

char mini_uart_getc(void) {
    while (!(*AUX_MU_LSR_REG & 0x01)); // Wait until RX has data
    return *AUX_MU_IO_REG;
}

void mini_uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') mini_uart_send('\r');
        mini_uart_send(*s++);
    }
}

int mini_uart_rx_empty(void) {
    return (*AUX_MU_LSR_REG & 0x01) == 0;
}

int mini_uart_tx_busy(void) {
    return (*AUX_MU_LSR_REG & 0x20) == 0;
}

void mini_uart_flush_rx(void) {
    while (!mini_uart_rx_empty()) {
        (void)mini_uart_getc();
    }
}

void mini_uart_flush_tx(void) {
    while (mini_uart_tx_busy()) {
        // Wait until TX empty
    }
}

void print_hex(unsigned int value) {
    const char hex_chars[] = "0123456789ABCDEF";
    mini_uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        char hex_digit = hex_chars[(value >> i) & 0xF];
        mini_uart_send(hex_digit);
    }
}