#include "syscalls.h"
#include "process.h"
#include "sched.h"
#include "interrupt.h"

void process0(void){
    
    bool interrupts_enabled = interrupts_are_enabled();
    if(!interrupts_enabled){
        printf("process 0: Interrupts not enabled.\n");
    }

    sched_add_task("bin/process1");
    
    sched_add_task("bin/process2");

    exit(0);
}

REGISTER_PROCESS(process0, "bin/process0");