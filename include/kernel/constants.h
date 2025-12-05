//
// Created by pveentjer on 12/5/25.
//

#ifndef MUNIX_CONSTANTS_H
#define MUNIX_CONSTANTS_H

#define MAX_PROCESS_CNT 64
// first app at 2 MiB
#define PROCESS_BASE 0x00200000
// each app 1 MiB
#define PROCESS_SIZE 0x00100000

#define MAX_PROCESS_NAME_LENGTH 64

#define MAX_SIGNALS 32   // enough for now

#endif //MUNIX_CONSTANTS_H
