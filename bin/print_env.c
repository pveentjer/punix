#include <stdint.h>
#include "libc.h"
#include "stdio.h"

extern char **environ;

int main(int argc, char **argv)
{
    printf("bonkers\n");

    if (environ == NULL)
    {
        printf("environ is NULL\n");
        return 1;
    }

    for (int i = 0; environ[i] != NULL; i++)
    {
        printf("%s\n", environ[i]);
    }

    return 0;
}