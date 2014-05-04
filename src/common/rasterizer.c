#include <stdlib.h>
#include <assert.h>
#include "rasterizer.h"
#include "vector.h"
#include "render_buffers.h"

#define OUT_BUFFER render_output_rgba

static void memset32( void *p, uint32 c, int len ) {
	uint32 *u = p;
	assert( len >= 0 );
	while( len-- ) *u++ = c;
}

int draw_box( uint32 c, int x, int y, int w, int h )
{
	int x0, y0, x1, y1;
	
	x0 = max( x, 0 );
	y0 = max( y, 0 );
	x1 = min( (long) x + w, (long) render_resx );
	y1 = min( (long) y + h, (long) render_resy );
	
	w = x1 - x0;
	h = y1 - y0;
	
	if ( w <= 0 )
		return 0;
	
	for( y=y0; y<y1; y++ )
		memset32( OUT_BUFFER + y * render_resx + x0, c, w );
	
	return w * h;
}

static int find_highest_vertex( int num, float verts[] )
{
	float top = verts[1];
	int n, top_n = 0;
	
	for( n=1; n<num; n++ ) {
		float y = verts[2*n+1];
		if ( y < top ) {
			top = y;
			top_n = n;
		}
	}
	
	return top_n;
}

/* Returns a mask where each bit tells, whether the corresponding vertex on a cube,
extending from (n,n,n) to (p,p,p), can be seen by an observer located at (x,y,z) */
int get_visible_cube_vertices( int x, int y, int z, int n, int p )
{
	int vis = 0;
	if ( x > n ) {
		if ( y > n ) {
			vis |= z > n; /* x0,y0,z0 */
			vis |= z < p << 1; /* x0,y0,z1 */
		}
		if ( y < p ) {
			vis |= z > n << 2; /* x0,y1,z0 */
			vis |= z < p << 3; /* x0,y1,z1 */
		}
	}
	if ( x < p ) {
		if ( y > n ) {
			vis |= z > n << 4; /* x1,y0,z0 */
			vis |= z < p << 5; /* x1,y0,z1 */
		}
		if ( y < p ) {
			vis |= z > n << 6; /* x1,y1,z0 */
			vis |= z < p << 7; /* x1,y1,z1 */
		}
	}
	return vis;
}

int draw_polygon( uint32 color, int num_verts, float fverts[] )
{
	typedef struct { float x, y; } Vertex;
	Vertex *verts, *nw, *ne, *sw, *se;
	int top;
	int dest_y;
	float slope_w, slope_e;
	
	verts = (Vertex*) fverts;
	top = find_highest_vertex( num_verts, fverts );
	
	nw = ne = verts + top;
	sw = verts + ( top + 1 ) % num_verts;
	se = verts + ( top - 1 + num_verts ) % num_verts;
	
	slope_w = ( sw->x - nw->x ) / ( sw->y - nw->y );
	slope_e = ( se->x - ne->x ) / ( se->y - ne->y );
	
	dest_y = nw->y;
	
	do {
		int y1;
		
		y1 = min( sw->y, se->y );
		
		while( dest_y < y1 ) {
			int x0_, x1_, x0, x1;
			
			x0_ = nw->x + slope_w * ( dest_y - nw->y );
			x1_ = ne->x + slope_e * ( dest_y - ne->y );
			
			x0_ = max( x0_, 0 );
			x1_ = min( x1_, (int) render_resx );
			
			x0 = min( x0_, x1_ ); /* allow vertices to be specified in either clockwise or counter-clockwise order */
			x1 = max( x0_, x1_ );
			
			memset32( OUT_BUFFER + dest_y * render_resx + x0, color, x1 - x0 );
			
			++dest_y;
		}
		
		/* Change either the west or the east edge */
		if ( sw->y < se->y ) {
			nw = sw;
			sw = verts + ( sw - verts + 1 ) % num_verts; /* rotate in counter-clockwise direction */
			slope_w = ( sw->x - nw->x ) / ( sw->y - nw->y );
		} else {
			ne = se;
			se = verts + ( se - verts - 1 + num_verts ) % num_verts; /* clockwise */
			slope_e = ( se->x - ne->x ) / ( se->y - ne->y );
		}
	} while( ne != nw );
	
	return 0;
}
