#ifndef _RASTERIZER_H
#define _RASTERIZER_H

/*
- These functions use the "skip buffer" to prevent overwriting previously written pixels
- Pixels are written to render_output_m
- Return value is the number of written pixels
- c is color index
*/

int draw_box( int c, int x, int y, int w, int h );
int draw_polygon( int c, int num_verts, ... ); /* num_verts is followed by 2*num_verts floats that specify vertex coordinates */

#endif
