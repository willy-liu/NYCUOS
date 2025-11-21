# Lab 2: UART Bootloader

Course Website: https://nycu-caslab.github.io/OSC2024/labs/lab2.html#

## Basic Exercise 1 - UART Bootloader - 30%

To avoid physically moving the SD card between the host and Raspberry Pi for every kernel update, we can implement a UART Bootloader. This allows loading the kernel image directly via UART during debugging.

### Python Script Usage

The provided Python script `send_kernel_serial.py` simplifies the process. It provides a command-line interface to send the kernel and interact with the device.

**Usage:**
```sh
python3 send_kernel_serial.py <TTY_PATH> <IMAGE_PATH>
```

**Commands:**
- `send`: Sends the kernel image to the bootloader. Once the transfer is complete, it automatically switches to an interactive "screen-like" mode where you can see the UART output and send input.

**Example:**
```text
> send
[INFO] Sending Kernel size: ...
...
[INFO] Kernel transfer complete!
[INFO] Switched to interactive mode (Ctrl+C to exit).
bootloader: kernel received successfully!
```

## Directory Structure

```
bootloader/
├── boot.S                  # Assembly startup code
├── linker.ld               # Linker script (relocated to 0x60000)
├── main.c                  # Bootloader logic
├── Makefile                # Build script
└── send_kernel_serial.py   # Python script to send kernel image from host
```

> **Note**: This lab reuses GPIO and Mini UART drivers from the [`uart/`](../../uart) directory.

## Implementation Details

1.  **Config Kernel Loading Setting**:
    -   **Goal**: Load the actual kernel image at `0x80000` without overlapping with the bootloader.
    -   **Implementation**: The bootloader is relocated to `0x60000` in `linker.ld`. On the SD card, `config.txt` specifies `kernel_address=0x60000` and `kernel=bootloader.img`.

2.  **UART Protocol**:
    -   **Goal**: Receive raw binary data through UART.
    -   **Implementation**: The bootloader waits for the kernel size (4 bytes), then reads the kernel content byte-by-byte into memory at `0x80000`, and finally jumps to it.

    > **Alternative Implementation**:
    > Instead of jumping to the kernel directly in `main.c`, you can return from `main` and handle the jump in `boot.S`.
    >
    > In `boot.S`:
    > ```assembly
    > bss_done:
    >     bl main
    >     mov x0, #0x80000
    >     br x0
    > ```

3.  **Python Sender**:
    -   **Goal**: Send the kernel image from the host.
    -   **Implementation**: `send_kernel_serial.py` opens the serial port, sends the size, and then streams the kernel binary.

## Prerequisites

- **Cross Compiler**: `aarch64-linux-gnu-gcc`
- **QEMU**: `qemu-system-aarch64`
- **Python 3**: with `pyserial` installed

## Build

To compile the project and generate the bootloader image (`bootloader.img`):

```sh
make
```

## Usage

### 1. Real Raspberry Pi

To run on real hardware, you need to configure the bootloader on the SD card and use the Python script to send the kernel.

**Step 1: Configure SD Card**
Modify (or create) `config.txt` on the boot partition of your SD card:
```ini
kernel_address=0x60000
kernel=bootloader.img
arm_64bit=1
```

**Step 2: Connect and Send Kernel**
Connect the Raspberry Pi to your computer via USB-TTL. Run the Python script:

```sh
python3 send_kernel_serial.py {TTY_PATH} {IMAGE_PATH}
# Example:
python3 send_kernel_serial.py /dev/tty.usbserial-B00467RB ../../lab1/kernel8.img
```

**Expected Output:**

```text
[INFO] Opened serial port: /dev/tty.usbserial-B00467RB
[INFO] Interactive terminal:
  Type 'send' -> send kernel, then enter screen-like mode
> bootloader: UART initialized successfully!
bootloader: waiting for loading kernel...
bootloader: waiting for kernel size...
send
[INFO] Sending Kernel size: 3312 bytes
bootloader: kernel siz[INFO] Sent kernel size
e = 0x00000CF0
bootloader: receiving kernel...
[INFO] Kernel transfer complete!
[INFO] Switched to interactive mode (Ctrl+C to exit).
bootloader: kernel received successfully!
bootloader: jumping to kernel...
```

### 2. QEMU

You can test the bootloader using QEMU with a pseudo TTY.

**Terminal 1: Start QEMU**
Run `make run`. This will start QEMU and redirect the serial output to a pseudo TTY.

```sh
❯ make run
qemu-system-aarch64 \
    -M raspi3b \
    -kernel bootloader.elf \
    -serial pty \
        -serial stdio \
    -display none
char device redirected to /dev/ttys032 (label serial0)
```
*(Note the device path, e.g., `/dev/ttys032`)*

**Terminal 2: Send Kernel**
In a separate terminal, use the Python script to send your kernel image to the device path shown in Terminal 1.

```sh
❯ python3 send_kernel_serial.py /dev/ttys032 ../../lab1/kernel8.img

[INFO] Opened serial port: /dev/ttys032
[INFO] Interactive terminal:
  Type 'send' -> send kernel, then enter screen-like mode
> send
[INFO] Sending Kernel size: 3312 bytes
bootloader: kernel size = 0x00000CF0
bootloader: receiving kernel...
[INFO] Sent kernel size
bootloader: kernel received successfully!
bootloader: jumping to kernel...
```

> **Note**: After sending, if the loaded kernel uses **Mini UART** (like `lab1/kernel8.img`), the output will appear in **Terminal 2** (the Python script). If it uses **PL011 UART**, it might appear in Terminal 1 depending on QEMU configuration.

## Clean

To remove build artifacts:

```sh
make clean
```
