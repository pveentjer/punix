#ifndef _SIGNAL_H
#define _SIGNAL_H

#define SIGSEGV 11

int kill(pid_t pid, int sig);

#endif /* _SIGNAL_H */