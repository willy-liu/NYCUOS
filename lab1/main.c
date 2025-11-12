extern void gpio_init_mini_uart(void);
extern void mini_uart_init(void);
extern void mini_uart_puts(const char *s);

void main(void) {
    mini_uart_init();
    mini_uart_puts("mini UART initialized successfully!\n");
    while (1);
}
