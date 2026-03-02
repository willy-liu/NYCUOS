# =======================
# Raspberry Pi 3 Bare-metal Makefile
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
# Objects (split by role)
# =======================
BOOT_OBJS   = boot.o
KERNEL_OBJS = main.o uart.o gpio.o cpio.o simple_alloc.o fdt.o

OBJS = $(BOOT_OBJS) $(KERNEL_OBJS)

# =======================
# Build rules
# =======================
all: kernel8.img

# Assembly
boot.o: boot.S
	$(CC) $(CFLAGS) -c $< -o $@

# Generic C rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link
kernel8.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BOOT_OBJS) $(KERNEL_OBJS)

# Binary image
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
		-initrd initramfs.cpio \
		-dtb bcm2710-rpi-3-b-plus.dtb

clean:
	rm -f *.o *.elf *.img
