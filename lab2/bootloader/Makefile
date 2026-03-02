# =======================
# Raspberry Pi 3 Bare-metal Makefile (Recommended)
# =======================

CROSS   = aarch64-linux-gnu
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy

# =======================
# Compiler / Linker flags
# =======================
CFLAGS  = -Wall -O2 -ffreestanding -nostdlib
CFLAGS += -I../../uart

LDFLAGS = -T linker.ld

# =======================
# Source search paths
# =======================
VPATH = . ../../uart

# =======================
# Objects
# =======================
BOOT_OBJS   = boot.o
BOOTLOADER_OBJS = main.o uart.o gpio.o
OBJS = $(BOOT_OBJS) $(BOOTLOADER_OBJS)

# =======================
# Build rules
# =======================
all: bootloader.img

# Assembly
boot.o: boot.S
	$(CC) $(CFLAGS) -c $< -o $@

# Generic C rule (all .c -> .o)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link
bootloader.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJS) $(BOOTLOADER_OBJS)

# Binary
bootloader.img: bootloader.elf
	$(OBJCOPY) -O binary $< $@

# =======================
# Run / Clean
# =======================
# PL011 UART -> PTY; Mini UART -> stdio
run: bootloader.img
	qemu-system-aarch64 \
    -M raspi3b \
    -kernel bootloader.img \
    -serial pty \
	-serial stdio \
    -display none

clean:
	rm -f *.o *.elf *.img
