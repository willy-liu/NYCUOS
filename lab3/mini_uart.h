#ifndef _MINI_UART_H_
#define _MINI_UART_H_

#include <stddef.h>
#include <stdint.h>

// Header-swap shim: including mini_uart.h rewrites uart_* calls to mini_uart_*.
#define uart_init             mini_uart_init
#define uart_putc             mini_uart_putc
#define uart_getc             mini_uart_getc
#define uart_puts             mini_uart_puts
#define uart_rx_empty         mini_uart_rx_empty
#define uart_tx_busy          mini_uart_tx_busy
#define uart_flush_rx         mini_uart_flush_rx
#define uart_flush_tx         mini_uart_flush_tx
#define uart_enable_interrupts mini_uart_enable_interrupts
#define uart_irq_handler      mini_uart_irq_handler
#define uart_irq_pending      mini_uart_irq_pending
#define uart_irq_mask         mini_uart_irq_mask
#define uart_irq_unmask       mini_uart_irq_unmask
#define uart_irq_top_half     mini_uart_irq_top_half
#define uart_irq_bottom_half  mini_uart_irq_bottom_half
#define uart_async_read       mini_uart_async_read
#define uart_async_write      mini_uart_async_write
#define print_hex             mini_uart_print_hex
#define print_hex64           mini_uart_print_hex64

void mini_uart_init(void);
void mini_uart_putc(char c);
char mini_uart_getc(void);
void mini_uart_puts(const char *s);
int mini_uart_rx_empty(void);
int mini_uart_tx_busy(void);
void mini_uart_flush_rx(void);
void mini_uart_flush_tx(void);
void mini_uart_enable_interrupts(void);
void mini_uart_irq_handler(void);
int mini_uart_irq_pending(void);
void mini_uart_irq_mask(void);
void mini_uart_irq_unmask(void);
void mini_uart_irq_top_half(void);
void mini_uart_irq_bottom_half(void);
size_t mini_uart_async_read(char *buf, size_t max_len);
size_t mini_uart_async_write(const char *buf, size_t len);

void mini_uart_print_hex(unsigned int value);
void mini_uart_print_hex64(uint64_t value);

#endif /* _MINI_UART_H_ */
