#include "uart.h"
#include "gpio.h"
#include "cpio.h"

int main() {
    gpio_init_uart();
    uart_puts("this is kernel cpio test!\n");
    // cpio_set_base((void*)0x8000000); // qemu initramfs base address 0x8000000
    cpio_set_base((void*)0x20000000); // raspi3b initramfs base address 0x20000000
    uart_puts("Listing all files in cpio archive:\n");
    cpio_list_all();
    char cmd[128];
    while (1) {
        uart_puts("> ");
        int len = 0;
        for (;;) {
            char c = uart_getc();
            if (c == '\r' || c == '\n') {
                uart_puts("\n");
                cmd[len] = '\0';
                break;
            }
            if ((c == '\b' || c == 127)) {
                if (len > 0) {
                    uart_puts("\b \b");
                    len--;
                }
                continue;
            }
            if (len < (int)sizeof(cmd) - 1) {
                uart_putc(c);
                cmd[len++] = c;
            }
        }
        char *p = cmd;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            continue;
        }
        if (p[0] == 'l' && p[1] == 's' && (p[2] == '\0' || p[2] == ' ' || p[2] == '\t')) {
            cpio_list_all();
            continue;
        }
        if (p[0] == 'c' && p[1] == 'a' && p[2] == 't' && (p[3] == '\0' || p[3] == ' ' || p[3] == '\t')) {
            char *path = p + 3;
            while (*path == ' ' || *path == '\t') {
                path++;
            }
            if (*path == '\0') {
                uart_puts("path required\n");
                continue;
            }
            unsigned long size = 0;
            const char *data = (const char *)cpio_get_file(path, &size);
            if (!data) {
                uart_puts("file not found\n");
                continue;
            }
            for (unsigned long i = 0; i < size; i++) {
                uart_putc(data[i]);
            }
            if (size == 0 || data[size - 1] != '\n') {
                uart_puts("\n");
            }
            continue;
        }
        uart_puts("unknown command\n");
    }
}