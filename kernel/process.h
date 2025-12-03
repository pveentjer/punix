#ifndef PROCESS_H
#define PROCESS_H

typedef int (*process_entry_t)(int argc, char **argv);

struct process_desc {
    const char       *name;   // e.g. "process1"
    process_entry_t   entry;  // function pointer
};

/* Put a process descriptor into a custom section ".proctable". */
#define REGISTER_PROCESS(fn, name_str) \
    static const struct process_desc __proc_##fn \
    __attribute__((used, section(".proctable"))) = { name_str, fn };

#endif // PROCESS_H
