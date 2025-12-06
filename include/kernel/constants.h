#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MAX_PROCESS_CNT 64
// first app at 2 MiB
#define PROCESS_BASE 0x00200000
// each app 1 MiB
#define PROCESS_SIZE 0x00100000

#define MAX_FILENAME_LEN 64

#define MAX_SIGNALS 32

// The maximum number of files per process
#define RLIMIT_NOFILE 16

#endif //CONSTANTS_H
