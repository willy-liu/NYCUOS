#ifndef _MINI_UART_H_
#define _MINI_UART_H_

void mini_uart_init(void);
void mini_uart_send(char c);
char mini_uart_getc(void);
void mini_uart_puts(const char *s);
int mini_uart_rx_empty(void);
void mini_uart_flush_tx(void);

#endif /* _MINI_UART_H_ */