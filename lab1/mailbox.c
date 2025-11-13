#include "mailbox.h"

// 等待 mailbox 可寫入 / 讀取
int mailbox_call(unsigned char ch, unsigned int *mailbox_msg) {
    unsigned int addr = (unsigned int)((unsigned long)mailbox_msg);
    if (addr & 0xF) return 0;  // 必須 16-byte 對齊

    // 寫入 (等待非滿)
    while (*MAILBOX_STATUS & MAILBOX_FULL);
    *MAILBOX_WRITE = addr | (ch & 0xF);

    // 讀取回覆
    while (1) {
        while (*MAILBOX_STATUS & MAILBOX_EMPTY);
        unsigned int resp = *MAILBOX_READ;
        if ((resp & 0xF) == ch && (resp & ~0xF) == addr)
            return mailbox_msg[1] == 0x80000000; // 成功標誌
    }
}

int get_arm_memory_info(unsigned int *mailbox) {

    // 取得 ARM memory
    mailbox[0] = 8 * 4;
    mailbox[1] = 0;
    mailbox[2] = MBOX_TAG_GET_ARM_MEMORY;
    mailbox[3] = 8;
    mailbox[4] = 0;
    mailbox[5] = 0; // base
    mailbox[6] = 0; // size
    mailbox[7] = MBOX_TAG_LAST;

    return mailbox_call(MBOX_CH_PROP, mailbox);
}

int get_board_revision(unsigned int *mailbox) {

    // 取得板子版本
    mailbox[0] = 7 * 4;
    mailbox[1] = 0;
    mailbox[2] = MBOX_TAG_GET_BOARD_REVISION;
    mailbox[3] = 4;
    mailbox[4] = 0;
    mailbox[5] = 0; // board revision
    mailbox[6] = MBOX_TAG_LAST;

    return mailbox_call(MBOX_CH_PROP, mailbox);
}