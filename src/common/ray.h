#pragma once
#ifndef _RAY_H
#define _RAY_H
#include "vector.h"

/* Must be aligned because of SSE */
typedef struct Ray
{
	_MM_ALIGN16 vec3f o; /* origin */
	_MM_ALIGN16 vec3f d; /* direction */
} Ray;

#endif
