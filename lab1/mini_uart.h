#ifndef _MINI_UART_H_
#define _MINI_UART_H_

void mini_uart_init(void);
void mini_uart_send(char c);
char mini_uart_recv(void);
void mini_uart_puts(const char *s);

#endif /* _MINI_UART_H_ */