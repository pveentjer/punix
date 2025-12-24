#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* ------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------ */

// GDT segment indices
#define GDT_KERNEL_CODE_IDX     1

// Segment selectors (index << 3)
#define GDT_KERNEL_CS     ((GDT_KERNEL_CODE_IDX) << 3)

#endif
