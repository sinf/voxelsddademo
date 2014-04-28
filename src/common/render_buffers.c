#include <stdlib.h>
#include <assert.h>
#include "render_buffers.h"

static void *all_buffers = NULL;

size_t
render_resx=0,
render_resy=0;

uint32
*render_output_write = NULL,
*render_output_rgba = NULL;

uint8 *render_output_m = NULL; /* materials */
float *render_output_z = NULL; /* ray depth (distance to first intersection) */

void swap_render_buffers( void )
{
	void *p = render_output_rgba;
	render_output_rgba = render_output_write;
	render_output_write = p;
}

int resize_render_buffers( size_t w, size_t h )
{
	size_t alloc_pixels, total_pixels, s[4];
	char *all_mem;
	
	assert( w % 16 == 0 );
	
	render_resx = w;
	render_resy = h;
	total_pixels = w * h;
	
	if ( all_buffers ) {
		free( all_buffers );
		all_buffers = NULL;
	}
	
	if ( total_pixels )
	{
		/* Allocate some extra pixels to avoid needing to check bounds in tight pixel processing loops */
		alloc_pixels = total_pixels + render_resx + 128;
		
		/* Pixel buffer sizes. Pad to 16 so that each buffer will be 16-aligned inside all_mem */
		s[0] = ( alloc_pixels * sizeof( render_output_m[0] ) + 0xF ) & ~0xF;
		s[1] = ( alloc_pixels * sizeof( render_output_z[0] ) + 0xF ) & ~0xF;
		s[2] = ( alloc_pixels * sizeof( render_output_write[0] ) + 0xF ) & ~0xF;
		s[3] = ( alloc_pixels * sizeof( render_output_rgba[0] ) + 0xF ) & ~0xF;
		
		/* Extra 16 in case the pointer needs to be adjusted to achieve alignment */
		all_mem = malloc( s[0] + s[1] + s[2] + s[3] + 16 );
		
		if ( all_mem )
		{
			/* This integer aligns all_mem to 16 bytes. Only the lowest 16 bits matter so (int) cast is ok */
			int off = (int) all_mem & 0xF;
			off *= !!off;
			
			all_buffers = all_mem;
			all_mem += off;
			
			assert( (int) all_mem % 16 == 0 );
			
			render_output_m = (void*) all_mem;
			render_output_z = (void*)( all_mem = all_mem + s[0] );
			render_output_write = (void*)( all_mem = all_mem + s[1] );
			render_output_rgba = (void*)( all_mem = all_mem + s[2] );
			
			return 1;
		}
	}
	
	render_output_m = NULL;
	render_output_z = NULL;
	render_output_write = NULL;
	render_output_rgba = NULL;
	
	return 0;
}
