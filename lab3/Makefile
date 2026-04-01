# =======================
# Raspberry Pi 3 Bare-metal Makefile
# =======================

CROSS   = aarch64-linux-gnu
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy

CPIO    = cpio

# =======================
# Compiler / Linker flags
# =======================
CFLAGS  = -Wall -O2 -ffreestanding -nostdlib

LDFLAGS = -T linker.ld

# =======================
# Objects (split by role)
# =======================
BOOT_OBJS   = boot.o exception.o
KERNEL_OBJS = main.o mini_uart.o gpio.o cpio.o simple_alloc.o fdt.o mailbox.o reboot.o

OBJS = $(BOOT_OBJS) $(KERNEL_OBJS)

ROOTFS_FILES = $(shell find rootfs -type f)
INITRAMFS_STAGING = .initramfs_build

# =======================
# Build rules
# =======================
all: kernel8.img initramfs.cpio

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

user_program/user.img:
	$(MAKE) -C user_program

initramfs.cpio: $(ROOTFS_FILES) user_program/user.img
	rm -rf $(INITRAMFS_STAGING)
	mkdir -p $(INITRAMFS_STAGING)
	cp -R rootfs/. $(INITRAMFS_STAGING)/
	cp user_program/user.img $(INITRAMFS_STAGING)/user.img
	cd $(INITRAMFS_STAGING) && find . | $(CPIO) -o -H newc > ../initramfs.cpio

# =======================
# Run / Clean
# =======================
run: kernel8.img initramfs.cpio
	qemu-system-aarch64 \
		-M raspi3b \
		-kernel kernel8.img \
		-serial null \
		-serial stdio \
		-display none \
		-initrd initramfs.cpio \
		-dtb bcm2710-rpi-3-b-plus.dtb

clean:
	rm -f *.o *.elf *.img initramfs.cpio
	rm -rf $(INITRAMFS_STAGING)
	$(MAKE) -C user_program clean
