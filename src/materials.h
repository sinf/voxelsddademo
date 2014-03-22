#pragma once
#ifndef _MATERIALS_H
#define _MATERIALS_H
#include "types.h"

/* Material number 0 is treated as air (transparent).
The color is used as background color */

typedef struct Material
{
	uint8 color[3];
} Material;

/* Remember: Hard limit is 255 different materials + transparency */
typedef unsigned Material_ID;

/* Must be a power of 2 (because of POT texture) */
#define N_MATERIALS 64

#define MATERIAL_BITS 6
#define MATERIAL_BITMASK 0x1F

#endif
