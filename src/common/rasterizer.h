#ifndef _RASTERIZER_H
#define _RASTERIZER_H
#include "types.h"

/*
- These functions use the "skip buffer" to prevent overwriting previously written pixels
- Pixels are written to render_output_m
- Return value is the number of written pixels
- All coordinates are in screen space
- c is color index
*/

int draw_box( uint32 c, int x, int y, int w, int h );

/* Verts contains (x,y) pairs. They need to be in counter-clockwise order */
int draw_polygon( uint32 c, int num_verts, float verts[] );

/* Bits of the return value:
	bit 0-2: vertex count
	bits 3-5: index of the first vertex
	bits 6-8: index of the 2nd vertex
	bits 9-11: index of the 3rd vertex
	etc..
Integer 0 is special and means "no polygon"
*/
uint32 get_cube_silhouette( int cube_min, int cube_max, int eye_x, int eye_y, int eye_z );

#endif
