# =======================
# Raspberry Pi 3 Bare-metal Makefile
# =======================

CROSS   = aarch64-linux-gnu
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy

CFLAGS  = -Wall -O2 -ffreestanding -nostdlib
LDFLAGS = -T linker.ld

# =======================
# Source / Object files
# =======================
COMMON_OBJS = boot.o gpio.o

# =======================
# Build rules
# =======================
.PHONY: all mini_uart uart run_mini_uart run_uart clean

all: mini_uart uart

# -----------------------
# mini UART 版本
# -----------------------
mini_uart: mini_uart_kernel8.img

mini_uart_kernel8.img: mini_uart_kernel8.elf
	$(OBJCOPY) -O binary mini_uart_kernel8.elf mini_uart_kernel8.img

mini_uart_kernel8.elf: $(COMMON_OBJS) mini_uart.o mini_uart_main.o linker.ld
	$(LD) $(LDFLAGS) -o mini_uart_kernel8.elf $(COMMON_OBJS) mini_uart.o mini_uart_main.o

mini_uart.o: mini_uart.c gpio.h
	$(CC) $(CFLAGS) -c mini_uart.c -o mini_uart.o

mini_uart_main.o: mini_uart_main.c gpio.h
	$(CC) $(CFLAGS) -c mini_uart_main.c -o mini_uart_main.o

# -----------------------
# PL011 UART 版本
# -----------------------
uart: uart_kernel8.img

uart_kernel8.img: uart_kernel8.elf
	$(OBJCOPY) -O binary uart_kernel8.elf uart_kernel8.img

uart_kernel8.elf: $(COMMON_OBJS) uart.o uart_main.o linker.ld
	$(LD) $(LDFLAGS) -o uart_kernel8.elf $(COMMON_OBJS) uart.o uart_main.o

uart.o: uart.c gpio.h
	$(CC) $(CFLAGS) -c uart.c -o uart.o

uart_main.o: uart_main.c gpio.h
	$(CC) $(CFLAGS) -c uart_main.c -o uart_main.o

# -----------------------
# 共用組語 / GPIO
# -----------------------
boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S -o boot.o

gpio.o: gpio.c gpio.h
	$(CC) $(CFLAGS) -c gpio.c -o gpio.o

# =======================
# Run / Clean
# =======================
run_mini_uart: mini_uart
	qemu-system-aarch64 -M raspi3b -kernel mini_uart_kernel8.img -serial null -serial stdio -display none

run_uart: uart
	qemu-system-aarch64 -M raspi3b -kernel uart_kernel8.img -serial stdio -display none

clean:
	rm -f *.o *.elf *.img
