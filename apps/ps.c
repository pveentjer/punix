#include "../include/kernel/libc.h"

int main(int argc, char **argv)
{
    if (argc != 1) {
        printf("ps: no arguments expected\n");
        return 1;
    }
//
//    struct task_info tasks[128];
//    int count = sched_get_tasks(tasks, 128);
//
//    printf("PID  NAME\n");
//    for (int i = 0; i < count; i++) {
//        printf("%d    %s\n", tasks[i].pid, tasks[i].name);
//    }

    return 0;
}
