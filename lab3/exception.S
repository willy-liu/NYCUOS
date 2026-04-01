.text

.equ CORE0_TIMER_IRQ_CTRL, 0x40000040

.macro save_all
    sub sp, sp, 34 * 8
    stp x0, x1, [sp, 16 * 0]
    stp x2, x3, [sp, 16 * 1]
    stp x4, x5, [sp, 16 * 2]
    stp x6, x7, [sp, 16 * 3]
    stp x8, x9, [sp, 16 * 4]
    stp x10, x11, [sp, 16 * 5]
    stp x12, x13, [sp, 16 * 6]
    stp x14, x15, [sp, 16 * 7]
    stp x16, x17, [sp, 16 * 8]
    stp x18, x19, [sp, 16 * 9]
    stp x20, x21, [sp, 16 * 10]
    stp x22, x23, [sp, 16 * 11]
    stp x24, x25, [sp, 16 * 12]
    stp x26, x27, [sp, 16 * 13]
    stp x28, x29, [sp, 16 * 14]
    str x30, [sp, 16 * 15]
    mrs x9, spsr_el1
    str x9, [sp, 16 * 16]
    mrs x9, elr_el1
    str x9, [sp, 16 * 16 + 8]
.endm

.macro load_all
    ldr x9, [sp, 16 * 16]
    msr spsr_el1, x9
    ldr x9, [sp, 16 * 16 + 8]
    msr elr_el1, x9
    ldp x0, x1, [sp, 16 * 0]
    ldp x2, x3, [sp, 16 * 1]
    ldp x4, x5, [sp, 16 * 2]
    ldp x6, x7, [sp, 16 * 3]
    ldp x8, x9, [sp, 16 * 4]
    ldp x10, x11, [sp, 16 * 5]
    ldp x12, x13, [sp, 16 * 6]
    ldp x14, x15, [sp, 16 * 7]
    ldp x16, x17, [sp, 16 * 8]
    ldp x18, x19, [sp, 16 * 9]
    ldp x20, x21, [sp, 16 * 10]
    ldp x22, x23, [sp, 16 * 11]
    ldp x24, x25, [sp, 16 * 12]
    ldp x26, x27, [sp, 16 * 13]
    ldp x28, x29, [sp, 16 * 14]
    ldr x30, [sp, 16 * 15]
    add sp, sp, 34 * 8
.endm

.global exception_handler
exception_handler:
    save_all
    mov x0, sp
    bl exception_entry
    load_all
    eret

.global irq_handler
irq_handler:
    save_all
    mov x0, sp
    bl irq_entry
    bl run_interrupt_tasks
    load_all
    eret

.align 11
.global exception_vector_table
exception_vector_table:
    b exception_handler
    .align 7
    b irq_handler
    .align 7
    b exception_handler
    .align 7
    b exception_handler
    .align 7

    b exception_handler
    .align 7
    b irq_handler
    .align 7
    b exception_handler
    .align 7
    b exception_handler
    .align 7

    b exception_handler
    .align 7
    b irq_handler
    .align 7
    b exception_handler
    .align 7
    b exception_handler
    .align 7

    b exception_handler
    .align 7
    b irq_handler
    .align 7
    b exception_handler
    .align 7
    b exception_handler
    .align 7

.global set_exception_vector_table
set_exception_vector_table:
    adr x0, exception_vector_table
    msr vbar_el1, x0
    isb
    ret

.global enter_user_program
enter_user_program:
    // x0: user entry address, x1: user stack top
    msr sp_el0, x1
    msr elr_el1, x0
    mov x2, #0x0
    msr spsr_el1, x2
    eret

.global core_timer_enable
core_timer_enable:
    mov x0, #1
    msr cntp_ctl_el0, x0 // Enable the timer

    mrs x0, cntfrq_el0
    msr cntp_tval_el0, x0 // Set the timer to trigger after 1 second (CNTFRQ_EL0 is the timer frequency)

    mov x0, #2
    ldr x1, =CORE0_TIMER_IRQ_CTRL
    str w0, [x1] // Enable the timer interrupt in the interrupt controller
    ret
