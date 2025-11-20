# Lab 1: Hello World

Course Website: https://nycu-caslab.github.io/OSC2024/labs/lab2.html#

This project implements a simple shell on a bare-metal Raspberry Pi 3. It demonstrates basic initialization, Mini UART communication, and Mailbox usage.

## Directory Structure

```
lab1/
├── boot.S          # Assembly startup code (sets up stack, clears BSS)
├── linker.ld       # Linker script (memory layout)
├── mailbox.c/h     # Mailbox communication with VideoCore IV
├── main.c          # Main entry point and shell implementation
├── Makefile        # Build script
└── reboot.c/h      # Reboot functionality
```

> **Note**: This lab reuses GPIO and Mini UART drivers from the [`uart/`](../uart) directory.

## Implementation Details

This lab fulfills the following requirements:

1.  **Basic Initialization**:
    -   **Goal**: Initialize the stack pointer and clear the BSS segment to ensure a clean C runtime environment.
    -   **Implementation**: Implemented in `boot.S`. It sets `sp` to `_stack_top` and zeroes out memory between `_bss_start` and `_bss_end` before jumping to `main`.

2.  **Mini UART**:
    -   **Goal**: Enable serial communication with the host.
    -   **Implementation**: Reused the driver from the `../uart` directory. It configures GPIO pins 14/15 to ALT5 and initializes the AUX peripheral.

3.  **Simple Shell**:
    -   **Goal**: Provide a command-line interface.
    -   **Implementation**: Implemented in `main.c`. It reads characters from UART, buffers them, and parses commands like `help`, `hello`, `info`, and `reboot`.

4.  **Mailbox**:
    -   **Goal**: Query hardware information from the GPU (VideoCore IV).
    -   **Implementation**: Implemented in `mailbox.c`. It constructs a message with tags (e.g., `GET_BOARD_REVISION`, `GET_ARM_MEMORY`) and writes the address to the Mailbox register.

5.  **Reboot**:
    -   **Goal**: Reset the Raspberry Pi 3.
    -   **Implementation**: Implemented in `reboot.c` using the Watchdog Timer (PM_RSTC, PM_WDOG) to trigger a system reset.



## Prerequisites

To build and run this project, you need the following tools installed:

- **Cross Compiler**: `aarch64-linux-gnu-gcc`
- **QEMU**: `qemu-system-aarch64`

## Build

To compile the project and generate the kernel image (`kernel8.img`), run:

```sh
make
```

## Run

To run the kernel in QEMU:

```sh
make run
```

This command executes:
```sh
qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial null -serial stdio -display none
```

To exit QEMU, press `Ctrl+A` then `X`.

## Usage

Once the system is running, you will see a shell prompt (`# `). The following commands are supported:

| Command  | Description |
|----------|-------------|
| `help`   | Print all available commands |
| `hello`  | Print "Hello World!" |
| `info`   | Print system information (Board Revision, ARM Memory) via Mailbox |
| `reboot` | Reboot the device |

## Clean

To remove build artifacts (`*.o`, `*.elf`, `*.img`):

```sh
make clean
```
