#ifndef _UART_H_
#define _UART_H_

#include <stddef.h>
#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *s);
int uart_rx_empty(void);
int uart_tx_busy(void);
void uart_flush_rx(void);
void uart_flush_tx(void);
void uart_enable_interrupts(void);
void uart_irq_handler(void);
int uart_irq_pending(void);
void uart_irq_mask(void);
void uart_irq_unmask(void);
void uart_irq_top_half(void);
void uart_irq_bottom_half(void);
size_t uart_async_read(char *buf, size_t max_len);
size_t uart_async_write(const char *buf, size_t len);

void print_hex(unsigned int value);
void print_hex64(uint64_t value);
#endif /* _UART_H_ */
