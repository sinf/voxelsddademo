#ifndef _GOOD_TIME_H
#define _GOOD_TIME_H
#include "types.h"
/* Returns current time in microseconds.
The returned time might not have a meaningful base (such as POSIX Epoch, start of program, etc).
Only useful for measuring elapsed time */
uint64 get_microsec( void );
#endif
