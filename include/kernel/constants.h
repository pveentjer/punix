#ifndef CONSTANTS_H
#define CONSTANTS_H

/* -------------------------------------------------- */
/* File descriptors                                  */
/* -------------------------------------------------- */

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

#define FD_NONE   -1

/* -------------------------------------------------- */
/* Errors                                            */
/* -------------------------------------------------- */

#define ENOSYS 38

/* -------------------------------------------------- */
/* Physical memory layout                             */
/* -------------------------------------------------- */

#define LOW_MEM_SIZE        (1u * 1024u * 1024u)

/*
 * Kernel is linked at 1 MiB and allowed to grow up to 1 MiB
 */
#define KERNEL_BASE         LOW_MEM_SIZE
#define KERNEL_SIZE         (1u * 1024u * 1024u)

/*
 * User processes start right after the kernel
 */
#define PROCESS_BASE        (KERNEL_BASE + KERNEL_SIZE)

/*
 * Each process gets a fixed 1 MiB region
 */
#define PROCESS_SIZE        (1u * 1024u * 1024u)

/* -------------------------------------------------- */
/* Process / scheduler limits                         */
/* -------------------------------------------------- */

/* Should be power of 2 */
#define MAX_PROCESS_CNT 64

#define PID_NONE -1

#define MAX_SIGNALS 32

/* -------------------------------------------------- */
/* Filesystem limits                                  */
/* -------------------------------------------------- */

/* Kernel-wide open file table (power of 2) */
#define MAX_FILE_CNT 1024

/* Per-process file limit (power of 2) */
#define RLIMIT_NOFILE 16

#define MAX_FILENAME_LEN 64

/* -------------------------------------------------- */
/* Integer limits                                     */
/* -------------------------------------------------- */

#define INT_MAX  2147483647
#define INT_MIN (-INT_MAX - 1)

/* -------------------------------------------------- */
/* TTY subsystem                                      */
/* -------------------------------------------------- */

#define TTY_COUNT             12u
#define TTY_INPUT_BUF_SIZE    256u
#define TTY_OUTPUT_BUF_SIZE   4096u

#endif /* CONSTANTS_H */
