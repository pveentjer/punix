//
// Created by pveentjer on 1/8/26.
//

#ifndef IO_H
#define IO_H

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline uint8_t cmos(uint8_t reg)
{
    outb(0x70, 0x80 | reg);
    return inb(0x71);
}



#endif //IO_H
