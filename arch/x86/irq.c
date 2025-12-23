// arch/x86/irq.c
#include <stdint.h>
#include <stdbool.h>
#include "kernel/irq.h"

/* ------------------------------------------------------------
 * IRQ state helpers
 * ------------------------------------------------------------ */
typedef uint32_t irq_state_t;

inline irq_state_t irq_disable(void)
{
    irq_state_t flags;
    __asm__ volatile(
            "pushfl\n\t"
            "popl %0\n\t"
            "cli\n\t"
            : "=r"(flags)
            :
            : "memory"
            );
    return flags;
}

inline void irq_restore(irq_state_t flags)
{
    __asm__ volatile(
            "pushl %0\n\t"
            "popfl\n\t"
            :
            : "r"(flags)
            : "memory", "cc"
            );
}

/* ------------------------------------------------------------
 * IDT structures
 * ------------------------------------------------------------ */
struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

/* ------------------------------------------------------------
 * PIC ports
 * ------------------------------------------------------------ */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* ------------------------------------------------------------
 * IRQ handler table
 * ------------------------------------------------------------ */
static void (*irq_handlers[IDT_ENTRIES])(void);

/* ------------------------------------------------------------
 * Port I/O
 * ------------------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ------------------------------------------------------------
 * IDT helper
 * ------------------------------------------------------------ */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

/* ------------------------------------------------------------
 * PIC helpers
 * ------------------------------------------------------------ */
static void pic_mask_all(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ------------------------------------------------------------
 * IRQ dispatcher (C)
 * ------------------------------------------------------------ */
void irq_dispatch(uint32_t vector)
{
    if (vector < IDT_ENTRIES)
    {
        void (*h)(void) = irq_handlers[vector];
        if (h)
        {
            h();
        }

        /*
         * Send EOI for hardware IRQs.
         *
         * Your current system uses the legacy (non-remapped) PIC layout:
         *   Master IRQs: vectors 0x08..0x0F
         *   Slave  IRQs: vectors 0x70..0x77
         *
         * (If you later remap PIC to 0x20..0x2F, adjust this accordingly.)
         */
        if ((vector >= 0x08 && vector <= 0x0F) ||
            (vector >= 0x70 && vector <= 0x77))
        {
            if (vector >= 0x70)
            {
                outb(PIC2_COMMAND, PIC_EOI);
            }
            outb(PIC1_COMMAND, PIC_EOI);
        }
    }
}

/* ------------------------------------------------------------
 * Flat per-vector stubs (generated with a C macro)
 * Each stub hardcodes its vector and calls irq_dispatch(vector).
 * ------------------------------------------------------------ */
#define DEFINE_IRQ_STUB(n)                                        \
    __attribute__((naked)) void irq_stub_##n(void)                \
    {                                                             \
        __asm__ volatile(                                         \
            "pushal\n\t"                                          \
            "pushl $" #n "\n\t"                                   \
            "call irq_dispatch\n\t"                               \
            "addl $4, %esp\n\t"                                   \
            "popal\n\t"                                           \
            "iret\n\t"                                            \
        );                                                        \
    }

/* Generate all 256 stubs */
#define X(n) DEFINE_IRQ_STUB(n)

X(0)   X(1)   X(2)   X(3)   X(4)   X(5)   X(6)   X(7)
X(8)   X(9)   X(10)  X(11)  X(12)  X(13)  X(14)  X(15)
X(16)  X(17)  X(18)  X(19)  X(20)  X(21)  X(22)  X(23)
X(24)  X(25)  X(26)  X(27)  X(28)  X(29)  X(30)  X(31)
X(32)  X(33)  X(34)  X(35)  X(36)  X(37)  X(38)  X(39)
X(40)  X(41)  X(42)  X(43)  X(44)  X(45)  X(46)  X(47)
X(48)  X(49)  X(50)  X(51)  X(52)  X(53)  X(54)  X(55)
X(56)  X(57)  X(58)  X(59)  X(60)  X(61)  X(62)  X(63)
X(64)  X(65)  X(66)  X(67)  X(68)  X(69)  X(70)  X(71)
X(72)  X(73)  X(74)  X(75)  X(76)  X(77)  X(78)  X(79)
X(80)  X(81)  X(82)  X(83)  X(84)  X(85)  X(86)  X(87)
X(88)  X(89)  X(90)  X(91)  X(92)  X(93)  X(94)  X(95)
X(96)  X(97)  X(98)  X(99)  X(100) X(101) X(102) X(103)
X(104) X(105) X(106) X(107) X(108) X(109) X(110) X(111)
X(112) X(113) X(114) X(115) X(116) X(117) X(118) X(119)
X(120) X(121) X(122) X(123) X(124) X(125) X(126) X(127)
X(128) X(129) X(130) X(131) X(132) X(133) X(134) X(135)
X(136) X(137) X(138) X(139) X(140) X(141) X(142) X(143)
X(144) X(145) X(146) X(147) X(148) X(149) X(150) X(151)
X(152) X(153) X(154) X(155) X(156) X(157) X(158) X(159)
X(160) X(161) X(162) X(163) X(164) X(165) X(166) X(167)
X(168) X(169) X(170) X(171) X(172) X(173) X(174) X(175)
X(176) X(177) X(178) X(179) X(180) X(181) X(182) X(183)
X(184) X(185) X(186) X(187) X(188) X(189) X(190) X(191)
X(192) X(193) X(194) X(195) X(196) X(197) X(198) X(199)
X(200) X(201) X(202) X(203) X(204) X(205) X(206) X(207)
X(208) X(209) X(210) X(211) X(212) X(213) X(214) X(215)
X(216) X(217) X(218) X(219) X(220) X(221) X(222) X(223)
X(224) X(225) X(226) X(227) X(228) X(229) X(230) X(231)
X(232) X(233) X(234) X(235) X(236) X(237) X(238) X(239)
X(240) X(241) X(242) X(243) X(244) X(245) X(246) X(247)
X(248) X(249) X(250) X(251) X(252) X(253) X(254) X(255)

#undef X

/* Table of stub addresses */
static void (*const irq_stub_table[IDT_ENTRIES])(void) =
        {
#define A(n) irq_stub_##n,
                A(0)   A(1)   A(2)   A(3)   A(4)   A(5)   A(6)   A(7)
                A(8)   A(9)   A(10)  A(11)  A(12)  A(13)  A(14)  A(15)
                A(16)  A(17)  A(18)  A(19)  A(20)  A(21)  A(22)  A(23)
                A(24)  A(25)  A(26)  A(27)  A(28)  A(29)  A(30)  A(31)
                A(32)  A(33)  A(34)  A(35)  A(36)  A(37)  A(38)  A(39)
                A(40)  A(41)  A(42)  A(43)  A(44)  A(45)  A(46)  A(47)
                A(48)  A(49)  A(50)  A(51)  A(52)  A(53)  A(54)  A(55)
                A(56)  A(57)  A(58)  A(59)  A(60)  A(61)  A(62)  A(63)
                A(64)  A(65)  A(66)  A(67)  A(68)  A(69)  A(70)  A(71)
                A(72)  A(73)  A(74)  A(75)  A(76)  A(77)  A(78)  A(79)
                A(80)  A(81)  A(82)  A(83)  A(84)  A(85)  A(86)  A(87)
                A(88)  A(89)  A(90)  A(91)  A(92)  A(93)  A(94)  A(95)
                A(96)  A(97)  A(98)  A(99)  A(100) A(101) A(102) A(103)
                A(104) A(105) A(106) A(107) A(108) A(109) A(110) A(111)
                A(112) A(113) A(114) A(115) A(116) A(117) A(118) A(119)
                A(120) A(121) A(122) A(123) A(124) A(125) A(126) A(127)
                A(128) A(129) A(130) A(131) A(132) A(133) A(134) A(135)
                A(136) A(137) A(138) A(139) A(140) A(141) A(142) A(143)
                A(144) A(145) A(146) A(147) A(148) A(149) A(150) A(151)
                A(152) A(153) A(154) A(155) A(156) A(157) A(158) A(159)
                A(160) A(161) A(162) A(163) A(164) A(165) A(166) A(167)
                A(168) A(169) A(170) A(171) A(172) A(173) A(174) A(175)
                A(176) A(177) A(178) A(179) A(180) A(181) A(182) A(183)
                A(184) A(185) A(186) A(187) A(188) A(189) A(190) A(191)
                A(192) A(193) A(194) A(195) A(196) A(197) A(198) A(199)
                A(200) A(201) A(202) A(203) A(204) A(205) A(206) A(207)
                A(208) A(209) A(210) A(211) A(212) A(213) A(214) A(215)
                A(216) A(217) A(218) A(219) A(220) A(221) A(222) A(223)
                A(224) A(225) A(226) A(227) A(228) A(229) A(230) A(231)
                A(232) A(233) A(234) A(235) A(236) A(237) A(238) A(239)
                A(240) A(241) A(242) A(243) A(244) A(245) A(246) A(247)
                A(248) A(249) A(250) A(251) A(252) A(253) A(254) A(255)
#undef A
        };

/* ------------------------------------------------------------
 * IRQ registration API
 * ------------------------------------------------------------ */
void irq_register_handler(uint8_t vector, void (*handler)(void))
{
    irq_handlers[vector] = handler;
}

/* ------------------------------------------------------------
 * IDT init
 * ------------------------------------------------------------ */
void idt_init(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        irq_handlers[i] = 0;
        idt_set_gate((uint8_t)i, (uint32_t)irq_stub_table[i], 0x08, 0x8E);
    }

    __asm__ volatile("lidt (%0)" : : "r"(&idtp));

    pic_mask_all();
}

void interrupts_enable(void)
{
    __asm__ volatile("sti");
}

bool interrupts_are_enabled(void)
{
    uint32_t eflags;
    __asm__ volatile("pushfl\n\tpopl %0" : "=r"(eflags));
    return (eflags & 0x200u) != 0;
}
