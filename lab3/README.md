# NYCU OS Lab3 README

This README summarizes the implementation status for:
- Basic Exercise 1: Exception (30%)
- Basic Exercise 2: Interrupt (10%)
- Basic Exercise 3: RPi3 Peripheral Interrupt (30%)
- Advanced Exercise 1: Timer Multiplexing (20%)

It also documents how to switch between `uart` and `mini_uart` in the current codebase.

## 1. Build and Run

From `lab3/`:

```bash
make
make run
```

This builds:
- `kernel8.img`
- `user_program/user.img`
- `initramfs.cpio` (packed from `rootfs/` and `user_program/user.img`)

## 2. Exercise 1 - Exception

### 2.1 EL2 -> EL1

Implemented in `boot.S`:
- Read `CurrentEL`
- If booted in EL2, call `from_el2_to_el1`
- Configure:
  - `hcr_el2` (EL1 runs AArch64)
  - `spsr_el2` (target PSTATE)
  - `elr_el2` (return address)
- Execute `eret` to enter EL1

### 2.2 EL1 -> EL0

Implemented in `exception.S` + `main.c`:
- `enter_user_program(entry, user_sp_top)` sets:
  - `sp_el0`
  - `elr_el1`
  - `spsr_el1`
- Execute `eret` to user mode
- Shell command `run [path]` loads user image from initramfs and jumps to it

### 2.3 EL0 -> EL1 Exception Vector + Handler

Implemented in `exception.S`:
- 0x800-aligned vector table (`.align 11`)
- Synchronous exceptions go to `exception_handler`
- IRQs go to `irq_handler`
- `set_exception_vector_table` writes `vbar_el1`

Implemented in `main.c`:
- `exception_entry` prints:
  - `spsr_el1`
  - `elr_el1`
  - `esr_el1`
- For SVC (`EC == 0x15`), prints user `x0` from saved context

### 2.4 Context Save/Restore

Implemented in `exception.S`:
- `save_all` saves x0-x30
- `load_all` restores x0-x30
- Handler flow:
  - `save_all`
  - call C handler (`exception_entry` / `irq_entry`)
  - `load_all`
  - `eret`

## 3. Exercise 2 - Core Timer Interrupt

Implemented in `exception.S` + `main.c`:

- `core_timer_enable` in `exception.S`
  - enable `cntp_ctl_el0`
  - program first timeout with `cntp_tval_el0`
  - unmask local timer IRQ via `CORE0_TIMER_IRQ_CTRL`

- `irq_entry` in `main.c`
  - checks core timer interrupt source (`CORE0_IRQ_SOURCE`, bit1)
  - dispatches to `core_timer_handler`

- `core_timer_handler` in `main.c`
  - receives timer interrupt and dispatches timer events
  - reprograms hardware timer for the next earliest deadline

Note:
- If IRQ is enabled in EL1 (`msr daifclr, #2`), timer can print while kernel shell is running.

## 4. Exercise 3 - Peripheral UART Interrupt (Async IO)

### 4.1 IRQ Source Dispatch

Implemented in `main.c`:
- `irq_entry` checks sources in order:
  1. core timer IRQ
  2. UART IRQ (`uart_irq_pending()`)

`uart_irq_pending()` is implemented in both drivers:
- `uart.c` (PL011 path)
- `mini_uart.c` (AUX mini UART path)

### 4.2 Async Read/Write Model

Implemented in both `uart.c` and `mini_uart.c`:
- RX ring buffer
- TX ring buffer
- IRQ-safe push/pop with local IRQ mask (`daif` save/restore)

APIs:
- `uart_enable_interrupts()`
- `uart_irq_handler()`
- `uart_async_read()`
- `uart_async_write()`

Shell read path in `main.c`:
- `shell_getc_async()`
- if no RX data, executes `wfi` (Wait For Interrupt), not busy polling

## 5. Advanced Exercise 1 - Timer Multiplexing (`setTimeout`)

Implemented in `main.c` using one-shot core timer:

- Software timer queue:
  - `timer_events[]` stores `(deadline_tick, callback, data, order)`
  - keeps stable order for same-deadline events with `order`

- Timer API (internal):
  - `timer_add_event(callback, data, after_seconds)`
  - computes absolute timeout tick from `cntpct_el0` and `cntfrq_el0`
  - reprograms core timer to earliest pending event only

- IRQ dispatch:
  - `core_timer_handler()` executes all expired callbacks
  - after handling, it reprograms the next soonest timeout
  - if queue is empty, timer is disabled to avoid redundant interrupts

- Shell command:
  - `setTimeout <message> <seconds>`
  - non-blocking: returns immediately
  - supports multiple concurrent timeouts
  - callback prints:
    - current time (`now`)
    - command execution time (`cmd`)
    - message text

Example:

```text
> setTimeout hello 3
setTimeout scheduled: hello after 3s
> setTimeout world 1
setTimeout scheduled: world after 1s
...
[setTimeout] now=...s cmd=...s msg=world
[setTimeout] now=...s cmd=...s msg=hello
```

## 6. User Program Loading from Initramfs

Implemented in `main.c` and Makefiles:
- `run [path]` default path is `user.img`
- fallback lookup supports `./user.img`
- `user_program/Makefile` builds `user_program/user.img`
- top-level `Makefile` packs `rootfs/` + `user_program/user.img` into `initramfs.cpio`

## 7. UART / mini_uart Switching Guide

Current state in this workspace is **mini UART**.

To switch driver implementation, update these files:
- `main.c`
- `fdt.c`
- `cpio.c`
- `Makefile`

### 6.1 Switch to mini UART

1. In `main.c`, `fdt.c`, `cpio.c`, include:

```c
#include "mini_uart.h"
```

2. In `Makefile`, ensure `KERNEL_OBJS` includes `mini_uart.o` (and not `uart.o` if you want a clean single-driver build).

### 6.2 Switch to PL011 UART

1. In `main.c`, `fdt.c`, `cpio.c`, include:

```c
#include "uart.h"
```

2. In `Makefile`, ensure `KERNEL_OBJS` includes `uart.o` (and not `mini_uart.o` for single-driver build).

Note:
- `mini_uart.h` provides API-compatible macros so call sites can stay as `uart_*`.

## 8. Quick Validation Checklist

- Build success:
  - `make`
- Initramfs contains user image:
  - boot shell command: `ls` should show `user.img` (or `./user.img`)
- Exception path:
  - boot shell command: `run user.img`
  - observe `[exception] ...` and user `x0` progression
- Async RX path:
  - shell still responsive
  - no busy polling in shell read path (`wfi` used while waiting)
- Timer multiplexing:
  - boot shell command: `setTimeout test 2`
  - verify callback is printed after about 2 seconds
  - issue multiple commands and verify ordering by timeout/issue time

## 9. Key Files

- Boot and EL switch: `boot.S`
- Exception vector and context save: `exception.S`
- Shell and dispatch logic: `main.c`
- PL011 async driver: `uart.c`, `uart.h`
- mini UART async driver: `mini_uart.c`, `mini_uart.h`
- Initramfs parser: `cpio.c`, `cpio.h`
- DTB parser: `fdt.c`, `fdt.h`
- Kernel build/package: `Makefile`
- User image build: `user_program/Makefile`, `user_program/user.S`
