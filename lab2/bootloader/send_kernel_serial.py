#!/usr/bin/env python3
import serial
import struct
import threading
import time
import sys
import os
import termios
import tty

def uart_reader(ser):
    """Background thread: read bytes from UART and print them"""
    try:
        while True:
            data = ser.read(1)  # blocking read of 1 byte
            if not data:
                continue
            try:
                ch = data.decode("latin1", errors="replace")
            except Exception:
                ch = data.decode(errors="replace")
            sys.stdout.write(ch)
            sys.stdout.flush()
    except Exception:
        pass

def send_kernel_data(ser, kernel):
    size = len(kernel)
    sys.stdout.write(f"[INFO] Sending Kernel size: {size} bytes\n")
    sys.stdout.flush()

    # 1. Send kernel size (4-byte LE)
    ser.write(struct.pack("<I", size))
    ser.flush()
    time.sleep(0.01)  # 讓 RPi 有時間處理 size，避免和 body 黏在一起
    sys.stdout.write("[INFO] Sent kernel size\n")
    sys.stdout.flush()

    # 2. Send kernel body in chunks
    CHUNK = 8
    sent = 0
    for i in range(0, size, CHUNK):
        chunk = kernel[i : i + CHUNK]
        ser.write(chunk)
        ser.flush()
        sent += len(chunk)
        # 可以印進度，但避免干擾畫面，這裡就不印百分比了
        time.sleep(0.001)

    sys.stdout.write("[INFO] Kernel transfer complete!\n")
    sys.stdout.write("[INFO] Switched to interactive mode (Ctrl+C to exit).\n")
    sys.stdout.flush()

def interactive_raw_console(ser):
    """
    像 screen 一樣：
    - 鍵盤輸入 -> 直接寫到 UART
    - UART 輸出 -> reader thread 自動印出
    - Ctrl+C -> 離開
    """
    import termios
    import tty

    fd = sys.stdin.fileno()
    old_attr = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)   # ★ 正確：setraw 在 tty 模組
        while True:
            ch = sys.stdin.read(1)
            if ch == '\x03':  # Ctrl-C
                raise KeyboardInterrupt
            ser.write(ch.encode("latin1", errors="replace"))
            ser.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_attr)


def interactive_send(tty_path, kernel_path):
    # Open serial port
    ser = serial.Serial(
        port=tty_path,
        baudrate=115200,
        timeout=None,      # blocking read in reader thread
        write_timeout=1
    )

    sys.stdout.write(f"[INFO] Opened serial port: {tty_path}\n")
    sys.stdout.flush()

    # Read kernel
    with open(kernel_path, "rb") as f:
        kernel = f.read()

    # Start UART reader thread
    reader_thread = threading.Thread(target=uart_reader, args=(ser,), daemon=True)
    reader_thread.start()

    sys.stdout.write("[INFO] Interactive terminal:\n")
    sys.stdout.write("  Type 'send' -> send kernel, then enter screen-like mode\n")
    sys.stdout.flush()

    try:
        while True:
            sys.stdout.write("> ")
            sys.stdout.flush()
            line = sys.stdin.readline()
            if not line:  # EOF
                break
            cmd = line.strip().lower()

            if cmd == "send":
                # 傳 kernel
                send_kernel_data(ser, kernel)
                # 傳完直接進入 raw 模式（像 screen）
                try:
                    interactive_raw_console(ser)
                except KeyboardInterrupt:
                    sys.stdout.write("\n[INFO] Exit interactive mode.\n")
                    sys.stdout.flush()
                break  # 結束整個程式
            elif cmd == "":
                continue
            else:
                sys.stdout.write("[INFO] Unknown command. Type 'send'.\n")
                sys.stdout.flush()
    finally:
        try:
            ser.close()
        except Exception:
            pass

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.stdout.write("Usage: python3 send_kernel_serial.py <tty_path> <kernel8.img>\n")
        sys.stdout.write("Example: python3 send_kernel_serial.py /dev/tty.usbserial-B00467RB kernel8.img\n")
        sys.stdout.flush()
        sys.exit(1)

    tty = sys.argv[1]
    kernel = sys.argv[2]

    interactive_send(tty, kernel)
