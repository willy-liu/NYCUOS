CROSS=aarch64-unknown-linux-gnu
CC=$(CROSS)-gcc
LD=$(CROSS)-ld
OBJCOPY=$(CROSS)-objcopy

all: kernel8.img

a.o: a.S
	$(CC) -c a.S -o a.o

kernel8.elf: a.o linker.ld
	$(LD) -T linker.ld -o kernel8.elf a.o

kernel8.img: kernel8.elf
	$(OBJCOPY) -O binary kernel8.elf kernel8.img

clean:
	rm -f *.o *.elf *.img

run: kernel8.img
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -S -s
