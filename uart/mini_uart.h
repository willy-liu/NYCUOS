#ifndef _MINI_UART_H_
#define _MINI_UART_H_

void mini_uart_init(void);
void mini_uart_putc(char c);
char mini_uart_getc(void);
void mini_uart_puts(const char *s);
int mini_uart_rx_empty(void);
int mini_uart_tx_busy(void);
void mini_uart_flush_rx(void);
void mini_uart_flush_tx(void);

void print_hex(unsigned int value);

#endif /* _MINI_UART_H_ */