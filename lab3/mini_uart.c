#include "gpio.h"
#include "stdint.h"
#include "mini_uart.h"

#define MMIO_BASE        0x3F000000
#define AUX_BASE         (MMIO_BASE + 0x215000)

#define AUX_IRQ          ((volatile unsigned int*)(AUX_BASE + 0x00))
#define AUX_ENABLES      ((volatile unsigned int*)(AUX_BASE + 0x04))
#define AUX_MU_IO_REG    ((volatile unsigned int*)(AUX_BASE + 0x40))
#define AUX_MU_IER_REG   ((volatile unsigned int*)(AUX_BASE + 0x44))
#define AUX_MU_IIR_REG   ((volatile unsigned int*)(AUX_BASE + 0x48))
#define AUX_MU_LCR_REG   ((volatile unsigned int*)(AUX_BASE + 0x4C))
#define AUX_MU_MCR_REG   ((volatile unsigned int*)(AUX_BASE + 0x50))
#define AUX_MU_LSR_REG   ((volatile unsigned int*)(AUX_BASE + 0x54))
#define AUX_MU_CNTL_REG  ((volatile unsigned int*)(AUX_BASE + 0x60))
#define AUX_MU_BAUD_REG  ((volatile unsigned int*)(AUX_BASE + 0x68))

#define IRQ_PENDING_1    ((volatile unsigned int*)(MMIO_BASE + 0xB204))
#define ENABLE_IRQS_1    ((volatile unsigned int*)(MMIO_BASE + 0xB210))

#define MINI_UART_IRQ_BIT (1u << 29)
#define MU_IER_RX_INT    (1u << 0)
#define MU_IER_TX_INT    (1u << 1)

#define RX_BUF_SIZE 256
#define TX_BUF_SIZE 1024

static volatile unsigned int rx_head = 0;
static volatile unsigned int rx_tail = 0;
static volatile unsigned int tx_head = 0;
static volatile unsigned int tx_tail = 0;
static volatile unsigned int deferred_flags = 0;
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

static void mini_uart_tx_kick_locked(void) {
    char c;
    while ((*AUX_MU_LSR_REG & 0x20) && tx_pop(&c)) {
        *AUX_MU_IO_REG = (unsigned int)c;
    }

    if (tx_tail != tx_head) {
        *AUX_MU_IER_REG |= MU_IER_TX_INT;
    } else {
        *AUX_MU_IER_REG &= ~MU_IER_TX_INT;
    }
}

void mini_uart_init(void) {
    gpio_init_mini_uart();

    *AUX_ENABLES |= 1;
    *AUX_MU_CNTL_REG = 0;
    *AUX_MU_IER_REG = 0;
    *AUX_MU_LCR_REG = 3;
    *AUX_MU_MCR_REG = 0;
    *AUX_MU_IIR_REG = 0xC6;
    *AUX_MU_BAUD_REG = 270;
    *AUX_MU_CNTL_REG = 3;
}

void mini_uart_enable_interrupts(void) {
    uint64_t flags = irq_save();

    *AUX_MU_IIR_REG = 0xC6;
    *AUX_MU_IER_REG = MU_IER_RX_INT;
    *ENABLE_IRQS_1 = MINI_UART_IRQ_BIT;

    irq_restore(flags);
}

void mini_uart_irq_mask(void) {
    *AUX_MU_IER_REG &= ~(MU_IER_RX_INT | MU_IER_TX_INT);
}

void mini_uart_irq_unmask(void) {
    *AUX_MU_IER_REG |= MU_IER_RX_INT;
    if (tx_tail != tx_head) {
        *AUX_MU_IER_REG |= MU_IER_TX_INT;
    }
}

void mini_uart_irq_top_half(void) {
    if (!mini_uart_irq_pending()) {
        return;
    }

    mini_uart_irq_mask();
    deferred_flags = 1;

    while (*AUX_MU_LSR_REG & 0x01) {
        (void)rx_push((char)(*AUX_MU_IO_REG & 0xFF));
    }

    (void)*AUX_IRQ;
}

void mini_uart_irq_bottom_half(void) {
    uint64_t flags = irq_save();

    if (deferred_flags) {
        mini_uart_tx_kick_locked();
    }
    deferred_flags = 0;
    mini_uart_irq_unmask();

    irq_restore(flags);
}

void mini_uart_irq_handler(void) {
    mini_uart_irq_top_half();
    mini_uart_irq_bottom_half();
}

int mini_uart_irq_pending(void) {
    return ((*IRQ_PENDING_1) & MINI_UART_IRQ_BIT) != 0;
}

size_t mini_uart_async_read(char *buf, size_t max_len) {
    size_t n = 0;
    uint64_t flags = irq_save();

    while (n < max_len && rx_pop(&buf[n])) {
        n++;
    }

    irq_restore(flags);
    return n;
}

size_t mini_uart_async_write(const char *buf, size_t len) {
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

    mini_uart_tx_kick_locked();
    irq_restore(flags);
    return n;
}

void mini_uart_putc(char c) {
    while (!(*AUX_MU_LSR_REG & 0x20));
    *AUX_MU_IO_REG = (unsigned int)c;
}

char mini_uart_getc(void) {
    while (!(*AUX_MU_LSR_REG & 0x01));
    return (char)(*AUX_MU_IO_REG & 0xFF);
}

void mini_uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') mini_uart_putc('\r');
        mini_uart_putc(*s++);
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
    }
}

void mini_uart_print_hex(unsigned int value) {
    const char hex_chars[] = "0123456789ABCDEF";
    mini_uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        mini_uart_putc(hex_chars[(value >> i) & 0xF]);
    }
}

void mini_uart_print_hex64(uint64_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    mini_uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        mini_uart_putc(hex_chars[(value >> i) & 0xF]);
    }
}
