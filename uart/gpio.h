#pragma once

#define MMIO_BASE   0x3F000000

// GPIO registers
#define GPFSEL1     ((volatile unsigned int*)(MMIO_BASE + 0x200004))
#define GPPUD       ((volatile unsigned int*)(MMIO_BASE + 0x200094))
#define GPPUDCLK0   ((volatile unsigned int*)(MMIO_BASE + 0x200098))

// GPIO function codes
#define GPIO_FUNC_INPUT   0
#define GPIO_FUNC_OUTPUT  1
#define GPIO_FUNC_ALT0    4
#define GPIO_FUNC_ALT5    2

// Function prototypes
void gpio_set_func(int pin, int func);
void gpio_disable_pud(int pin);
void gpio_init_mini_uart(void);
void gpio_init_uart(void);
