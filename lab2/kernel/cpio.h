#ifndef CPIO_H
#define CPIO_H
#include <stddef.h>
#include <stdint.h>
// New ASCII CPIO header format
// https://man.freebsd.org/cgi/man.cgi?query=cpio&sektion=5
struct cpio_newc_header {
    char    c_magic[6];
    char    c_ino[8];
    char    c_mode[8];
    char    c_uid[8];
    char    c_gid[8];
    char    c_nlink[8];
    char    c_mtime[8];
    char    c_filesize[8];
    char    c_devmajor[8];
    char    c_devminor[8];
    char    c_rdevmajor[8];
    char    c_rdevminor[8];
    char    c_namesize[8];
    char    c_check[8];
}; // 110 bytes

#endif // CPIO_H

// =============================================================
// =============== Core CPIO Parser API ========================
// =============================================================

// 設定 cpio 映像的起始位址
void cpio_set_base(void *addr);
// 取得 cpio 映像的起始位址
void* cpio_get_entry_start();
// 取得下一個 entry 的位址
void* cpio_get_next(void *current);
// 根據檔名取得檔案內容位址，並回傳檔案大小
void* cpio_get_file(const char *filename, size_t *filesize);
// 列出所有檔案名稱
void cpio_list_all();

// =============================================================
// =============== Utilities for header parsing =================
// =============================================================

// hex string → uint32，例如 "0000001A" → 26
uint32_t cpio_hex_to_uint(const char *hex, int len);

// 對齊到 4 bytes（cpio newc 的規定）
size_t cpio_align4(size_t x);

// 判斷是不是結束 entry：filename == "TRAILER!!!"
int cpio_is_trailer(const char *name);