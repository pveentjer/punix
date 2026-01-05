
#include "string.h"
#include "fcntl.h"
#include "stdio.h"

int main(void)
{
    /*
     * ESC[H   → cursor home
     * ESC[2J  → clear screen
     * ESC[3J  → clear scrollback (xterm-compatible, optional)
     */
    const char *seq = "\033[H\033[2J\033[3J";

    write(STDOUT_FILENO, seq, strlen(seq));

    return 0;
}
