#ifndef _UART_H_
#define _UART_H_

void uart_init(void);
void uart_send(char c);
char uart_getc(void);
void uart_puts(const char *s);
int uart_rx_empty(void);
int uart_tx_busy(void);
void uart_flush_rx(void);
void uart_flush_tx(void);

void print_hex(unsigned int value);
#endif /* _UART_H_ */