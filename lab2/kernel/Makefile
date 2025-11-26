# =======================
# Raspberry Pi 3 Bare-metal Makefile
# =======================

CROSS   = aarch64-linux-gnu
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy

CFLAGS  = -Wall -O2 -ffreestanding -nostdlib
CFLAGS += -I../../uart
LDFLAGS = -T linker.ld

UART_H = ../../uart/uart.h
UART_C = ../../uart/uart.c

GPIO_H = ../../uart/gpio.h
GPIO_C = ../../uart/gpio.c

# =======================
# Source / Object files
# =======================
OBJS = boot.o main.o uart.o gpio.o cpio.o

# =======================
# Build rules
# =======================
all: kernel8.img

# 組語
boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S -o $@

# C 原始碼
main.o: main.c $(UART_H) $(GPIO_H)
	$(CC) $(CFLAGS) -c main.c -o $@

uart.o: $(UART_C) $(UART_H) $(GPIO_H)
	$(CC) $(CFLAGS) -c $(UART_C) -o $@

gpio.o: $(GPIO_C) $(GPIO_H)
	$(CC) $(CFLAGS) -c $(GPIO_C) -o $@

cpio.o: cpio.c cpio.h $(UART_H) $(GPIO_H)
	$(CC) $(CFLAGS) -c cpio.c -o $@

# 連結
kernel8.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 轉成 binary
kernel8.img: kernel8.elf
	$(OBJCOPY) -O binary $< $@

# =======================
# Run / Clean
# =======================
run: kernel8.img
	qemu-system-aarch64 \
    -M raspi3b \
    -kernel kernel8.img \
    -serial stdio \
	-serial null \
    -display none \
	-initrd initramfs.cpio

clean:
	rm -f *.o *.elf *.img
