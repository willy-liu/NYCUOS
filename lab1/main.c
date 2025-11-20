#include "mini_uart.h"
#include "mailbox.h"
#include "reboot.h"

#define MAX_CMD_LEN 64

void shell(void);
int strcmp(const char *s1, const char *s2);
int strlen(const char *s);

void main(void) {
    mini_uart_init();
    mini_uart_puts("Welcome to mini UART shell!\r\n");
    shell();   // 進入互動模式
}

void shell(void) {
    char buffer[MAX_CMD_LEN];
    int index = 0;
    mini_uart_puts("# ");  // 提示符

    while (1) {
        char c = mini_uart_getc();

        // 處理 Enter
        if (c == '\r' || c == '\n') {
            mini_uart_puts("\r\n");
            buffer[index] = '\0';  // 結尾字元

            // ===== 指令解析 =====
            if (index == 0) {
                // 空輸入，直接顯示新提示符
            } else if (strcmp(buffer, "help") == 0) {
                mini_uart_puts("Available commands:\r\n");
                mini_uart_puts("  help  : print all available commands\r\n");
                mini_uart_puts("  hello : print Hello World!\r\n");
                mini_uart_puts("  info : print system information\r\n");
                mini_uart_puts("  reboot: reboot the device\r\n");
            } else if (strcmp(buffer, "hello") == 0) {
                mini_uart_puts("Hello World!\r\n");
            
            } else if (strcmp(buffer, "info") == 0) {

                unsigned int mailbox[8] __attribute__((aligned(16)));

                if (get_board_revision(mailbox)) { // 取得板子版本
                    mini_uart_puts("Board Revision: ");
                    print_hex(mailbox[5]);
                    mini_uart_puts("\r\n");
                } else {
                    mini_uart_puts("Failed to get board revision.\r\n");
                }

                if (get_arm_memory_info(mailbox)) { // 取得 ARM memory
                    mini_uart_puts("ARM Memory Info:\r\n");
                    mini_uart_puts("  Base: ");
                    print_hex(mailbox[5]);
                    mini_uart_puts("\r\n  Size: ");
                    print_hex(mailbox[6]);
                    mini_uart_puts("\r\n");
                } else {
                    mini_uart_puts("Failed to get ARM memory info.\r\n");
                }
            } else if (strcmp(buffer, "reboot") == 0) {
                reset(10);  // reboot after 10 ticks
            } else {
                mini_uart_puts("Unknown command: ");
                mini_uart_puts(buffer);
                mini_uart_puts("\r\n");
            }

            // 重置狀態
            index = 0;
            mini_uart_puts("# ");
        }

        // 處理 Backspace
        else if (c == 127 || c == '\b') {
            if (index > 0) {
                index--;
                mini_uart_puts("\b \b"); // 往後刪除
            }
        }

        // 一般輸入字元
        else {
            if (index < MAX_CMD_LEN - 1) {
                buffer[index++] = c;
                mini_uart_send(c);  // Echo 顯示
            }
        }
    }
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strlen(const char *s) {
    int len = 0;
    while (*s++) len++;
    return len;
}