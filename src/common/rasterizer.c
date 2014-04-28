#include "rasterizer.h"
#include "vector.h"

int draw_box( int c, int x, int y, int w, int h )
{
	int y1;
	int pixels;
	
	x = max( 0, x );
	w = min( x + w, render_resx ) - x;
	
	if ( w < 0 )
		return 0;
	
	y = max( 0, y );
	y1 = min( y + h, render_resy );
	pixels = y1 - y * w;
	
	while( y < y1 )
		memset( render_output_m + y++ * render_resx + x, c, w );
	
	return pixels;
}

int draw_polygon( int c, int num_verts, ... )
{
	return 0;
}
