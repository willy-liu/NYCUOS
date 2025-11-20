#include "uart.h"
#include "gpio.h"

#define KERNEL_LOAD_ADDR 0x80000

static unsigned int uart_get_u32(void)
{
    unsigned int b0 = (unsigned char)uart_getc();
    unsigned int b1 = (unsigned char)uart_getc();
    unsigned int b2 = (unsigned char)uart_getc();
    unsigned int b3 = (unsigned char)uart_getc();
    return (b0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

int main(void) {
    gpio_init_uart();
    uart_init();

    uart_puts("bootloader: UART initialized successfully!\r\n");
    uart_puts("bootloader: waiting for loading kernel...\r\n");

    // 先把 RX 清乾淨，避免上一次 boot / 垃圾字元干擾這次的 size
    uart_flush_rx();

    // 1. 接收 kernel 大小
    uart_puts("bootloader: waiting for kernel size...\r\n");
    unsigned int kernel_size = uart_get_u32();

    uart_puts("bootloader: kernel size = ");
    print_hex(kernel_size);
    uart_puts("\r\n");

    if (kernel_size == 0) {
        uart_puts("bootloader: ERROR — kernel size is 0, abort.\r\n");
        while (1) { }
    }

    // 2. 接收 kernel 本體
    uart_puts("bootloader: receiving kernel...\r\n");
    unsigned char *kernel = (unsigned char *)KERNEL_LOAD_ADDR;

    for (unsigned int i = 0; i < kernel_size; i++) {
        kernel[i] = (unsigned char)uart_getc();   // blocking read
    }

    uart_puts("bootloader: kernel received successfully!\r\n");

    // 收完再清一次 RX，避免多餘資料影響 kernel 開機（例如 Python 亂丟）
    uart_flush_rx();

    // 3. 跳到 kernel entry
    uart_puts("bootloader: jumping to kernel...\r\n");
    uart_flush_tx();

    void (*kernel_entry)(void) = (void *)KERNEL_LOAD_ADDR;
    kernel_entry();   // never returns

    return 0; // never reached
}
