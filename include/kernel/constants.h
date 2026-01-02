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

#define EFAULT      14
#define ENOSYS      38
#define EINTR       4
#define EINVAL      22
#define ECHILD      10

/* -------------------------------------------------- */
/* Physical memory layout                             */
/* -------------------------------------------------- */

#define KB(x)  ((x) * 1024UL)
#define MB(x)  ((x) * 1024UL * 1024UL)
#define GB(x)  ((x) * 1024UL * 1024UL * 1024UL)


#define LOW_MEM_SIZE        MB(1)

/*
 * The base virtual address of the kernel
 */
#define KERNEL_VA_BASE      GB(2)
#define KERNEL_PA_BASE      MB(1)

#define KERNEL_VA_SIZE      MB(4)

// todo:
#define KERNEL_STACK_SIZE   KB(16)
#define KERNEL_STACK_TOP_VA    (KERNEL_VA_BASE + KERNEL_VA_SIZE - KB(4))

/*
 * The base virtual address of the process
 *
 * Processes always have a different virtual address than the kernel
 */
#define PROCESS_VA_BASE     MB(4)

#define PROCESS_PA_START    KERNEL_PA_BASE + KERNEL_VA_SIZE

/*
 * Each process gets a fixed 1 MiB region
 */
#define PROCESS_VA_SIZE     MB(1)

#define PROCESS_HEAP_SIZE   KB(512)

#define PROCESS_STACK_TOP   (PROCESS_VA_BASE + PROCESS_VA_SIZE - KB(4))

/* -------------------------------------------------- */
/* Process / scheduler limits                         */
/* -------------------------------------------------- */

/* Should be power of 2 */
#define MAX_PROCESS_CNT     64

#define PID_NONE            -1

#define MAX_SIGNALS         32

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


#define WNOHANG     0x01


#endif /* CONSTANTS_H */
