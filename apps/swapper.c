int main(int argc, char **argv)
{
    for (int k = 0; k < 10000; k++)
    {
        asm volatile("hlt");
    }

    return 0;
}
