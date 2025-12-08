#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>

/* ------------------------------------------------------------
 * Directory entry structure (similar to POSIX struct dirent)
 * ------------------------------------------------------------ */
struct dirent
{
    /* Inode number (or unique ID if no FS) */
    uint32_t d_ino;
    /* Total size of this record */
    uint16_t d_reclen;
    /* File type (see DT_* below) */
    uint8_t d_type;
    /* Null-terminated filename */
    char d_name[MAX_FILENAME_LEN];
};

/* ------------------------------------------------------------
 * File type constants (used in d_type)
 * ------------------------------------------------------------ */
#define DT_UNKNOWN  0
#define DT_REG      1   /* Regular file */
#define DT_DIR      2   /* Directory */
#define DT_CHR      3   /* Character device */
#define DT_BLK      4   /* Block device */
#define DT_FIFO     5   /* FIFO / pipe */
#define DT_SOCK     6   /* Socket */
#define DT_LNK      7   /* Symbolic link */

/* ------------------------------------------------------------
 * Optional helper macros (mimic POSIX)
 * ------------------------------------------------------------ */
#define IFTODT(mode)  (((mode) >> 12) & 0xF)
#define DTTOIF(type)  ((type) << 12)

int getdents(int fd, struct dirent *buf, unsigned int count);

#endif /* DIRENT_H */
