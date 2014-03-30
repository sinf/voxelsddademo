#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "render_threads.h"
#include "render_core.h"
#include "threads.h"
#include "microsec.h"

typedef struct SlaveThreadParams
{
	int id; /* 0, 1, 2, 3, .. */
	Thread thread;
} SlaveThreadParams;

#define MAX_RENDER_THREADS 64
static SlaveThreadParams threads[MAX_RENDER_THREADS];
int num_render_threads = 0;

static Mutex render_state_mutex = MUTEX_INITIALIZER;

/* *********************************************** */
/* All of this is associated with render_state_mutex */
static volatile enum {
	R_RENDER, /* Slave threads should re-render when current_frame_id changes */
	R_EXIT /* Slave threads should terminate */
} render_state = R_RENDER;

#define INITIAL_FRAME_ID 0
typedef uint64 FrameID;
volatile FrameID current_frame_id = INITIAL_FRAME_ID;
/* *********************************************** */

static Mutex finished_parts_mutex = MUTEX_INITIALIZER;
static Cond finished_parts_cond = COND_INITIALIZER;
static volatile int finished_parts = 0; /* Associated with finished_parts_mutex */

static void *render_thread_func( void *p )
{
	const SlaveThreadParams * const volatile self = p;
	FrameID my_old_frame_id = INITIAL_FRAME_ID;
	int running = 1;
	size_t start_row, end_row, ray_buffer_size;
	float *ray_buffer; /* temporary buffer for ray origins & directions */
	
	start_row = self->id * render_resy / num_render_threads;
	end_row = ( self->id + 1 ) * render_resy / num_render_threads;
	ray_buffer_size = RENDER_THREAD_MEM_PER_PIXEL * ( end_row - start_row ) * render_resx;
	ray_buffer = aligned_alloc( 16, ray_buffer_size );
	if ( !ray_buffer ) {
		printf( "Error: Failed to allocate ray buffer (%u KiB)\n", (unsigned)(ray_buffer_size>>10) );
		return NULL;
	}
	
	while( running )
	{
		FrameID my_current_frame_id;
		int my_render_state;
		
		mutex_lock( &render_state_mutex );
		{
			/* Get local copies of the global variables */
			my_render_state = render_state;
			my_current_frame_id = current_frame_id;
		}
		mutex_unlock( &render_state_mutex );
		
		switch( my_render_state )
		{
			case R_RENDER:
				/* Compare last rendered frame ID with the current frame ID.
					We don't want to render the same thing twice. */
				if ( my_old_frame_id != my_current_frame_id )
				{
					/* Do some heavy number crunching, recursion and memory I/O */
					render_part( start_row, end_row, ray_buffer );
					
					/* Job finished - notify main thread */
					mutex_lock( &finished_parts_mutex );
					{
						finished_parts += 1;
						cond_signal( &finished_parts_cond );
					}
					mutex_unlock( &finished_parts_mutex );
					
					/* Remember that this frame has been rendered */
					my_old_frame_id = my_current_frame_id;
				}
				break;
			
			case R_EXIT:
				running = 0;
				break;
			
			default:
				/* Spurious wakeup */
				break;
		}
	}
	
	free( ray_buffer );
	return NULL;
}

void stop_render_threads( void )
{
	int n;
	
	if ( num_render_threads <= 0 ) {
		printf( "Note: tried to stop renderer threads but none were running\n" );
		return;
	}
	
	render_state = R_EXIT;
	mutex_unlock( &render_state_mutex );
	
	/* Wait until all threads have terminated */
	for( n=0; n<num_render_threads; n++ )
		thread_join( threads[n].thread );
	
	num_render_threads = 0;
	memset( (void*) threads, 0, sizeof(threads) );
}

void start_render_threads( int count )
{	
	int n;
	
	#ifdef NEED_EXPLICIT_MUTEX_INIT
	static int has_init = 0;
	if ( !has_init ) {
		mutex_init( &render_state_mutex );
		mutex_init( &finished_parts_mutex );
		cond_init( &finished_parts_cond );
		has_init = 1;
	}
	#endif
	
	if ( count <= 0 )
		return;
	
	if ( num_render_threads > 0 ) {
		/* Clean up the old threads before creating new ones */
		stop_render_threads();
	}
	
	num_render_threads = count;
	render_state = R_RENDER;
	current_frame_id = INITIAL_FRAME_ID;
	finished_parts = 0;
	
	/* Keep this mutex hostage whenever workers aren't supposed to do anything */
	mutex_lock( &render_state_mutex );
	
	for( n=0; n<num_render_threads; n++ ) {
		threads[n].id = n;
		thread_create( &threads[n].thread, render_thread_func, (void*)(threads+n) );
	}
}

static uint64 frame_start_time = 0;
void begin_volume_rendering( void )
{
	/* No threads, can't render */
	if ( num_render_threads <= 0 )
		return;
	
	mutex_lock( &finished_parts_mutex );
	current_frame_id++;
	finished_parts = 0;
	
	frame_start_time = get_microsec();
	
	/* Release the hostage */
	mutex_unlock( &render_state_mutex );
}

void end_volume_rendering( RayPerfInfo info[1] )
{
	if ( num_render_threads <= 0 )
		return;
	
	/* This thread has already locked finished_parts_mutex */
	while( finished_parts < num_render_threads )
	{
		cond_wait( &finished_parts_cond, &finished_parts_mutex );
		/* Wait until threads are finished */
	}
	mutex_unlock( &finished_parts_mutex );
	
	/* Freeze workers */
	mutex_lock( &render_state_mutex );
	
	if ( info )
	{
		uint64_t t = get_microsec();
		info->frame_time = t > frame_start_time ? ( t - frame_start_time ) : 0;
		info->rays_per_frame = render_resx * render_resy << enable_shadows;
		info->rays_per_sec = info->frame_time ? ( 1000000 * info->rays_per_frame + 500000 ) / info->frame_time : 0;
	}
}
