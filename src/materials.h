#pragma once
#ifndef _MATERIALS_H
#define _MATERIALS_H
#include "types.h"

/* Material number 0 is treated as air (transparent).
The color is used as background color */

typedef struct Material {
	uint8 color[4];
} Material;

typedef unsigned Material_ID;

#define MATERIAL_BITS 6
#define NUM_MATERIALS (1<<MATERIAL_BITS)
#define MATERIAL_BITMASK (NUM_MATERIALS-1)

#endif
