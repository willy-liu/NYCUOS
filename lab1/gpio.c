#include "gpio.h"

void gpio_set_func(int pin, int func) {
    volatile unsigned int *reg = GPFSEL1 + (pin / 10) - 1;
    int shift = (pin % 10) * 3;
    unsigned int val = *reg;
    val &= ~(7 << shift);
    val |=  (func << shift);
    *reg = val;
}

void gpio_disable_pud(int pin) {
    *GPPUD = 0;
    for (int i = 0; i < 150; i++) asm volatile("nop");
    *GPPUDCLK0 = (1 << pin);
    for (int i = 0; i < 150; i++) asm volatile("nop");
    *GPPUDCLK0 = 0;
}

void gpio_init_mini_uart(void) {
    gpio_set_func(14, GPIO_FUNC_ALT5); // TXD1
    gpio_set_func(15, GPIO_FUNC_ALT5); // RXD1
    gpio_disable_pud(14);
    gpio_disable_pud(15);
}
