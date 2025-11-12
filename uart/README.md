# Raspberry Pi 3 UART 範例（mini UART 與 PL011 UART）

此資料夾示範在 **Raspberry Pi 3** 上使用兩種 UART 方式的簡易範例：  
- **mini UART**（屬於 Auxiliary peripherals，功能較簡化）  
- **PL011 full UART**（完整功能的主 UART）  

兩者皆可在 QEMU 或實機環境下運作，  
並附有 Makefile 方便快速編譯與執行。

---

## 🚀 使用方式

### 🧩 編譯與執行 mini UART 版本

```bash
make mini_uart
make run_mini_uart
```

QEMU 若輸出：
```
mini UART initialized successfully!
```
即表示 mini UART 初始化成功 ✅

---

### 🧩 編譯與執行 PL011 UART 版本

```bash
make uart
make run_uart
```

QEMU 若輸出：
```
UART initialized successfully!
```
即表示 PL011 UART 初始化成功 ✅

---

## 📦 專案檔案說明

| 檔名 | 說明 |
|------|------|
| **boot.S** | 啟動程式（boot loader），設定堆疊與清空 `.bss` 段後跳入 `main`。 |
| **linker.ld** | 連結器腳本，定義程式在記憶體中的段落配置。 |
| **gpio.c / gpio.h** | GPIO 控制與輔助函式，負責切換 GPIO14/15 至 ALT 模式（UART 腳位）。 |
| **mini_uart.c** | mini UART（UART1）的驅動實作，使用 `AUX_MU_*` 暫存器。 |
| **mini_uart_main.c** | 使用 mini UART 的示範主程式。 |
| **uart.c** | PL011 full UART（UART0）的驅動實作，使用 `UART0_*` 暫存器。 |
| **uart_main.c** | 使用 PL011 UART 的示範主程式。 |
| **Makefile** | 建置與執行控制腳本，支援以下目標：<br>• `make mini_uart` / `make run_mini_uart`<br>• `make uart` / `make run_uart`<br>• `make clean` 清除輸出檔案 |

---

## 🧰 補充說明

- QEMU 預設會自動對應 UART，因此可直接看到輸出結果。  
- 在實體 Raspberry Pi 上執行時，  
  - mini UART 需設定 **GPIO14/15 → ALT5**  
  - PL011 UART 需設定 **GPIO14/15 → ALT0**  
  這部分已在 `gpio.c` 內自動完成。  
- 輸出映像檔：  
  - mini UART → `mini_uart_kernel8.img`  
  - PL011 UART → `uart_kernel8.img`

- 實機執行：
  - 將 .img 複製到 SD 卡開機分割區
    ```bash
    # mini UART 版本
    cp mini_uart_kernel8.img /Volumes/{VOLUME_NAME}/kernel8.img

    # PL011 UART 版本
    cp uart_kernel8.img /Volumes/{VOLUME_NAME}/kernel8.img
    ```
    > ⚠️ {VOLUME_NAME} 請替換為實際 SD 卡掛載名稱，例如 /Volumes/NO\ NAME。

