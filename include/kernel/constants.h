#ifndef CONSTANTS_H
#define CONSTANTS_H


#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2


#define ENOSYS 38

// The maximum number of processes.
// Should to be power of 2.
#define MAX_PROCESS_CNT 64

// first app at 2 MiB
#define PROCESS_BASE 0x00200000
// each app 1 MiB
#define PROCESS_SIZE 0x00100000

#define MAX_FILENAME_LEN 64

#define MAX_SIGNALS 32

// The maximum number of open files for the kernel
// should be power of 2.
#define MAX_FILE_CNT 1024

// The maximum number of files per process
// should be power of 2
#define RLIMIT_NOFILE 16

#define FD_NONE -1

#define PID_NONE -1

#define INT_MAX  2147483647
#define INT_MIN (-INT_MAX - 1)

#define TTY_COUNT             5u
#define TTY_INPUT_BUF_SIZE    256u
#define TTY_OUTPUT_BUF_SIZE   4096u

#endif //CONSTANTS_H
