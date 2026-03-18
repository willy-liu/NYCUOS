#!/usr/bin/env python3
import os
import queue
import struct
import threading
import time
import sys
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

try:
    import serial  # type: ignore[import-not-found]
    from serial.tools import list_ports  # type: ignore[import-not-found]
except ImportError:
    serial = None
    list_ports = None


class SerialBootloaderUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Kernel Serial Sender")
        self.root.geometry("980x680")
        self.root.minsize(860, 560)

        self.serial_port = None
        self.read_thread = None
        self.reader_stop = threading.Event()
        self.output_queue = queue.Queue()
        self.write_lock = threading.Lock()

        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="115200")
        self.image_path_var = tk.StringVar()
        self.status_var = tk.StringVar(value="Disconnected")

        self._build_ui()
        self.refresh_ports()
        self.root.after(50, self.flush_output_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _build_ui(self):
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(3, weight=1)

        conn_frame = ttk.LabelFrame(self.root, text="Connection")
        conn_frame.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 6))
        conn_frame.columnconfigure(1, weight=1)

        ttk.Label(conn_frame, text="Port:").grid(row=0, column=0, padx=(10, 6), pady=10, sticky="w")
        self.port_box = ttk.Combobox(conn_frame, textvariable=self.port_var, state="readonly")
        self.port_box.grid(row=0, column=1, padx=6, pady=10, sticky="ew")

        ttk.Button(conn_frame, text="Refresh", command=self.refresh_ports).grid(
            row=0, column=2, padx=6, pady=10
        )

        ttk.Label(conn_frame, text="Baud:").grid(row=0, column=3, padx=(12, 6), pady=10, sticky="w")
        self.baud_box = ttk.Combobox(
            conn_frame,
            textvariable=self.baud_var,
            values=["9600", "57600", "115200", "230400", "460800", "921600"],
            width=10,
        )
        self.baud_box.grid(row=0, column=4, padx=6, pady=10)

        ttk.Button(conn_frame, text="Connect", command=self.connect_serial).grid(
            row=0, column=5, padx=6, pady=10
        )
        ttk.Button(conn_frame, text="Disconnect", command=self.disconnect_serial).grid(
            row=0, column=6, padx=(6, 10), pady=10
        )

        image_frame = ttk.LabelFrame(self.root, text="Kernel Image")
        image_frame.grid(row=1, column=0, sticky="ew", padx=10, pady=6)
        image_frame.columnconfigure(1, weight=1)

        ttk.Label(image_frame, text=".img:").grid(row=0, column=0, padx=(10, 6), pady=10, sticky="w")
        ttk.Entry(image_frame, textvariable=self.image_path_var).grid(
            row=0, column=1, padx=6, pady=10, sticky="ew"
        )
        ttk.Button(image_frame, text="Browse", command=self.select_image).grid(
            row=0, column=2, padx=6, pady=10
        )
        ttk.Button(image_frame, text="Send Image", command=self.send_image).grid(
            row=0, column=3, padx=(6, 10), pady=10
        )

        action_frame = ttk.LabelFrame(self.root, text="Common Actions")
        action_frame.grid(row=2, column=0, sticky="ew", padx=10, pady=6)

        ttk.Button(action_frame, text="Send Enter", command=lambda: self.send_text("\r")).grid(
            row=0, column=0, padx=(10, 6), pady=10
        )
        ttk.Button(action_frame, text="Send Ctrl+C", command=lambda: self.send_text("\x03")).grid(
            row=0, column=1, padx=6, pady=10
        )
        ttk.Button(action_frame, text="Clear Terminal", command=self.clear_terminal).grid(
            row=0, column=2, padx=6, pady=10
        )

        terminal_frame = ttk.LabelFrame(self.root, text="UART Terminal")
        terminal_frame.grid(row=3, column=0, sticky="nsew", padx=10, pady=6)
        terminal_frame.columnconfigure(0, weight=1)
        terminal_frame.rowconfigure(0, weight=1)

        self.terminal = tk.Text(
            terminal_frame,
            wrap="word",
            state="disabled",
            background="#111111",
            foreground="#e6e6e6",
            insertbackground="#e6e6e6",
            padx=8,
            pady=8,
        )
        self.terminal.grid(row=0, column=0, sticky="nsew")
        self.terminal.bind("<KeyPress>", self.on_terminal_keypress)

        scrollbar = ttk.Scrollbar(terminal_frame, orient="vertical", command=self.terminal.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.terminal.configure(yscrollcommand=scrollbar.set)

        ttk.Label(
            self.root,
            text="Click terminal and type: every key is sent to UART immediately (Enter, Backspace, arrows, Ctrl+C).",
            anchor="w",
        ).grid(row=4, column=0, sticky="ew", padx=10, pady=(6, 4))

        status_bar = ttk.Label(self.root, textvariable=self.status_var, anchor="w")
        status_bar.grid(row=5, column=0, sticky="ew", padx=10, pady=(0, 8))

    def refresh_ports(self):
        ports = [p.device for p in list_ports.comports()]
        self.port_box["values"] = ports
        if ports and self.port_var.get() not in ports:
            self.port_var.set(ports[0])
        if not ports:
            self.port_var.set("")
        self.log("[INFO] Refreshed serial port list")

    def connect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.log("[WARN] Serial port already connected")
            return

        port = self.port_var.get().strip()
        if not port:
            messagebox.showerror("No Port", "Please choose a serial port.")
            return

        try:
            baud = int(self.baud_var.get().strip())
        except ValueError:
            messagebox.showerror("Invalid Baud", "Baud rate must be an integer.")
            return

        try:
            self.serial_port = serial.Serial(
                port=port,
                baudrate=baud,
                timeout=0.1,
                write_timeout=1,
            )
            self.reader_stop.clear()
            self.read_thread = threading.Thread(target=self.uart_reader_loop, daemon=True)
            self.read_thread.start()
            self.status_var.set(f"Connected: {port} @ {baud}")
            self.log(f"[INFO] Connected to {port} @ {baud}")
            self.terminal.focus_set()
        except Exception as exc:
            self.status_var.set("Disconnected")
            messagebox.showerror("Connection Failed", str(exc))

    def disconnect_serial(self):
        self.reader_stop.set()
        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception:
                pass
        self.serial_port = None
        self.status_var.set("Disconnected")
        self.log("[INFO] Disconnected")

    def uart_reader_loop(self):
        while not self.reader_stop.is_set():
            ser = self.serial_port
            if not ser or not ser.is_open:
                break
            try:
                data = ser.read(256)
                if data:
                    text = data.decode("latin1", errors="replace")
                    self.output_queue.put(text)
            except Exception as exc:
                self.output_queue.put(f"\n[ERROR] UART read failed: {exc}\n")
                break

    def flush_output_queue(self):
        pending = []
        try:
            while True:
                pending.append(self.output_queue.get_nowait())
        except queue.Empty:
            pass

        if pending:
            self.terminal.configure(state="normal")
            self.terminal.insert("end", "".join(pending))
            self.terminal.see("end")
            self.terminal.configure(state="disabled")

        self.root.after(50, self.flush_output_queue)

    def select_image(self):
        path = filedialog.askopenfilename(
            title="Select kernel image",
            filetypes=[("Image Files", "*.img"), ("All Files", "*.*")],
        )
        if path:
            self.image_path_var.set(path)
            self.log(f"[INFO] Selected image: {path}")

    def send_image(self):
        if not self.is_connected():
            messagebox.showerror("Not Connected", "Please connect to a serial port first.")
            return

        image_path = self.image_path_var.get().strip()
        if not image_path:
            messagebox.showerror("No Image", "Please select a .img file first.")
            return

        if not os.path.isfile(image_path):
            messagebox.showerror("File Not Found", image_path)
            return

        worker = threading.Thread(target=self._send_image_worker, args=(image_path,), daemon=True)
        worker.start()

    def _send_image_worker(self, image_path):
        try:
            with open(image_path, "rb") as file_obj:
                kernel = file_obj.read()

            self.log(f"[INFO] Sending image: {os.path.basename(image_path)}")
            self.send_kernel_data(kernel)
            self.log("[INFO] Image transfer complete")
        except Exception as exc:
            self.log(f"[ERROR] Send image failed: {exc}")

    def send_kernel_data(self, kernel):
        size = len(kernel)
        self.log(f"[INFO] Sending kernel size: {size} bytes")

        with self.write_lock:
            self.serial_port.write(struct.pack("<I", size))
            self.serial_port.flush()
            time.sleep(0.01)

            chunk_size = 8
            for start in range(0, size, chunk_size):
                chunk = kernel[start : start + chunk_size]
                self.serial_port.write(chunk)
                self.serial_port.flush()
                time.sleep(0.001)

    def on_terminal_keypress(self, event):
        if not self.is_connected():
            return "break"

        key_map = {
            "Return": "\r",
            "KP_Enter": "\r",
            "BackSpace": "\x08",
            "Delete": "\x7f",
            "Tab": "\t",
            "Escape": "\x1b",
            "Up": "\x1b[A",
            "Down": "\x1b[B",
            "Right": "\x1b[C",
            "Left": "\x1b[D",
            "Home": "\x1b[H",
            "End": "\x1b[F",
        }

        if event.keysym in key_map:
            self.send_text(key_map[event.keysym])
            return "break"

        if event.keysym == "c" and (event.state & 0x4):
            self.send_text("\x03")
            return "break"

        if event.char:
            self.send_text(event.char)

        return "break"

    def send_text(self, text):
        if not self.is_connected():
            self.log("[WARN] Cannot send: serial port not connected")
            return

        try:
            with self.write_lock:
                self.serial_port.write(text.encode("latin1", errors="replace"))
                self.serial_port.flush()
        except Exception as exc:
            self.log(f"[ERROR] UART write failed: {exc}")

    def clear_terminal(self):
        self.terminal.configure(state="normal")
        self.terminal.delete("1.0", "end")
        self.terminal.configure(state="disabled")

    def is_connected(self):
        return self.serial_port is not None and self.serial_port.is_open

    def log(self, text):
        self.output_queue.put(text + "\n")

    def on_close(self):
        self.disconnect_serial()
        self.root.destroy()


def main():
    if serial is None or list_ports is None:
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror(
            "Missing Dependency",
            "pyserial is required. Install with: pip install pyserial",
        )
        root.destroy()
        sys.exit(1)

    root = tk.Tk()
    app = SerialBootloaderUI(root)
    app.log("[INFO] UI ready")
    root.mainloop()


if __name__ == "__main__":
    main()
