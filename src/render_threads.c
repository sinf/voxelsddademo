#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "render_threads.h"
#include "render_core.h"
#include "threads.h"

typedef struct SlaveThreadParams
{
	int id; /* 0, 1, 2, 3, .. */
	Thread thread;
} SlaveThreadParams;

#define MAX_RENDER_THREADS 64
static SlaveThreadParams threads[MAX_RENDER_THREADS];
int num_render_threads = 0;

static Mutex render_state_mutex = MUTEX_INITIALIZER;
static Cond render_state_cond = COND_INITIALIZER;

/* *********************************************** */
/* All of this is associated with render_state_mutex */
static volatile enum {
	R_RENDER, /* Slave threads should re-render when current_frame_id changes */
	R_EXIT /* Slave threads should terminate */
} render_state = R_RENDER;

#define INITIAL_FRAME_ID 0
static volatile unsigned current_frame_id = INITIAL_FRAME_ID;
/* *********************************************** */

static Mutex finished_parts_mutex = MUTEX_INITIALIZER;
static Cond finished_parts_cond = COND_INITIALIZER;
static volatile int finished_parts = 0; /* Associated with finished_parts_mutex */

static void *render_thread_func( void *p )
{
	const SlaveThreadParams * const volatile self = p;
	unsigned my_old_frame_id = INITIAL_FRAME_ID;
	int running = 1;
	size_t start_row, end_row, ray_buffer_size;
	float *ray_buffer; /* temporary buffer for ray origins & directions */
	
	start_row = self->id * render_resy / num_render_threads;
	end_row = ( self->id + 1 ) * render_resy / num_render_threads;
	printf( "Thread %d: Initializing.. got scanlines %u ... %u\n", self->id, (unsigned) start_row, (unsigned) end_row );
	ray_buffer_size = 6 * sizeof( float ) * ( end_row - start_row ) * render_resx;
	ray_buffer = aligned_alloc( 16, ray_buffer_size );
	if ( !ray_buffer ) {
		printf( "Error: Failed to allocate ray buffer (%u KiB)\n", (unsigned)(ray_buffer_size>>10) );
		return NULL;
	}
	
	while( running )
	{
		unsigned my_current_frame_id;
		int my_render_state;
		
		mutex_lock( &render_state_mutex );
		{
			#if 0
			/* Wait for a signal from the master thread */
			cond_wait( &render_state_cond, &render_state_mutex );
			#endif
			
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
	
	printf( "Thread %d: Terminated\n", self->id );
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
	
	printf( "Stopping renderer threads...\n" );
	
	render_state = R_EXIT;
	mutex_unlock( &render_state_mutex );
	
	#if 0
	/* Request all threads to terminate */
	mutex_lock( &render_state_mutex );
	{
		render_state = R_EXIT;
		cond_broadcast( &render_state_cond );
	}
	mutex_unlock( &render_state_mutex );
	#endif
	
	/* Wait until all threads have terminated */
	for( n=0; n<num_render_threads; n++ )
		thread_join( threads[n].thread );
	
	printf( "Threads terminated as expected\n" );
	
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
		cond_init( &render_state_cond );
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
	
	printf( "Starting renderer threads... (%d)\n", count );
	
	num_render_threads = count;
	render_state = R_RENDER;
	current_frame_id = INITIAL_FRAME_ID;
	finished_parts = 0;
	
	mutex_lock( &render_state_mutex );
	
	for( n=0; n<num_render_threads; n++ ) {
		threads[n].id = n;
		thread_create( &threads[n].thread, render_thread_func, (void*)(threads+n) );
	}
}

void begin_volume_rendering( void )
{	
	/* No threads, can't render */
	if ( num_render_threads <= 0 )
		return;
	
	/* At least 1 slave thread */
	/** mutex_lock( &render_state_mutex ); **/
	{
		current_frame_id++;
		
		mutex_lock( &finished_parts_mutex );
		finished_parts = 0;
		
		/* Wake up the worker threads.
			Once they notice that current_frame_id
			has increased they will start rendering. */
		cond_broadcast( &render_state_cond );
	}
	mutex_unlock( &render_state_mutex );
}

void end_volume_rendering( void )
{
	/* This thread has already locked finished_parts_mutex */
	while( finished_parts < num_render_threads )
	{
		cond_wait( &finished_parts_cond, &finished_parts_mutex );
		/* Wait until threads are finished */
	}
	mutex_unlock( &finished_parts_mutex );
	
	mutex_lock( &render_state_mutex );
}
