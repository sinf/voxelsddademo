#pragma once
#ifndef _TYPES_H
#define _TYPES_H

/* Note: stdint.h is not part of C89 */
#include <stdint.h>

#ifdef __GNUC__
	#define _MM_ALIGN16 __attribute__ ((aligned (16)))
#endif

/* Remove the ugly _t suffixes. Might not be needed in visual studio */
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

#endif
