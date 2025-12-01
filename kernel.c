#define VIDEO_MEMORY 0xB8000
#define WHITE_ON_BLACK 0x07
#define SCREEN_SIZE 80 * 25

static void clear_screen(void)
{
    volatile unsigned char *video = (volatile unsigned char *)VIDEO_MEMORY;
    for (int i = 0; i < SCREEN_SIZE * 2; i += 2) {
        video[i] = ' ';             // space
        video[i + 1] = WHITE_ON_BLACK;
    }
}

static void print(const char *s)
{
    volatile char *video = (volatile char*)VIDEO_MEMORY;
    while (*s) {
        *video++ = *s++;
        *video++ = WHITE_ON_BLACK;
    }
}

void main(void)
{
    clear_screen();
    print("Munix 0.001");
    for (;;) ;
}

