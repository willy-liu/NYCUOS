#include "gpio.h"
#include "stdint.h"
#include "uart.h"

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
#define UART0_MIS       ((volatile unsigned int*)(UART0_BASE + 0x40))
#define UART0_ICR       ((volatile unsigned int*)(UART0_BASE + 0x44))

#define IRQ_PENDING_2   ((volatile unsigned int*)(MMIO_BASE + 0xB208))
#define ENABLE_IRQS_2   ((volatile unsigned int*)(MMIO_BASE + 0xB214))

#define UART_IRQ_ID     57u
#define UART_IRQ_BIT    (1u << (UART_IRQ_ID - 32u))

#define UART_IMSC_RXIM  (1u << 4)
#define UART_IMSC_TXIM  (1u << 5)
#define UART_IMSC_RTIM  (1u << 6)

#define RX_BUF_SIZE 256
#define TX_BUF_SIZE 1024

static volatile unsigned int rx_head = 0;
static volatile unsigned int rx_tail = 0;
static volatile unsigned int tx_head = 0;
static volatile unsigned int tx_tail = 0;
static volatile unsigned int deferred_mis = 0;
static char rx_buf[RX_BUF_SIZE];
static char tx_buf[TX_BUF_SIZE];

static uint64_t irq_save(void) {
    uint64_t daif;
    asm volatile("mrs %0, daif" : "=r"(daif));
    asm volatile("msr daifset, #2");
    return daif;
}

static void irq_restore(uint64_t daif) {
    asm volatile("msr daif, %0" :: "r"(daif));
}

static int rx_push(char c) {
    unsigned int next = (rx_head + 1) % RX_BUF_SIZE;
    if (next == rx_tail)
        return 0;
    rx_buf[rx_head] = c;
    rx_head = next;
    return 1;
}

static int rx_pop(char *out) {
    if (rx_tail == rx_head)
        return 0;
    *out = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return 1;
}

static int tx_push(char c) {
    unsigned int next = (tx_head + 1) % TX_BUF_SIZE;
    if (next == tx_tail)
        return 0;
    tx_buf[tx_head] = c;
    tx_head = next;
    return 1;
}

static int tx_pop(char *out) {
    if (tx_tail == tx_head)
        return 0;
    *out = tx_buf[tx_tail];
    tx_tail = (tx_tail + 1) % TX_BUF_SIZE;
    return 1;
}

static void uart_tx_kick_locked(void) {
    char c;
    while (!(*UART0_FR & (1 << 5)) && tx_pop(&c)) {
        *UART0_DR = (unsigned int)c;
    }

    if (tx_tail != tx_head) {
        *UART0_IMSC |= UART_IMSC_TXIM;
    } else {
        *UART0_IMSC &= ~UART_IMSC_TXIM;
    }
}

void uart_init(void) {
    gpio_init_uart();              // Initialize GPIO pins for UART
    
    *UART0_CR = 0x00000000;        // 關閉 UART
    *UART0_ICR = 0x7FF;            // 清除所有中斷

    // 波特率設定：115200 bps (UARTCLK = 48 MHz)
    *UART0_IBRD = 26;              // 整數部分
    *UART0_FBRD = 3;               // 小數部分

    *UART0_LCRH = (3 << 5);        // 8 bits，無 parity，1 stop bit
    *UART0_CR = 0x301;             // 啟用 UART、TX、RX
    *UART0_IMSC = 0;
}

void uart_enable_interrupts(void) {
    uint64_t flags = irq_save();

    *UART0_ICR = 0x7FF;
    *UART0_IMSC = UART_IMSC_RXIM | UART_IMSC_RTIM;
    *ENABLE_IRQS_2 = UART_IRQ_BIT;

    irq_restore(flags);
}

void uart_irq_mask(void) {
    *UART0_IMSC &= ~(UART_IMSC_RXIM | UART_IMSC_RTIM | UART_IMSC_TXIM);
}

void uart_irq_unmask(void) {
    *UART0_IMSC |= UART_IMSC_RXIM | UART_IMSC_RTIM;
    if (tx_tail != tx_head) {
        *UART0_IMSC |= UART_IMSC_TXIM;
    }
}

void uart_irq_top_half(void) {
    unsigned int mis = *UART0_MIS;
    if (!mis) {
        return;
    }

    uart_irq_mask();
    deferred_mis |= mis;

    if (mis & (UART_IMSC_RXIM | UART_IMSC_RTIM)) {
        while (!(*UART0_FR & (1 << 4))) {
            (void)rx_push((char)(*UART0_DR & 0xFF));
        }
        *UART0_ICR = UART_IMSC_RXIM | UART_IMSC_RTIM;
    }

    if (mis & UART_IMSC_TXIM) {
        *UART0_ICR = UART_IMSC_TXIM;
    }
}

void uart_irq_bottom_half(void) {
    uint64_t flags = irq_save();

    if (deferred_mis & UART_IMSC_TXIM) {
        uart_tx_kick_locked();
    }
    deferred_mis = 0;

    uart_irq_unmask();
    irq_restore(flags);
}

void uart_irq_handler(void) {
    uart_irq_top_half();
    uart_irq_bottom_half();
}

int uart_irq_pending(void) {
    return ((*IRQ_PENDING_2) & UART_IRQ_BIT) != 0;
}

size_t uart_async_read(char *buf, size_t max_len) {
    size_t n = 0;
    uint64_t flags = irq_save();

    while (n < max_len && rx_pop(&buf[n])) {
        n++;
    }

    irq_restore(flags);
    return n;
}

size_t uart_async_write(const char *buf, size_t len) {
    size_t n = 0;
    uint64_t flags = irq_save();

    while (n < len) {
        char c = buf[n];
        if (c == '\n') {
            if (!tx_push('\r'))
                break;
        }
        if (!tx_push(c))
            break;
        n++;
    }

    uart_tx_kick_locked();
    irq_restore(flags);
    return n;
}

void uart_putc(char c) {
    while (*UART0_FR & (1 << 5));   // TXFF = 1 表示滿
    *UART0_DR = c;
}

char uart_getc(void) {
    while (*UART0_FR & (1 << 4));   // RXFE = 1 表示空
    return *UART0_DR;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

int uart_rx_empty(void) {
    return (*UART0_FR & (1 << 4)) != 0;
}

int uart_tx_busy(void) {
    return (*UART0_FR & (1 << 3)) != 0;
}

void uart_flush_rx(void) {
    while (!uart_rx_empty()) {
        (void)uart_getc();
    }
}

void uart_flush_tx(void) {
    while (uart_tx_busy()) {
        // BUSY=1 → still sending
    }
}

void print_hex(unsigned int value) {
    const char hex_chars[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc(hex_chars[(value >> i) & 0xF]);
    }
}

void print_hex64(uint64_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex_chars[(value >> i) & 0xF]);
    }
}
