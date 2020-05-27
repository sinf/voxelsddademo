#include <SDL.h>
#include "render_buffers.h"
#include "render_core.h"
#include "voxels.h"
#include "camera.h"

static void transform_vec( float c[4], const float a[16], const float b[4] )
{
	#if 1
	c[0] = a[0]*b[0] + a[4]*b[1] + a[8]*b[2] + a[12]*b[3];
	c[1] = a[1]*b[0] + a[5]*b[1] + a[9]*b[2] + a[13]*b[3];
	c[2] = a[2]*b[0] + a[6]*b[1] + a[10]*b[2] + a[14]*b[3];
	c[3] = a[3]*b[0] + a[7]*b[1] + a[11]*b[2] + a[15]*b[3];
	#else
	c[0] = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
	c[1] = a[4]*b[0] + a[5]*b[1] + a[6]*b[2] + a[7]*b[3];
	c[2] = a[8]*b[0] + a[9]*b[1] + a[10]*b[2] + a[11]*b[3];
	c[3] = a[12]*b[0] + a[13]*b[1] + a[14]*b[2] + a[15]*b[3];
	#endif
}

#define ENABLE_DEPTH_BUFFER 1
#if ENABLE_DEPTH_BUFFER
static void write_depth( uint32 *pixel_buf, int x0, int y0, int r, float z, uint32 color )
{
	int x1 = x0 + r;
	int y1 = y0 + r;
	int y, x;
	
	x0 = max( x0, 0 );
	y0 = max( y0, 0 );
	x1 = min( x1, (int) render_resx );
	y1 = min( y1, (int) render_resy );
	
	for( y=y0; y<y1; y++ ) {
		for( x=x0; x<x1; x++ ) {
			float *p = render_output_z + y * render_resx + x;
			if ( z < *p ) {
				*p = z;
				pixel_buf[ y * render_resx + x ] = color;
			}
		}
	}
}
static int scan_depth( int x0, int y0, int r, float z )
{
	int x1 = x0 + r;
	int y1 = y0 + r;
	int y, x;
	
	x0 = max( x0, 0 );
	y0 = max( y0, 0 );
	x1 = min( x1, (int) render_resx );
	y1 = min( y1, (int) render_resy );
	
	for( y=y0; y<y1; y++ ) {
		for( x=x0; x<x1; x++ ) {
			if ( z < render_output_z[ y * render_resx + x ] )
				return 1;
		}
	}
	
	return 0;
}
#endif

static void rasterize_octree1( OctreeNode *node, int x0, int y0, int z0, int recur_m, int depth, float cam_x, float cam_y, float cam_z, float cot_fov_per_2, const float mvp[16], SDL_Surface *screen )
{
	float w[4];
	float s[4];
	float px, py, z;
	float r;
	float h = 1 << depth >> 1;
	
	w[0] = cam_x - x0 - h;
	w[1] = cam_y - y0 - h;
	w[2] = cam_z - z0 - h;
	w[3] = 1;
	
	transform_vec( s, mvp, w );
	
	px = screen->w / 2 * ( 1.0f + s[0] / s[3] );
	py = screen->h / 2 * ( 1.0f + s[1] / s[3] );
	z = s[2]; /** / s[3]; **/
	r = ( 1 << depth ) * cot_fov_per_2 / z;
	
	/*
	r = h / s[2] * .25;
	r = r > 500 ? 500 : ( r < 1 ? 1 : r );
	*/
	
	if ( depth > 3 || node->children ) {
		int c;
		
		#if ENABLE_DEPTH_BUFFER
		if ( !scan_depth( px, py, r, z ) )
			return; /* all pixels would be occluded anyway so no point proceeding further */
		#endif
		
		--depth;
		for( c=0; c<8; c++ ) {
			int cm = c ^ recur_m;
			rasterize_octree1( node->children ? ( node->children + cm ) : node,
				x0 + ( cm >> 2 << depth ),
				y0 + ( ( cm >> 1 & 1 ) << depth ),
				z0 + ( ( cm & 1 ) << depth ),
				recur_m, depth,
				cam_x, cam_y, cam_z,
				cot_fov_per_2,
				mvp, screen );
		}
	}
	else if ( node->mat )
	{
		uint32 color = materials_rgb[node->mat];
		SDL_Rect rc;
		
		if ( z < 0 )
			return; /* is behind the camera */
		
		#if ENABLE_DEPTH_BUFFER
		write_depth( screen->pixels, px, py, r, z, color );
		#else
		rc.x = px - r;
		rc.y = py - r;
		rc.w = rc.h = r;
		SDL_FillRect( screen, &rc, color );
		#endif
	}
}

void rasterize_octree( struct Octree *tree, struct Camera *camera, struct SDL_Surface *screen )
{
	float s = 1 << tree->root_level;
	
	int recur_m =
	( camera->eye_to_world[6] < 0 ) << 2
	| ( camera->eye_to_world[7] < 0 ) << 1
	| ( -camera->eye_to_world[8] < 0 );
	
	size_t n, a;
	a = render_resy * render_resx;
	for( n=0; n<a; n++ )
		render_output_z[n] = 10000000;
	
	rasterize_octree1( &tree->root, 0, 0, 0, recur_m, tree->root_level,
		s * camera->pos[0], s * camera->pos[1], s * camera->pos[2],
		acos( camera->fovy / 2.0f ),
		camera->mvp, screen );
}
