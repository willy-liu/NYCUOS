CROSS   = aarch64-linux-gnu
CC      = $(CROSS)-gcc
LD      = $(CROSS)-ld
OBJCOPY = $(CROSS)-objcopy

CFLAGS  = -Wall -O2 -ffreestanding -nostdlib
LDFLAGS = -Ttext=0x0

all: user.img

user.o: user.S
	$(CC) $(CFLAGS) -c $< -o $@

user.elf: user.o
	$(LD) $(LDFLAGS) -o $@ $<

user.img: user.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f *.o *.elf *.img
