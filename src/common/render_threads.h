#ifndef _RENDER_THREADS_H
#define _RENDER_THREADS_H
#include "types.h"

typedef struct {
	uint64 frame_time; /* microseconds */
	uint64 rays_per_frame;
	uint64 rays_per_sec;
} RayPerfInfo;

extern int num_render_threads;

/* Starts N threads that are used for rendering.
	Any previous threads will be stopped and cleaned up.
	Note: A single renderer thread is created when N=1. No threads are created when N<=0 */
void start_render_threads( int N );

/* Does nothing if no threads are running. Stalls until all threads are dead */
void stop_render_threads( void );

void begin_volume_rendering( void ); /* signals the worker threads to begin rendering a frame */
void end_volume_rendering( RayPerfInfo info[1] ); /* waits and returns only when the entire frame has been rendered. if info!=NULL then performance data is written there */

#endif
