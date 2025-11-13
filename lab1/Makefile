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
OBJS = boot.o main.o mini_uart.o gpio.o mailbox.o

# =======================
# Build rules
# =======================
all: kernel8.img

# 組語
boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S -o $@

# C 原始碼
main.o: main.c mini_uart.h gpio.h
	$(CC) $(CFLAGS) -c main.c -o $@

mini_uart.o: mini_uart.c mini_uart.h gpio.h
	$(CC) $(CFLAGS) -c mini_uart.c -o $@

gpio.o: gpio.c gpio.h
	$(CC) $(CFLAGS) -c gpio.c -o $@

mailbox.o: mailbox.c mailbox.h
	$(CC) $(CFLAGS) -c mailbox.c -o $@

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
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial null -serial stdio -display none

clean:
	rm -f *.o *.elf *.img
