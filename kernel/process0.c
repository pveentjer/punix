#include "syscalls.h"
#include "process.h"
#include "sched.h"

void process0(void){
    
    sched_add_task("process1");
    
    sched_add_task("process2");

    exit(0);
}

REGISTER_PROCESS(process0, "process0");