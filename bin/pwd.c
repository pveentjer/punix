#include "libc.h"

int main(void)
{
    char buf[4096];

    void *cwd = getcwd(buf, sizeof(buf));
    if (cwd == NULL)
    {
        printf("pwd: failed to get current directory\n");
        return 1;
    }

    printf("%s\n", buf);
    return 0;
}
