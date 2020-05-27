#include <stdlib.h>
#include <assert.h>
#include "rasterizer.h"
#include "vector.h"
#include "render_buffers.h"

#define OUT_BUFFER render_output_rgba

static void memset32( void *p, uint32 c, int len ) {
	uint32 *u = p;
#if 0
	assert( len >= 0 );
	while( len-- ) *u++ = c;
#elif 1
	while( len-- ) u[len] ^= c;
#else
	/* wireframe */
	if ( len > 0 ) {
		u[0] = c;
		u[len-1] = c;
		if ( len > 1 ) {
			u[1] = c;
			u[len-2] = c;
		}
	}
#endif
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

int draw_polygon( uint32 color, int num_verts, float fverts[] )
{
	typedef struct { float x, y; } Vertex;
	Vertex *verts, *nw, *ne, *sw, *se;
	int top;
	int dest_y;
	float slope_w, slope_e;
	
	if ( num_verts <= 0 )
		return 0;
	
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
		
		dest_y = max( dest_y, 0 );
		y1 = max( y1, 0 );
		y1 = min( y1, render_resy );
		
		while( dest_y < y1 ) {
			int x0_, x1_, x0, x1;
			
			x0_ = nw->x + slope_w * ( dest_y - nw->y );
			x1_ = ne->x + slope_e * ( dest_y - ne->y );
			
			x0_ = max( x0_, 0 );
			x1_ = min( x1_, (int) render_resx );
			
		#if 0
			/* allow vertices to be specified in either clockwise or counter-clockwise order */
			x0 = min( x0_, x1_ );
			x1 = max( x0_, x1_ );
		#else
			/* vertices MUST be specified in counter-clockwise order (otherwise nothing will be drawn) */
			x0 = x0_;
			x1 = x1_;
			if ( x0 < x1 )
		#endif
			
			memset32( OUT_BUFFER + dest_y * render_resx + x0, color, x1 - x0 );
			
			++dest_y;
		}
		
		if ( y1 == render_resy )
			break;
		
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

uint32 get_cube_silhouette( int n, int p, int x, int y, int z )
{
	/* Hand-crafted table of polygons. Array index is the sector of eye position
		bit 0-2: vertex count
		bits 3-5: index of the first vertex
		bits 6-8: index of the 2nd vertex
		bits 9-11: index of the 3rd vertex
		etc..
	Integer 0 is special and means "no polygon"
	*/
	static const uint32 sil[] = {
#if 1
	#define QUAD(a,b,c,d) ((d)<<12|(c)<<9|(b)<<6|(a)<<3|4)
	#define HEX(a,b,c,d,e,f) ((f)<<18|(e)<<15|QUAD(a,b,c,d)|2)
#else
	#define QUAD(d,c,b,a) ((d)<<12|(c)<<9|(b)<<6|(a)<<3|4)
	#define HEX(f,e,d,c,b,a) ((f)<<18|(e)<<15|QUAD(a,b,c,d)|2)
#endif
	#define DUMMY 0
		/* x0 y0 */
		HEX(2,3,1,5,4,6), /* z0 */
		HEX(2,3,1,5,4,0), /* z1 */
		HEX(2,3,7,5,4,0), /* z2 */
		DUMMY,
		/* x0 y1 */
		HEX(2,3,1,0,4,6),
		QUAD(2,3,1,0),
		HEX(2,3,7,5,1,0),
		DUMMY,
		/* x0 y2 */
		HEX(7,3,1,0,4,6),
		HEX(7,3,1,0,2,6),
		HEX(7,5,1,0,2,6),
		DUMMY,
		/* x0 y3 */
		DUMMY,DUMMY,DUMMY,DUMMY,
		
		/* x1 y0 */
		HEX(2,0,1,5,4,6),
		QUAD(0,1,5,4),
		HEX(3,7,5,4,0,1),
		DUMMY,
		/* x1 y1 */
		QUAD(2,0,4,6),
		DUMMY, /* inside the cube */
		QUAD(3,7,5,1),
		DUMMY,
		/* x1 y2 */
		HEX(3,2,0,4,6,7),
		QUAD(3,2,6,7),
		HEX(3,2,6,7,5,1),
		DUMMY,
		/* x1 y3 */
		DUMMY,DUMMY,DUMMY,DUMMY,
		
		/* x2 y0 */
		HEX(6,2,0,1,5,7),
		HEX(6,4,0,1,5,7),
		HEX(6,4,0,1,3,7),
		DUMMY,
		/* x2 y1 */
		HEX(6,2,0,4,5,7),
		QUAD(6,4,5,7),
		HEX(6,4,5,1,3,7),
		DUMMY,
		/* x2 y2 */
		HEX(3,2,0,4,5,7),
		HEX(3,2,6,4,5,7),
		HEX(3,2,6,4,5,1)
	#undef QUAD
	#undef HEX
	#undef DUMMY
	};
	
	int sec_x, sec_y, sec_z, sector;
	int check[ sizeof( sil ) / sizeof( sil[0] ) >= 42 ];
	int check2[ sizeof( sil[0] ) * 8 >= 21 ];
	
	(void) check;
	(void) check2;
	
	/* sectors:
	  n   p
	0 | 1 | 2
	coords grow that way ->
	*/
	sec_x = ( x >= n ) + ( x >= p );
	sec_y = ( y >= n ) + ( y >= p );
	sec_z = ( z >= n ) + ( z >= p );
	sector = sec_x << 4 | sec_y << 2 | sec_z;
	
	return sil[sector];
}
