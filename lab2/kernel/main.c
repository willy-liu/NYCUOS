#include "uart.h"
#include "gpio.h"
#include "cpio.h"

int main() {
    gpio_init_uart();
    uart_puts("this is kernel cpio test!\n");
    cpio_set_base((void*)0x8000000); // qemu initramfs base address
    cpio_list_all();
}