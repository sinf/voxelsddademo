#pragma once
#ifndef _MATERIALS_H
#define _MATERIALS_H
#include "types.h"

/* Material number 0 is treated as air (transparent). */

#define MATERIAL_BITS 6
#define NUM_MATERIALS (1<<MATERIAL_BITS)
#define MATERIAL_BITMASK (NUM_MATERIALS-1)

#endif
