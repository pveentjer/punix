#ifndef _SIGNAL_H
#define _SIGNAL_H

#define SIGSEGV   11

#define SA_SIGINFO    4
#define SA_RESTART    0x10000000

#define SIG_DFL  ((void (*)(int))0)
#define SIG_IGN  ((void (*)(int))1)

typedef unsigned long sigset_t;

struct sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, void *, void *);
    };
    sigset_t   sa_mask;
    int        sa_flags;
    void     (*sa_restorer)(void);
};

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

int kill(pid_t pid, int sig);

#endif /* _SIGNAL_H */