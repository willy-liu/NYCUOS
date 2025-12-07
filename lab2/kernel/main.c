#include <stdint.h>
#include "uart.h"
#include "gpio.h"
#include "cpio.h"
#include "simple_alloc.h"

static void print_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help            Show this message\n");
    uart_puts("  ls              List files inside initramfs\n");
    uart_puts("  cat <path>      Print file content from initramfs\n");
    uart_puts("  alloc [bytes] [string]   Allocate a char* and print it\n");
}

static int match_command(const char *input, const char *keyword, const char **args_out) {
    int i = 0;
    while (keyword[i] && input[i] == keyword[i]) {
        i++;
    }
    if (keyword[i] != '\0') {
        return 0;
    }
    if (input[i] != '\0' && input[i] != ' ' && input[i] != '\t') {
        return 0;
    }
    if (args_out) {
        const char *args = input + i;
        while (*args == ' ' || *args == '\t') {
            args++;
        }
        *args_out = args;
    }
    return 1;
}

static const char *skip_spaces(const char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static size_t read_token_len(const char *s) {
    size_t len = 0;
    while (s[len] != '\0' && s[len] != ' ' && s[len] != '\t') {
        len++;
    }
    return len;
}

static int parse_decimal_token(const char *token, size_t len, size_t *out_value) {
    if (len == 0)
        return 0;
    size_t value = 0;
    for (size_t i = 0; i < len; i++) {
        char c = token[i];
        if (c < '0' || c > '9') {
            return 0;
        }
        value = value * 10u + (size_t)(c - '0');
    }
    *out_value = value;
    return 1;
}

static size_t simple_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

static void handle_alloc_command(const char *args) {
    const char *cursor = skip_spaces(args);
    size_t specified_size = 0;
    int size_provided = 0;
    const char *string_arg = NULL;

    if (*cursor != '\0') {
        size_t first_len = read_token_len(cursor);
        if (parse_decimal_token(cursor, first_len, &specified_size)) {
            size_provided = 1;
            cursor = skip_spaces(cursor + first_len);
            if (*cursor != '\0') {
                string_arg = cursor;
            }
        } else {
            string_arg = cursor;
        }
    }

    size_t string_len = string_arg ? simple_strlen(string_arg) : 0;
    size_t request = size_provided ? specified_size : (string_len ? string_len : 8);

    if (request == 0) {
        uart_puts("alloc size must be greater than zero\n");
        return;
    }

    size_t total_bytes = request + 1; // +1 for '\0'
    char *string = (char *)simple_alloc(total_bytes);
    if (!string) {
        uart_puts("simple_alloc: not enough space\n");
        return;
    }

    size_t copy_len = 0;
    int truncated = 0;
    if (string_arg) {
        copy_len = string_len;
        if (copy_len > request) {
            copy_len = request;
            truncated = 1;
        }
        for (size_t i = 0; i < copy_len; i++) {
            string[i] = string_arg[i];
        }
        string[copy_len] = '\0';
    } else {
        for (size_t i = 0; i < request; i++) {
            string[i] = (char)('a' + (i % 26));
        }
        string[request] = '\0';
        copy_len = request;
    }

    uart_puts("char* string = simple_alloc(");
    print_hex((unsigned int)total_bytes);
    uart_puts(");\n");

    uart_puts("string address: ");
    print_hex((unsigned int)(uintptr_t) string);
    uart_puts("\nstring content: ");
    uart_puts(string);
    uart_puts("\n");

    if (truncated) {
        uart_puts("(input truncated to fit allocation)\n");
    }

    uart_puts("Remaining bytes: ");
    print_hex((unsigned int)simple_alloc_bytes_free());
    uart_puts("\nHex preview: ");

    size_t preview = copy_len < 16 ? copy_len : 16;
    for (size_t i = 0; i < preview; i++) {
        uart_puts(" ");
        print_hex((unsigned int)(unsigned char)string[i]);
    }
    uart_puts("\n");
}

int main() {
    gpio_init_uart();
    simple_alloc_init(NULL, 0);
    uart_puts("this is kernel cpio test!\n");
    // cpio_set_base((void*)0x8000000); // qemu initramfs base address 0x8000000
    cpio_set_base((void*)0x20000000); // raspi3b initramfs base address 0x20000000
    uart_puts("Listing all files in cpio archive:\n");
    cpio_list_all();
    char cmd[128];
    while (1) {
        uart_puts("> ");
        int len = 0;
        for (;;) {
            char c = uart_getc();
            if (c == '\r' || c == '\n') {
                uart_puts("\n");
                cmd[len] = '\0';
                break;
            }
            if ((c == '\b' || c == 127)) {
                if (len > 0) {
                    uart_puts("\b \b");
                    len--;
                }
                continue;
            }
            if (len < (int)sizeof(cmd) - 1) {
                uart_putc(c);
                cmd[len++] = c;
            }
        }
        char *p = cmd;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            continue;
        }
        const char *args = NULL;
        if (match_command(p, "help", &args)) {
            print_help();
            continue;
        }
        if (match_command(p, "ls", &args)) {
            cpio_list_all();
            continue;
        }
        if (match_command(p, "cat", &args)) {
            if (*args == '\0') {
                uart_puts("path required\n");
                continue;
            }
            unsigned long size = 0;
            const char *data = (const char *)cpio_get_file(args, &size);
            if (!data) {
                uart_puts("file not found\n");
                continue;
            }
            for (unsigned long i = 0; i < size; i++) {
                uart_putc(data[i]);
            }
            if (size == 0 || data[size - 1] != '\n') {
                uart_puts("\n");
            }
            continue;
        }
        if (match_command(p, "alloc", &args)) {
            handle_alloc_command(args);
            continue;
        }
        uart_puts("unknown command, use `help`\n");
    }
}