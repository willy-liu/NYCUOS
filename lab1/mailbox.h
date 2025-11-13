#ifndef _MAILBOX_H_
#define _MAILBOX_H_

#include <stdint.h>

// Mailbox base (for Raspberry Pi 3)
#define MMIO_BASE        0x3F000000
#define MAILBOX_BASE     (MMIO_BASE + 0xB880)

#define MAILBOX_READ     ((volatile unsigned int*)(MAILBOX_BASE + 0x00))
#define MAILBOX_STATUS   ((volatile unsigned int*)(MAILBOX_BASE + 0x18))
#define MAILBOX_WRITE    ((volatile unsigned int*)(MAILBOX_BASE + 0x20))

#define MAILBOX_FULL     0x80000000
#define MAILBOX_EMPTY    0x40000000

// 通道號 (channel)
#define MBOX_CH_PROP     8    // Property channel for GPU queries

// Tags (property messages)
#define MBOX_TAG_GET_BOARD_REVISION  0x00010002
#define MBOX_TAG_GET_ARM_MEMORY      0x00010005
#define MBOX_TAG_LAST                0x00000000

// 結構
int mailbox_call(unsigned char ch, unsigned int *mailbox_msg);
int get_arm_memory_info(unsigned int *mailbox);
int get_board_revision(unsigned int *mailbox);

#endif
