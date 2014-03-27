#define _GNU_SOURCE 1
#include <time.h>
#include "microsec.h"

uint64 get_microsec( void )
{
	struct timespec now;
	
	if  ( clock_gettime( CLOCK_MONOTONIC, &now ) < 0 )
		return 0;
	
	return 
		(uint64) now.tv_sec * 1000000
		+ (uint64) now.tv_nsec / 1000;
}
