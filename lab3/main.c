#include <stdint.h>
#include "mini_uart.h"
#include "gpio.h"
#include "cpio.h"
#include "simple_alloc.h"
#include "fdt.h"
#include "mailbox.h"
#include "reboot.h"

extern void set_exception_vector_table(void);
extern void enter_user_program(void *entry, void *user_sp_top);
extern void core_timer_enable(void);

static uint8_t user_stack[4096] __attribute__((aligned(16)));

#define CORE0_IRQ_SOURCE ((volatile unsigned int *)0x40000060)

#define TIMER_MAX_EVENTS 32
#define TIMEOUT_MSG_MAX 96

#define TASK_QUEUE_MAX 64
#define TASK_PRIO_TIMER 1
#define TASK_PRIO_UART  2

typedef void (*timer_callback_t)(void *);

struct timer_event {
    int used;
    uint64_t deadline_tick;
    uint64_t order;
    timer_callback_t callback;
    void *data;
};

struct timeout_context {
    int used;
    uint64_t command_tick;
    char message[TIMEOUT_MSG_MAX];
};

static struct timer_event timer_events[TIMER_MAX_EVENTS];
static struct timeout_context timeout_contexts[TIMER_MAX_EVENTS];
static uint64_t timer_freq_hz = 0;
static uint64_t timer_order_seq = 0;

typedef void (*task_callback_t)(void *);

struct irq_task {
    int used;
    uint32_t priority;
    uint64_t order;
    task_callback_t callback;
    void *data;
};

static struct irq_task irq_tasks[TASK_QUEUE_MAX];
static uint64_t irq_task_order_seq = 0;
static int current_task_priority = -1;

static void print_dec_u64(uint64_t value);
static uint64_t timer_now_tick(void);
static void core_timer_handler(void);
static void timer_multiplex_init(void);
static int timer_add_event(timer_callback_t callback, void *data, uint64_t after_seconds);
static int irq_task_enqueue(task_callback_t callback, void *data, uint32_t priority);
static int irq_task_pick_highest(void);
static void irq_task_run_all_preemptive(void);
static void uart_task_callback(void *data);
static void timer_task_callback(void *data);

static char shell_getc_async(void) {
    char c;
    while (uart_async_read(&c, 1) == 0) {
        asm volatile("wfi");
    }
    return c;
}

static void print_dec_u64(uint64_t value) {
    char buf[21];
    int i = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

static uint64_t timer_now_tick(void) {
    uint64_t now = 0;
    asm volatile("mrs %0, cntpct_el0" : "=r"(now));
    return now;
}

static void core_timer_disable(void) {
    uint64_t ctl = 0;
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(ctl));
}

static int irq_task_enqueue(task_callback_t callback, void *data, uint32_t priority) {
    for (int i = 0; i < TASK_QUEUE_MAX; i++) {
        if (irq_tasks[i].used) {
            continue;
        }
        irq_tasks[i].used = 1;
        irq_tasks[i].priority = priority;
        irq_tasks[i].order = irq_task_order_seq++;
        irq_tasks[i].callback = callback;
        irq_tasks[i].data = data;
        return 0;
    }
    return -1;
}

static int irq_task_pick_highest(void) {
    int best = -1;
    for (int i = 0; i < TASK_QUEUE_MAX; i++) {
        if (!irq_tasks[i].used) {
            continue;
        }
        if (best < 0 ||
            irq_tasks[i].priority > irq_tasks[best].priority ||
            (irq_tasks[i].priority == irq_tasks[best].priority &&
             irq_tasks[i].order < irq_tasks[best].order)) {
            best = i;
        }
    }
    return best;
}

static void irq_task_run_all_preemptive(void) {
    uint64_t old_daif = 0;
    asm volatile("mrs %0, daif" : "=r"(old_daif));
    asm volatile("msr daifclr, #2");

    int interrupted_prio = current_task_priority;
    while (1) {
        int idx = irq_task_pick_highest();
        if (idx < 0) {
            break;
        }

        // Nested preemption: only run tasks that can preempt the interrupted one.
        if (interrupted_prio >= 0 && (int)irq_tasks[idx].priority <= interrupted_prio) {
            break;
        }

        task_callback_t cb = irq_tasks[idx].callback;
        void *cb_data = irq_tasks[idx].data;
        int prev_prio = current_task_priority;

        irq_tasks[idx].used = 0;
        current_task_priority = (int)irq_tasks[idx].priority;
        if (cb) {
            cb(cb_data);
        }
        current_task_priority = prev_prio;
    }

    asm volatile("msr daif, %0" :: "r"(old_daif));
}

static void uart_task_callback(void *data) {
    (void)data;
    uart_irq_bottom_half();
}

static void timer_task_callback(void *data) {
    (void)data;
    core_timer_handler();
}

void run_interrupt_tasks(void) {
    irq_task_run_all_preemptive();
}

static void core_timer_set_deadline(uint64_t deadline_tick) {
    uint64_t ctl = 1;
    asm volatile("msr cntp_cval_el0, %0" :: "r"(deadline_tick));
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(ctl));
}

static uint64_t tick_to_sec(uint64_t tick) {
    if (timer_freq_hz == 0) {
        return 0;
    }
    return tick / timer_freq_hz;
}

static void timer_reprogram_next_deadline(void) {
    int best = -1;
    for (int i = 0; i < TIMER_MAX_EVENTS; i++) {
        if (!timer_events[i].used) {
            continue;
        }
        if (best < 0 ||
            timer_events[i].deadline_tick < timer_events[best].deadline_tick ||
            (timer_events[i].deadline_tick == timer_events[best].deadline_tick &&
             timer_events[i].order < timer_events[best].order)) {
            best = i;
        }
    }

    if (best < 0) {
        core_timer_disable();
        return;
    }

    core_timer_set_deadline(timer_events[best].deadline_tick);
}

static int timer_add_event(timer_callback_t callback, void *data, uint64_t after_seconds) {
    if (timer_freq_hz == 0) {
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < TIMER_MAX_EVENTS; i++) {
        if (!timer_events[i].used) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1;
    }

    uint64_t now = timer_now_tick();
    uint64_t after_tick = after_seconds * timer_freq_hz;

    timer_events[slot].used = 1;
    timer_events[slot].deadline_tick = now + after_tick;
    timer_events[slot].order = timer_order_seq++;
    timer_events[slot].callback = callback;
    timer_events[slot].data = data;

    timer_reprogram_next_deadline();
    return 0;
}

static void timeout_message_callback(void *data) {
    struct timeout_context *ctx = (struct timeout_context *)data;
    uint64_t now_tick = timer_now_tick();

    uart_puts("[setTimeout] now=");
    print_dec_u64(tick_to_sec(now_tick));
    uart_puts("s cmd=");
    print_dec_u64(tick_to_sec(ctx->command_tick));
    uart_puts("s msg=");
    uart_puts(ctx->message);
    uart_puts("\n");

    ctx->used = 0;
}

static void timer_multiplex_init(void) {
    asm volatile("mrs %0, cntfrq_el0" : "=r"(timer_freq_hz));
    core_timer_disable();
}

static void core_timer_handler(void) {
    while (1) {
        int best = -1;
        uint64_t now = timer_now_tick();

        for (int i = 0; i < TIMER_MAX_EVENTS; i++) {
            if (!timer_events[i].used) {
                continue;
            }
            if (timer_events[i].deadline_tick > now) {
                continue;
            }
            if (best < 0 ||
                timer_events[i].deadline_tick < timer_events[best].deadline_tick ||
                (timer_events[i].deadline_tick == timer_events[best].deadline_tick &&
                 timer_events[i].order < timer_events[best].order)) {
                best = i;
            }
        }

        if (best < 0) {
            break;
        }

        timer_callback_t callback = timer_events[best].callback;
        void *cb_data = timer_events[best].data;
        timer_events[best].used = 0;

        if (callback) {
            callback(cb_data);
        }
    }

    timer_reprogram_next_deadline();
}

void irq_entry(uint64_t *saved_regs) {
    (void)saved_regs;

    // Core0 IRQ source bit1 = CNTPNSIRQ.
    if ((*CORE0_IRQ_SOURCE) & (1u << 1)) {
        core_timer_disable();
        if (irq_task_enqueue(timer_task_callback, NULL, TASK_PRIO_TIMER) != 0) {
            uart_puts("[irq] timer task queue full\n");
            core_timer_handler();
        }
        return;
    }

    if (uart_irq_pending()) {
        uart_irq_top_half();
        if (irq_task_enqueue(uart_task_callback, NULL, TASK_PRIO_UART) != 0) {
            uart_puts("[irq] uart task queue full\n");
            uart_irq_bottom_half();
        }
        return;
    }

    uart_puts("[irq] unknown source\n");
}

static void print_help(void) {
    uart_puts("Available commands:\n");
    uart_puts("  help            Show this message\n");
    uart_puts("  hello           Print Hello World!\n");
    uart_puts("  info            Print system information (board revision, ARM memory)\n");
    uart_puts("  reboot          Reboot the device\n");
    uart_puts("  ls              List files inside initramfs\n");
    uart_puts("  cat <path>      Print file content from initramfs\n");
    uart_puts("  alloc [bytes] [string]   Allocate a char* and print it\n");
    uart_puts("  run [path]      Run user program from initramfs (default: user.img)\n");
    uart_puts("  setTimeout <message> <seconds>   Print message after timeout\n");
}

static void run_user_program(const char *path) {
    size_t size = 0;
    void *entry = cpio_get_file(path, &size);
    if (!entry) {
        // Most cpio tools store entries as "./name"; try that form as fallback.
        char alt[64];
        int i = 0;
        alt[i++] = '.';
        alt[i++] = '/';
        while (path[i - 2] && i < (int)sizeof(alt) - 1) {
            alt[i] = path[i - 2];
            i++;
        }
        alt[i] = '\0';
        entry = cpio_get_file(alt, &size);
    }
    if (!entry) {
        uart_puts("user program not found in initramfs\n");
        return;
    }

    uintptr_t user_sp_top = (uintptr_t)user_stack + sizeof(user_stack);
    user_sp_top &= ~(uintptr_t)0xF; // keep SP 16-byte aligned

    uart_puts("Run user program at ");
    print_hex64((uint64_t)(uintptr_t)entry);
    uart_puts(", size=");
    print_hex((unsigned int)size);
    uart_puts("\n");

    enter_user_program(entry, (void *)user_sp_top);
}

void exception_entry(uint64_t *saved_regs) {
    uint64_t spsr = 0;
    uint64_t elr = 0;
    uint64_t esr = 0;

    asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    asm volatile("mrs %0, esr_el1" : "=r"(esr));

    uart_puts("[exception] spsr_el1=");
    print_hex64(spsr);
    uart_puts(" elr_el1=");
    print_hex64(elr);
    uart_puts(" esr_el1=");
    print_hex64(esr);
    uart_puts("\n");

    // ESR EC = 0x15 means SVC from AArch64.
    // For SVC, ELR_EL1 already points to the next instruction.
    // Do not add +4, otherwise user flow skips one instruction.
    if (((esr >> 26) & 0x3F) == 0x15) {
        uart_puts("[exception] user x0=");
        print_hex64(saved_regs[0]); // x0 saved from EL0 context
        uart_puts("\n");
        asm volatile("msr elr_el1, %0" :: "r"(elr));
    }
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

static int parse_set_timeout_args(const char *args, char *message_out, size_t message_cap,
                                  uint64_t *seconds_out) {
    const char *start = skip_spaces(args);
    if (*start == '\0') {
        return 0;
    }

    const char *end = start;
    while (*end) {
        end++;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    if (end == start) {
        return 0;
    }

    const char *sec_begin = end;
    while (sec_begin > start && sec_begin[-1] != ' ' && sec_begin[-1] != '\t') {
        sec_begin--;
    }
    if (sec_begin == start) {
        return 0;
    }

    uint64_t seconds = 0;
    for (const char *p = sec_begin; p < end; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        seconds = seconds * 10u + (uint64_t)(*p - '0');
    }

    const char *msg_end = sec_begin;
    while (msg_end > start && (msg_end[-1] == ' ' || msg_end[-1] == '\t')) {
        msg_end--;
    }
    if (msg_end == start) {
        return 0;
    }

    size_t msg_len = (size_t)(msg_end - start);
    if (msg_len >= message_cap) {
        msg_len = message_cap - 1;
    }
    for (size_t i = 0; i < msg_len; i++) {
        message_out[i] = start[i];
    }
    message_out[msg_len] = '\0';
    *seconds_out = seconds;
    return 1;
}

static void handle_set_timeout_command(const char *args) {
    char message[TIMEOUT_MSG_MAX];
    uint64_t seconds = 0;

    if (!parse_set_timeout_args(args, message, sizeof(message), &seconds)) {
        uart_puts("usage: setTimeout <message> <seconds>\n");
        return;
    }

    int ctx_idx = -1;
    for (int i = 0; i < TIMER_MAX_EVENTS; i++) {
        if (!timeout_contexts[i].used) {
            ctx_idx = i;
            break;
        }
    }
    if (ctx_idx < 0) {
        uart_puts("setTimeout queue full\n");
        return;
    }

    timeout_contexts[ctx_idx].used = 1;
    timeout_contexts[ctx_idx].command_tick = timer_now_tick();

    size_t msg_len = simple_strlen(message);
    if (msg_len >= sizeof(timeout_contexts[ctx_idx].message)) {
        msg_len = sizeof(timeout_contexts[ctx_idx].message) - 1;
    }
    for (size_t i = 0; i < msg_len; i++) {
        timeout_contexts[ctx_idx].message[i] = message[i];
    }
    timeout_contexts[ctx_idx].message[msg_len] = '\0';

    if (timer_add_event(timeout_message_callback, &timeout_contexts[ctx_idx], seconds) != 0) {
        timeout_contexts[ctx_idx].used = 0;
        uart_puts("setTimeout add failed\n");
        return;
    }

    uart_puts("setTimeout scheduled: ");
    uart_puts(timeout_contexts[ctx_idx].message);
    uart_puts(" after ");
    print_dec_u64(seconds);
    uart_puts("s\n");
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

/* ----------------------------------------------------------------
 * Device-tree helpers: find initramfs address from /chosen node
 * ---------------------------------------------------------------- */
static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static uintptr_t initramfs_addr = 0;

static void initramfs_callback(const char *node_name, const char *prop_name,
                                const void *data, uint32_t len)
{
    if (!str_eq(node_name, "chosen"))
        return;

    if (str_eq(prop_name, "linux,initrd-start")) {
        if (len == 4) {
            initramfs_addr = (uintptr_t)fdt_read_be32(data);
        } else if (len == 8) {
            /* 64-bit big-endian address */
            uint32_t hi = fdt_read_be32(data);
            uint32_t lo = fdt_read_be32((const uint8_t *)data + 4);
            initramfs_addr = ((uintptr_t)hi << 32) | lo;
        }
    }
}

int main(void *dtb_addr) {
    uart_init();
    simple_alloc_init(NULL, 0);
    set_exception_vector_table();
    core_timer_enable();
    timer_multiplex_init();
    uart_enable_interrupts();
    asm volatile("msr daifclr, #2"); // 在EL1開啟中斷，這樣 uart_irq_handler 才能被呼叫
    uart_puts("this is kernel cpio test!\n");

    /* Parse DTB to discover initramfs address dynamically */
    fdt_init(dtb_addr);
    fdt_traverse(initramfs_callback);

    if (initramfs_addr) {
        uart_puts("[fdt] initramfs found at: ");
        print_hex((unsigned int)initramfs_addr);
        uart_puts("\n");
        cpio_set_base((void *)initramfs_addr);
    } else {
        uart_puts("[fdt] WARNING: initramfs not found in DTB, using default 0x8000000/0x2000000\n");
        cpio_set_base((void *)0x8000000); // qemu default initramfs address
        // cpio_set_base((void *)0x2000000); // Raspberry Pi 3 default initramfs address
    }

    uart_puts("Listing all files in cpio archive:\n");
    cpio_list_all();
    char cmd[128];
    while (1) {
        uart_puts("> ");
        int len = 0;
        for (;;) {
            char c = shell_getc_async();
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
        if (match_command(p, "hello", &args)) {
            uart_puts("Hello World!\n");
            continue;
        }
        if (match_command(p, "info", &args)) {
            unsigned int mailbox[8] __attribute__((aligned(16)));

            if (get_board_revision(mailbox)) {
                uart_puts("Board Revision: ");
                print_hex(mailbox[5]);
                uart_puts("\n");
            } else {
                uart_puts("Failed to get board revision.\n");
            }

            if (get_arm_memory_info(mailbox)) {
                uart_puts("ARM Memory Info:\n");
                uart_puts("  Base: ");
                print_hex(mailbox[5]);
                uart_puts("\n  Size: ");
                print_hex(mailbox[6]);
                uart_puts("\n");
            } else {
                uart_puts("Failed to get ARM memory info.\n");
            }
            continue;
        }
        if (match_command(p, "reboot", &args)) {
            reset(10);
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
        if (match_command(p, "run", &args)) {
            const char *path = (*args == '\0') ? "user.img" : args;
            run_user_program(path);
            continue;
        }
        if (match_command(p, "setTimeout", &args)) {
            handle_set_timeout_command(args);
            continue;
        }
        uart_puts("unknown command, use `help`\n");
    }
}