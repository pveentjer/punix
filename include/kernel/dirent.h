#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>

/* ------------------------------------------------------------
 * Directory entry structure (similar to POSIX struct dirent)
 * ------------------------------------------------------------ */
struct dirent
{
    uint32_t d_ino;      /* Inode number (or unique ID if no FS) */
    uint16_t d_reclen;   /* Total size of this record */
    uint8_t  d_type;     /* File type (see DT_* below) */
    char     d_name[256];/* Null-terminated filename */
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
