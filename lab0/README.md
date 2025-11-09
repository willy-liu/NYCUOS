# Lab 0: Environment Setup

## Introduction

In Lab 0, you need to prepare the environment for future development.
You should install the target toolchain, and use them to build a
bootable image for rpi3.

## Goals of this lab

- Set up the development environment.
- Understand what's cross-platform development.
- Test your rpi3.

> [!IMPORTANT]
> This lab is an introductory lab. It won't be part of your final grade,
> but you still need to finish all `todo` parts, or you'll be in trouble
> in the next lab.

## Cross-Platform Development

### Cross Compiler

Rpi3 uses ARM Cortex-A53 CPU. To compile your source code to 64-bit ARM
machine code, you need a cross compiler if you develop on a non-ARM64
environment.

> [!TIP] Todo
>
> Install a cross compiler on your host computer.

> [!Note]用Brew 安裝 aarch64-unknown-linux-gnu
> ```Bash
> brew tap messense/macos-cross-toolchains # 加入來源倉庫
> brew install aarch64-unknown-linux-gnu
> ```

### Linker

You might not notice the existence of linkers before. It's because the
compiler uses the default linker script for you. (`ld --verbose` to
check the content) In bare-metal programming, you should set the memory
layout yourself.

This is an incomplete linker script for you. You should extend it in the
following lab.

``` none
SECTIONS
{
  . = 0x80000;
  .text : { *(.text) }
}
```

### QEMU

In cross-platform development, it's easier to validate your code on an
emulator first. You can use QEMU to test your code first before
validating them on a real rpi3.

> [!WARNING]
> Although QEMU provides a machine option for rpi3, it doesn't behave
> the same as a real rpi3. You should validate your code on your rpi3,
> too.

> [!TIP] Todo
>
> Install `qemu-system-aarch64`.

:::spoiler Implementation

用 Brew 安裝qemu
```Bash
brew install qemu
```

:::
## From Source Code to Kernel Image

You have the basic knowledge of the toolchain for cross-platform
development. Now, it’s time to practice them.

### From Source Code to Object Files

Source code is converted to object files by a cross compiler. After
saving the following assembly as `a.S`, you can convert it to an object
file by `aarch64-linux-gnu-gcc -c a.S`. Or if you would like to, you can
also try llvm’s linker
`clang -mcpu=cortex-a53 --target=aarch64-rpi3-elf -c a.S`, especially if
you are trying to develop on macOS.

``` c
.section ".text"
_start:
  wfe
  b _start
```

### From Object Files to ELF

A linker links object files to an ELF file. An ELF file can be loaded
and executed by program loaders. Program loaders are usually provided by
the operating system in a regular development environment. In bare-metal
programming, ELF can be loaded by some bootloaders.

To convert the object file from the previous step to an ELF file, you
can save the provided linker script as `linker.ld`, and run the
following command.

``` none
# On GNU LD
aarch64-linux-gnu-ld -T linker.ld -o kernel8.elf a.o
# On LLVM
ld.lld -m aarch64elf -T linker.ld -o kernel8.elf a.o
```

### From ELF to Kernel Image

Rpi3's bootloader can't load ELF files. Hence, you need to convert the
ELF file to a raw binary image. You can use `objcopy` to convert ELF
files to raw binary.

``` none
aarch64-linux-gnu-objcopy -O binary kernel8.elf kernel8.img
# Or
llvm-objcopy --output-target=aarch64-rpi3-elf -O binary kernel8.elf kernle8.img
```

### Check on QEMU

After building, you can use QEMU to see the dumped assembly.

``` none
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm
```

> [!TIP] Todo
>
> Build your first kernel image, and check it on QEMU.

<details open>
    <summary><b>implementation</b></summary>

---
- 建立`a.S`
副檔名 `.S` = **會經過 C 前處理器的組語**（大寫 S 表示可含巨集、include 等）
```asm
.section ".text"       // 放到程式的 .text 區段（可執行機器碼）
.global _start         // 宣告 _start 為全域符號，bootloader 從這裡跳入
_start:
    wfe                // Wait For Event：讓 CPU 休眠，降低空迴圈耗能
    b _start           // 無限迴圈，跳回 _start
```
樹莓派開機後會把 kernel 載入 0x80000 並從 _start 執行
這程式就是「一個什麼都不做、待機的最小 kernel」   

---
- 建立`linker.ld`
副檔名 .ld = linker script，決定程式放在哪個位址
```ld
SECTIONS {
  . = 0x80000;
  .text : { *(.text) }
}
```
沒有 linker script，linker 會把程式放到 PC/Linux 預設位址 → 不能在裸機跑
    
---
- 編譯、連結、轉成裸機映像
| 副檔名    | 意義                                 |
| ------ | ---------------------------------- |
| `.S`   | ARM 組語原始碼                          |
| `.o`   | **Object File**：編譯後但未連結的機器碼        |
| `.elf` | 可執行檔 + 調試資訊（bootloader 無法直接載入）     |
| `.img` | **純裸機二進位影像**，樹莓派 bootloader 可以直接載入 |

```bash
aarch64-unknown-linux-gnu-gcc -c a.S -o a.o
```
- `-c`：只生成 .o，不連結
- 用 cross compiler 產生 ARM64 指令，不是 macOS 指令

```bash
aarch64-unknown-linux-gnu-ld -T linker.ld -o kernel8.elf a.o
```
- `ld`：把 `.o` 連結成 ELF 格式可執行檔
- `-T linker.ld`：使用我們自己指定的 memory layout

```bash
aarch64-unknown-linux-gnu-objcopy -O binary kernel8.elf kernel8.img
```
- ELF 包含符號表，樹莓派 bootloader 不吃
- 這一步把 ELF 剝皮 → 純機器碼 (.img)
    
---
- 建立 Makefile（簡化以後方便跑）
```make
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
```
---
- 在 QEMU 上測試 kernel
```bash
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm
```
    
- `-M raspi3b`：使用樹莓派 3 模擬機
- `-kernel kernel8.img`：直接載入我們的裸機 kernel
- `-d in_asm`：輸出 CPU 正在執行的指令（方便 debug）
輸出：
```
----------------
IN: 
0x00000000:  580000c0  ldr      x0, #0x18
0x00000004:  aa1f03e1  mov      x1, xzr
...

----------------
IN: 
0x00080000:  d503205f  wfe      
0x00080004:  17ffffff  b        #0x80000

----------------
IN: 
0x00000300:  d2801b05  mov      x5, #0xd8
0x00000304:  d53800a6  mrs      x6, mpidr_el1
0x00000308:  924004c6  and      x6, x6, #3
...
    
```
輸出說明：
(1) 0x00000000 附近：啟動前導程式
0x00000000: ldr x0, #...
0x00000004: mov x1, xzr
...
是 Raspberry Pi 固件（bootcode.bin）或 QEMU boot stub 的執行過程。

負責設定基本暫存器、找到 kernel8.img 的載入位置、跳轉到 0x80000 執行 kernel。

(2) 0x00080000：kernel _start 本體
0x00080000: wfe
0x00080004: b #0x80000

代表：
- kernel 正確被載入
- linker.ld 設定 . = 0x80000 是正確的
- CPU 正在執行你寫的程式

(3) 0x00000300 附近：多核心啟動等待邏輯
0x00000300: mov x5, #0xd8
0x00000304: mrs x6, mpidr_el1
0x00000308: and x6, x6, #3
0x0000030c: wfe
0x00000310: ldr x4, [x5, x6, lsl #3]
0x00000314: cbz x4, #0x30c

樹莓派 3 有 4 顆 Cortex-A53，開機時只有 核心 0 會開始執行。
其他核心會進入：wfe → 等待 → 檢查能不能醒 → 沒被叫起來就繼續睡

</details>

## Deploy to REAL Rpi3

### Flash Bootable Image to SD Card

To prepare a bootable image for rpi3, you have to prepare at least the
following stuff.

- An FAT16/32 partition contains
  - Firmware for GPU.
  - Kernel image.(kernel8.img)

There are two ways to do it.

1\.  
We already prepared a [bootable
image](https://github.com/nycu-caslab/OSC2024/raw/main/supplement/nycuos.img).

You can use the following command to flash it to your SD card.

``` none
dd if=nycuos.img of=/dev/sdb
```

> [!WARNING]
> /dev/sdb should be replaced by your SD card device. You can check it
> by <span class="title-ref">lsblk</span>

It's already partition and contains a FAT32 filesystem with firmware
inside. You can mount the partition to check.

2\.  
Partition the disk and prepare the booting firmware yourself. You can
download the firmware from
<https://github.com/raspberrypi/firmware/tree/master/boot>

bootcode.bin, fixup.dat and start.elf are essentials. More information
about pi3's booting could be checked on the official website
<https://www.raspberrypi.org/documentation/configuration/boot_folder.md>
<https://www.raspberrypi.org/documentation/hardware/raspberrypi/bootmodes/README.md>

Finally, put the firmware and your kernel image into the FAT partition.

> [!IMPORTANT]
> Besides using `mkfs.fat -F 32` to create a FAT32 filesystem, you
> should also set the partition type to FAT.

> [!TIP] Todo
>
> Use either one of the methods to set up your SD card.


<details open>
    <summary>Implementation</summary>
    
- 燒錄方法1:
    使用Raspberry Pi Imager直接選擇nycuos.img
    
- 燒錄方法2:
    1. 檢查 SD 卡位置`diskutil list`
    2. 卸載 SD 卡`diskutil unmountDisk /dev/disk{X}`
    3. 燒錄`sudo dd if=nycuos.img of=/dev/disk{X} bs=4m`

燒錄完畢後將SD卡插回樹莓派後開機，並接上UART

- 查看UART位置：`ls /dev/tty.usb*`
- 連上UART Console： `screen /dev/tty.usbserial-XXXXXXXX 115200`
- 鍵盤輸入東西也有顯示輸入的內容就是正確的（不用按enter）

p.s. 手動中斷screen連線方法：
- 查看pid：`lsof | grep usbserial`
- kill掉process：`kill -9 {pid}`
    
</details>

### Interact with Rpi3

In our provided bootable image, it contains a kernel image that can
echoes what you type through UART. You can use it to test if your Lab
kits function well.

1\. If you use method 2 to set up your bootable image, you should
download
[kernel8.img](https://github.com/nycu-caslab/OSC2024/raw/main/supplement/kernel8.img)
, and put it into your boot partition. It's identical to the one in the
provided bootable image.

2.  Plug in the UART to USB converter to your host machine, and open it
    through a serial console such as screen or putty with the correct
    baud rate.
3.  Connect TX, RX, GND to the corresponding pins on rpi3, and turn on
    your rpi3. You can follow the picture below to set up your UART.
4.  After your rpi3 powers on, you can type some letters, and your
    serial console should print what you just typed.

``` none
screen /dev/ttyUSB0 115200
```

## Debugging

### Debug on QEMU

Debugging on QEMU is a relatively easier way to validate your code. QEMU
could dump memory, registers, and expose them to a debugger. You can use
the following command waiting for gdb connection.

``` none
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -S -s
```

Then you can use the following command in gdb to load debugging
information and connect to QEMU.

``` none
file kernel8.elf
target remote :1234
```

> [!IMPORTANT]
> Your gdb should also be cross-platform gdb.

### Debug on Real Rpi3

You could either use print log or JTAG to debug on a real rpi3. We don't
provide JTAG in this course, you can try it if you have one.
<https://metebalci.com/blog/bare-metal-raspberry-pi-3b-jtag/>
