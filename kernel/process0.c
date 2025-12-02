#include "syscalls.h"
#include "process.h"
#include "sched.h"

void process0(void){
    
    sched_add_task("bin/process1");
    
    sched_add_task("bin/process2");

    exit(0);
}

REGISTER_PROCESS(process0, "bin/process0");