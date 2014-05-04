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

/* Returns a mask where each bit tells, whether the corresponding vertex on a cube,
extending from (n,n,n) to (p,p,p), can be seen by an observer located at (x,y,z) */
int get_visible_cube_vertices( int x, int y, int z, int n, int p );

/* Verts contains (x,y) pairs. They need to be in counter-clockwise order */
int draw_polygon( uint32 c, int num_verts, float verts[] );

#endif
