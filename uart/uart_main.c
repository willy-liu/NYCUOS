extern void uart_init(void);
extern void uart_puts(const char *s);

void main(void) {
    uart_init();
    uart_puts("UART initialized successfully!\n");
    while (1);
}
