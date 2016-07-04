#include <stdlib.h>
#include <signal.h>
#include "SDL.h"
#include <xmmintrin.h>

#include "types.h"
#include "vector.h"
#include "ray.h"
#include "aabb.h"

#include "voxels.h"
#include "voxels_io.h"
#include "voxels_csg.h"
#include "city.h"

#include "camera.h"
#include "render_buffers.h"
#include "render_core.h"
#include "render_threads.h"
#include "text.h"
#include "graph.h"
#include "rasterizer.h"
#include "world_gen.h"
#include "microsec.h"

#include "oc_rasterizer.h"

#define DEFAULT_RESX 800
#define DEFAULT_RESY 600
#define DEFAULT_OCTREE_DEPTH 9
#define DEFAULT_THREADS 6

#define DEFAULT_FOV radians(65)
#define FOV_INCR radians(5)

#define BRUSH_RADIUS_INCR 5
#define BRUSH_DEFAULT_RADIUS 1.4
#define BRUSH_DEFAULT_MAT 15

#define SHOW_HELP 0

static int brush_mat = BRUSH_DEFAULT_MAT;
static float brush_radius = BRUSH_DEFAULT_RADIUS;

static int mouse_x = 0;
static int mouse_y = 0;

static SDL_Surface *screen = NULL;
static int benchmark_mode = 0;
static int upscale_shift = 0;

static float light_a1 = 0;
static float light_a2 = 0;
static float light_r = 1;
static int moving_light = 0;

static Octree *the_volume = NULL;
static Camera the_camera;

static void get_light_pos( float p[3] )
{
	float x, y, z;
	float t = 1000;
	
	light_a1 = fmod( light_a1, 2*M_PI );
	light_a2 = fmod( light_a2, 2*M_PI );
	
	x = cos( light_a1 ) * sin( light_a2 ) * light_r * t;
	z = sin( light_a1 ) * sin( light_a2 ) * light_r * t;
	y = cos( light_a2 ) * light_r * t;
	
	x += light_r/2;
	z += light_r/2;
	
	p[0] = x;
	p[1] = y;
	p[2] = z;
}

static void update_light_pos( void )
{
	float p[3];
	get_light_pos( p );
	set_light_pos( p[0], p[1], p[2] );
}

static void quit( /* any number of arguments */ )
{
	stop_render_threads();
	SDL_Quit();
	exit(0);
}

static void resize( int w, int h, int extra_flags )
{
	int flags;
	
	w &= ~0xF;
	
	flags = SDL_SWSURFACE;
	flags |= SDL_RESIZABLE;
	flags |= extra_flags;
	screen = SDL_SetVideoMode( w, h, 32, flags );
	
	if ( !screen )
	{
		printf( "Failed to set %dx%d video mode, reason: %s\n", w, h, SDL_GetError() );
		exit(0);
	}
	
	resize_render_output( w >> upscale_shift, h >> upscale_shift );
}

static void setup_test_scene( Octree *volume )
{	
	const float size = volume->size;
	aabb3f box;
	int n;
	
	#if 1
	oc_clear( the_volume, 0 );
	generate_city( volume );
	#else
	generate_world( volume );
	#endif
	
	/* Add colored axes */
	for( n=0; n<3; n++ )
	{
		box.min[0] = box.min[1] = box.min[2] = 0;
		box.max[0] = box.max[1] = box.max[2] = size/32;
		box.max[n] = size/2;
		csg_box( volume, &box, 2+n );
	}
	
	/* Add a white corner piece where the 3 axes meet */
	box.min[0] = box.min[1] = box.min[2] = 0;
	box.max[0] = box.max[1] = box.max[2] = size/32;
	csg_box( volume, &box, 1 );
}

static void reset_camera( void )
{
	the_camera.pos[0] = 0.5f;
	the_camera.pos[1] = 0.5f;
	the_camera.pos[2] = -1.0f;
	
	the_camera.yaw = 0.0f;
	the_camera.pitch = 0.0f;
	
	set_projection( &the_camera, DEFAULT_FOV, screen->w / (float) screen->h );
	update_camera_matrix( &the_camera );
}

static void shoot( int win_x, int win_y, int m )
{
	uint8 mat = 0;
	int x, y;
	float depth;
	Ray ray;
	
	x = win_x / (float) screen->w * render_resx;
	y = win_y / (float) screen->h * render_resy;
	
	get_primary_ray( &ray, &the_camera, the_volume, x, y );
	depth = oc_traverse( the_volume, &mat, ray.o[0], ray.o[1], ray.o[2], ray.d[0], ray.d[1], ray.d[2], NAN );
	
	if ( mat != 0 )
	{
		Sphere sph;
		int n;
		
		sph.r = brush_radius * ( 1 << the_volume->root_level ) / 512.0;
		for( n=0; n<3; n++ )
		{
			sph.o[n] = ray.o[n] + ray.d[n] * depth;
			sph.o[n] = clamp( sph.o[n], 0, the_volume->size );
		}
		
		#if 0
		printf( "sph o=(%f,%f,%f)\no=(%f,%f,%f) d=(%f,%f,%f) z=%f\n", sph.o[0], sph.o[1], sph.o[2],
			ray.o[0], ray.o[1], ray.o[2], ray.d[0], ray.d[1], ray.d[2], depth );
		#endif
		
		csg_sphere( the_volume, &sph, m );
	}
}

static int hook_mouse = 0;
void process_input( float timestep, int screen_centre_x, int screen_centre_y, Camera *camera )
{
	float speed = 1.0f * timestep;
	vec3f motion = {0.0f, 0.0f, 0.0f};
	uint8 *keys, buttons;
	
	int should_warp_mouse = 0;
	vec2i mouse_motion;
	
	SDL_PumpEvents();
	keys = SDL_GetKeyState( NULL );
	buttons = SDL_GetMouseState( NULL, NULL );
	mouse_motion[0] = screen_centre_x - mouse_x;
	mouse_motion[1] = screen_centre_y - mouse_y;
	
	if ( buttons & SDL_BUTTON(2) )
	{
		/* MMB: fine zoom */
		
		float x = mouse_motion[0];
		float y = mouse_motion[1];
		float fine_zoom = (float) sqrt( x*x + y*y ) * 0.05f * timestep;
		
		if ( SDL_GetModState() & KMOD_LCTRL )
			fine_zoom *= 0.01;
		
		if ( y < 0 )
			fine_zoom = -fine_zoom;
		
		if ( moving_light ) {
			light_r += fine_zoom;
			update_light_pos();
		} else {
			motion[2] += fine_zoom;
		}
		
		should_warp_mouse = 1;
	}
	else if ( hook_mouse )
	{
		/* Mouse look */
		float sens = -0.015;
		float x, y;
		
		if ( keys[SDLK_LCTRL] )
			sens *= 0.2f;
		
		x = mouse_motion[0] * sens;
		y = mouse_motion[1] * sens;
		
		if ( moving_light ) {
			light_a1 += x;
			light_a2 += y;
			update_light_pos();
		}
		else
			rotate_camera( camera, x, y );
		
		should_warp_mouse = 1;
	}
	
	if ( should_warp_mouse )
	{
		SDL_WarpMouse( screen_centre_x, screen_centre_y );
		mouse_x = screen_centre_x;
		mouse_y = screen_centre_y;
	}
	
	if ( buttons & SDL_BUTTON(1) )
		shoot( mouse_x, mouse_y, brush_mat );
	else if ( buttons & SDL_BUTTON(3) )
		shoot( mouse_x, mouse_y, 0 );
	
	if ( keys[SDLK_LSHIFT] )
		speed *= 5.f;
	if ( keys[SDLK_LCTRL] )
		speed *= 0.2f;
	
	if ( keys[SDLK_a] ) motion[0] -= speed;
	if ( keys[SDLK_d] ) motion[0] += speed;
	if ( keys[SDLK_s] ) motion[2] -= speed;
	if ( keys[SDLK_w] ) motion[2] += speed;
	if ( keys[SDLK_e] ) motion[1] += speed;
	if ( keys[SDLK_q] ) motion[1] -= speed;
	
	update_camera_matrix( camera );
	move_camera_local( camera, motion );
}

static void expand_matrix( float b[16], const float a[9] )
{
	memcpy( b, a, 3 * sizeof *a );
	memcpy( b+4, a+3, 3 * sizeof *a );
	memcpy( b+8, a+6, 3 * sizeof *a );
	b[3] = b[7] = b[11] = b[15] = 0;
}

static void transpose_4x4( float out[16], const float in[16] )
{
	__m128 a, b, c, d;
	a = _mm_load_ps( in );
	b = _mm_load_ps( in + 4 );
	c = _mm_load_ps( in + 8 );
	d = _mm_load_ps( in + 12 );
	_MM_TRANSPOSE4_PS( a, b, c, d );
	_mm_store_ps( out, a );
	_mm_store_ps( out + 4, b );
	_mm_store_ps( out + 8, c );
	_mm_store_ps( out + 12, d );
}

static void make_frustum( float m[16], float r, float t, float n, float f )
{
	memset( m, 0, sizeof(float)*16 );
	m[0] = 1 / r;
	m[5] = 1 / t;
	m[10] = -2 / ( f - n );
	m[11] = ( n - f ) / ( f - n );
}
/*
0 1 2 3
4 5 6 7
8 9 10 11
12 13 14 15
*/

static void mult_mat4( float c[16], const float a[16], const float b[16] )
{
	int i, j, k;
	for( i=0; i<4; i++ ) {
		for( j=0; j<4; j++ ) {
			float d = 0;
			for( k=0; k<4; k++ ) {
				d += a[4*i+k] * b[j+4*k];
			}
			c[4*i+j] = d;
		}
	}
}

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

static uint32 gather_vertices( float dst[6*2], const float src[8*2], uint32 bits )
{
	uint32 cc, c;
	cc = c = bits & 0x7;
	bits >>= 3;
	while( c-- ) {
		int n = bits & 0x7;
		dst[0] = src[2*n];
		dst[1] = src[2*n+1];
		dst += 2;
		bits >>= 3;
	}
	return cc;
}

static void draw_ui_overlay( SDL_Surface *surf, Camera *camera )
{
	float verts[8*2];
	float sil[6*2];
	int p;
	uint32 vt;
	
	for( p=0; p<8; p++ )
	{
		const float k = 1;
		int x_bit, y_bit, z_bit;
		float w[4];
		float s[4];
		int px, py;
		
		/* get_light_pos( w ); */
		
		x_bit = p >> 2;
		y_bit = p >> 1 & 1;
		z_bit = p & 1;
		
		w[0] = k * x_bit;
		w[1] = k * y_bit;
		w[2] = k * z_bit;
		w[3] = 1;
		
		w[0] = -( w[0] - camera->pos[0] );
		w[1] = -( w[1] - camera->pos[1] );
		w[2] = -( w[2] - camera->pos[2] );
		
		transform_vec( s, camera->mvp, w );
		
		px = surf->w / 2 * ( 1.0f + s[0] / s[3] );
		py = surf->h / 2 * ( 1.0f + s[1] / s[3] );
		
		verts[2*p] = px;
		verts[2*p+1] = py;
		
		/*
		c = SDL_MapRGB( surf->format, 127 + x_bit * 128, 127 + y_bit * 128, 127 + z_bit * 128 );
		SDL_FillRect( surf, &r, c );
		*/
	}
	
	if ( 0 )
		return;
	
	vt = get_cube_silhouette( 1, 2, camera->pos[0]+1, camera->pos[1]+1, camera->pos[2]+1 );
	draw_polygon( 0xFF0000, gather_vertices( sil, verts, vt ), sil );
}

static void draw_text_overlay( SDL_Surface *surf, RayPerfInfo perf, Camera *camera )
{
	static Graph graph = {
		{0,0,MAX_GRAPH_W,80},
		{255,255,255},{255,0,0},
		1000000,1,
		0,0,0,0,{0}
	};
	static Graph graph2 = {
		{0,0,MAX_GRAPH_W,80},
		{255,255,255},{0,255,0},
		1000,0,
		0,0,0,0,{0}
	};
	char buf[256];
	
	if ( surf->w < 200 || surf->h < 200 )
		return;
	
	#if SHOW_HELP
	draw_text( surf, 0, 0,
		"Escape: quit\n"
		"Q,W,E,A,S,D: Move camera\n"
		"Space: Capture mouse\n"
		"F1: Save\n"
		"F2: Load\n"
		"F3: Reset view\n"
		"F4: Reset terrain\n"
		"F5: Fullscreen\n"
		"F6: Render mode\n"
		"F7,F8: LOD\n"
		"F9,F10: FOV\n"
		"F11: Shadows\n" );
	#endif
	
	draw_text_f( surf,
		0, SHOW_HELP ? ( surf->h - 4*GLYPH_H ) : 0,
		"Vol.s: %d^3\n"
		"Depth: %d/%d\n"
		"Nodes: %u\n"
		"Mat=%d\n"
		"DAC=%d\n"
		"(%.2f,%.2f,%.2f)"
		"(%.2f,%.2f,%.2f)"
		,
		(int) the_volume->size,
		(int) the_volume->root_level - oc_detail_level,
		(int) the_volume->root_level,
		the_volume->num_nodes,
		brush_mat,
		enable_dac_method,
		camera->pos[0],
		camera->pos[1],
		camera->pos[2],
		camera->eye_to_world[6],
		camera->eye_to_world[7],
		camera->eye_to_world[8]
		);
	
	snprintf( buf, sizeof(buf), "%ux%u|%4u ms|%u K rays|%3u.%03u M rays/sec ",
		(unsigned) render_resx, (unsigned) render_resy,
		(unsigned)( ( perf.frame_time + 500 ) / 1000 ),
		(unsigned)( ( perf.rays_per_frame + 500 ) / 1000 ),
		(unsigned)( perf.rays_per_sec / 1000000 ), (unsigned)( ( perf.rays_per_sec + 500 ) / 1000 % 1000 ) );
	
	draw_text( surf, surf->w - strlen(buf) * GLYPH_W, surf->h - GLYPH_H, buf );
	
	graph.bounds.x = surf->w - graph.bounds.w - 3;
	graph.bounds.y = 50;
	
	graph2.bounds.x = surf->w - graph2.bounds.w - 3;
	graph2.bounds.y = graph.bounds.y + graph.bounds.h + 60;
	
	update_graph( &graph, perf.rays_per_sec );
	update_graph( &graph2, perf.frame_time );
	draw_graph( &graph, surf );
	draw_graph( &graph2, surf );
	
	draw_text( surf, graph.bounds.x - 5 * GLYPH_W, graph.bounds.y - 2*GLYPH_H, "M Rays/sec" );
	draw_text( surf, graph2.bounds.x - 3 * GLYPH_W, graph2.bounds.y - 2*GLYPH_H, "ms/frame" );
	
	draw_text_f( surf, graph.bounds.x - 2 * GLYPH_W, graph.bounds.y + graph.bounds.h + 5,
		"Avg: %-d", (unsigned)( ( graph.total / graph.bounds.w + 500000 )/ 1000000 ) );
	
	draw_text_f( surf, graph2.bounds.x - 2 * GLYPH_W, graph2.bounds.y + graph2.bounds.h + 5,
		"Avg: %-d\n"
		"FPS: %-3d\n"
		, (unsigned)( ( graph2.total / graph2.bounds.w + 500 ) / 1000 ),
		perf.frame_time ? (int)( 1000000 / perf.frame_time ) : 999 );
}

static void load_materials( SDL_PixelFormat *format )
{
	const float gamma = ENABLE_GAMMA_CORRECTION ? THE_GAMMA_VALUE : 1;
	SDL_Surface *s;
	int m;
	
	printf( "Loading materials...\n" );
	s = SDL_LoadBMP( "data/materials.bmp" );
	if ( !s )
		return;
	
	SDL_LockSurface( s );
	
	/* todo: rewrite this function
	this will corrupt data and/or segfault if the surface fails to load,
	or if there isn't enough pixels, or if the pixel format is incompatible etc... */
	
	for( m=0; m<64; m++ )
	{
		uint32 pixel = *(uint32*)( (uint8*) s->pixels + s->format->BytesPerPixel * m );
		const unsigned b = pixel & 0xFF;
		const unsigned g = pixel >> 8 & 0xFF;
		const unsigned r = pixel >> 16 & 0xFF;
		
		materials_diff[m][0] = pow( r / 255.0f, gamma );
		materials_diff[m][1] = pow( g / 255.0f, gamma );
		materials_diff[m][2] = pow( b / 255.0f, gamma );
		
		materials_spec[m][0] = 1;
		materials_spec[m][1] = 1;
		materials_spec[m][2] = 1;
		materials_spec[m][3] = 1;
		
		materials_rgb[m] = SDL_MapRGB( format, r, g, b );
	}
	
	memset( materials_diff[0], 0, sizeof(float)*4 );
	memset( materials_spec[0], 0, sizeof(float)*4 );
	
	SDL_UnlockSurface( s );
	SDL_FreeSurface( s );
	printf( "Ok\n" );
}

static void blit2x( uint32 *dst, uint32 *src, size_t w, size_t h, size_t dst_pitch )
{
	float *dst0, *dst1;
	size_t y, x;
	
	dst_pitch >>= 2;
	dst0 = (float*) dst;
	dst1 = dst0 + dst_pitch;
	dst_pitch <<= 1;
	
	for( y=0; y<h; y++ ) {
		for( x=0; x<w; x+=4,src+=4 ) {
			__m128 a, b;
			a = _mm_load_ps( (void*) src );
			b = _mm_unpackhi_ps( a, a );
			a = _mm_unpacklo_ps( a, a );
			_mm_store_ps( dst0+2*x+4, b );
			_mm_store_ps( dst1+2*x+4, b );
			_mm_store_ps( dst0+2*x, a );
			_mm_store_ps( dst1+2*x, a );
			/* SDL better align the memory or else... */
		}
		dst0 += dst_pitch;
		dst1 += dst_pitch;
	}
}

static const char HELP_TEXT[] = \
"Usage:\n"
"    rays.bin [options]\n"
"Options:\n"
"  -h,--help   Print this text and exit\n"
"  -res=WxH    Window size\n"
"  -d=N        Set maximum octree depth\n"
"  -t=N        Rendering threads (0=single thread)\n"
"  -bench      Run benchmark\n"
"Key mappings:\n"
"  1,2,3,4,5: set brush radius\n"
"  F1: dump octree to file\n"
"  F2: load octree\n"
"  F3: reset camera\n"
"  F4: regenerate the volume\n"
"  F5: toggle fullscreen\n"
"  F6: switch shading modes\n"
"  F7,f8: adjust octree traversal depth\n"
"  F9,f10: adjust field of view\n"
"  F11: toggle shadows\n"
"  Space: grab cursor\n"
"  P: enable phong\n"
"  U: enable 2x upscaling\n"
"  Y: show depth buffer\n"
"  O: enable ambient occlusion\n"
"  I: toggle traversal method\n"
"  L: move light (hold)\n"
"  ESC: quit\n";

int main( int argc, char **argv )
{
	int resx = DEFAULT_RESX;
	int resy = DEFAULT_RESY;
	int max_octree_depth = DEFAULT_OCTREE_DEPTH;
	int n_threads = DEFAULT_THREADS;
	
	int vflags = 0;
	char **arg;
	RayPerfInfo perf = {0};
	uint64 prev_tick_time;
	Camera prev_camera;
	int rasterize_voxels = 1;
	
	for( arg=argv+argc-1; arg!=argv; arg-- )
	{
		const char *a = *arg;
		
		if ( strncmp(a, "-res=", 5) == 0 )
			sscanf( a, "-res=%dx%d", &resx, &resy );
		else if ( strncmp(a, "-t=", 3) == 0 )
			sscanf( a, "-t=%d", &n_threads );
		else if ( strcmp(a, "-bench") == 0 )
			benchmark_mode = 1;
		else if ( strncmp(*arg, "-d=", 3) == 0 )
			sscanf( *arg, "-d=%d", &max_octree_depth );
		else if ( !strcmp(a, "-h") || !strcmp(a, "--help") )
		{
			printf( "%s", HELP_TEXT );
			return 0;
		}
	}
	
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 )
	{
		printf( "Failed to initialize SDL, reason: %s\n", SDL_GetError() );
		return 0;
	}
	
	signal( SIGINT, quit );
	resize( resx, resy, 0 );
	load_materials( screen->format );
	
	printf( "Render resolution: %dx%d (%dx%d)\n", resx, resy, (int) render_resx, (int) render_resy );
	printf( "Rendering threads: %d\n", n_threads );
	printf( "Max octree depth: %d\n", max_octree_depth );
	printf( "Max voxel resolution: %d\n", 1 << max_octree_depth );
	
	the_volume = oc_init( max_octree_depth );
	setup_test_scene( the_volume );
	printf( "Initial octree nodes: %u\n", the_volume->num_nodes );
	
	if ( !load_font() )
		printf( "Warning: failed to load font: %s\n", SDL_GetError() );
	
	reset_camera();
	update_light_pos();
	
	if ( n_threads > 0 ) {
		start_render_threads( n_threads );
	}
	
	prev_tick_time = get_microsec();
	prev_camera = the_camera;
	
	for( ;; )
	{
		SDL_Event event;
		FILE *file;
		
		uint64 now = get_microsec();
		float timestep = ( now - prev_tick_time ) * 1e-6;
		prev_tick_time = now;
		
		while( SDL_PollEvent(&event) )
		{
			switch( event.type )
			{
				case SDL_QUIT:
					quit();
					break;
				
				case SDL_VIDEORESIZE:
					resize( event.resize.w, event.resize.h, 0 );
					break;
				
				case SDL_ACTIVEEVENT:
					if ( event.active.state & SDL_APPINPUTFOCUS && event.active.gain == 0 )
					{
						/* Lost input focus (e.g. the user alt-tabbed) */
						hook_mouse = 0;
						SDL_ShowCursor( 1 );
					}
					break;
				
				case SDL_KEYUP:
					if ( event.key.keysym.sym == SDLK_l )
						moving_light = 0;
					break;
				
				case SDL_KEYDOWN:
					switch( event.key.keysym.sym )
					{
						case SDLK_1:
						case SDLK_2:
						case SDLK_3:
						case SDLK_4:
						case SDLK_5:
							brush_radius = BRUSH_DEFAULT_RADIUS
								+ ( event.key.keysym.sym - SDLK_1 ) * BRUSH_RADIUS_INCR;
							break;
						
						case SDLK_F1:
							/* Dump octree to disk */
							file = fopen( "oc_cache.dat", "w" );
							{
								if ( file )
									oc_write( file, the_volume );
								else
									printf( "Error: failed to open file\n" );
							}
							fclose( file );
							break;
							
						case SDLK_F2:
							/* Read octree from disk */
							file = fopen( "oc_cache.dat", "r" );
							{
								if ( file )
								{
									/* Old octree MUST be free'd before loading new content. */
									oc_free( the_volume );
									the_volume = oc_read( file );
									if ( !the_volume )
									{
										the_volume = oc_init( max_octree_depth );
										setup_test_scene( the_volume );
									}
									reset_camera();
									fclose( file );
								}
								else
								{
									printf( "Error: failed to open file\n" );
								}
							}
							oc_detail_level = 0;
							break;
							
						case SDLK_F3:
							reset_camera();
							break;
						
						case SDLK_F4:
							oc_free( the_volume );
							the_volume = oc_init( max_octree_depth );
							setup_test_scene( the_volume );
							break;
						
						case SDLK_F5:
							if ( vflags & SDL_FULLSCREEN )
								vflags &= ~SDL_FULLSCREEN;
							else
								vflags |= SDL_FULLSCREEN;
							resize( screen->w, screen->h, vflags );
							break;
						
						case SDLK_F6:
							{
								static int mode = 0;
								switch( ++mode )
								{
									case 2:
										/* Show eye space normals */
										oc_show_travel_depth = 0;
										show_normals = 1;
										break;
									
									case 1:
										/* Visualize octree depth */
										oc_show_travel_depth = 1;
										show_normals = 0;
										break;
									
									default:
										/* Larger than 2; reset to 0 */
										mode = 0;
									case 0:
										/* Show shaded materials */
										oc_show_travel_depth = 0;
										show_normals = 0;
										break;
								}
							}
							break;
						
						case SDLK_F7:
							/* More detail */
							oc_detail_level -= 1;
							oc_detail_level = max( oc_detail_level, 0 );
							break;
						
						case SDLK_F8:
							/* Less detail */
							oc_detail_level += 1;
							oc_detail_level = min( oc_detail_level, the_volume->root_level );
							break;
						
						case SDLK_F9:
							set_projection( &the_camera, the_camera.fovx + FOV_INCR, screen->w / (float) screen->h );
							break;
						
						case SDLK_F10:
							set_projection( &the_camera, the_camera.fovx - FOV_INCR, screen->w / (float) screen->h );
							break;
						
						case SDLK_F11:
							enable_shadows = !enable_shadows;
							break;
						
						case SDLK_SPACE:
							SDL_ShowCursor( hook_mouse );
							hook_mouse = !hook_mouse;
							if ( hook_mouse )
							{
								mouse_x = screen->w >> 1;
								mouse_y = screen->h >> 1;
								SDL_WarpMouse( mouse_x, mouse_y );
							}
							break;
						
						case SDLK_p:
							enable_phong = !enable_phong;
							break;
						case SDLK_u:
							upscale_shift = !upscale_shift;
							resize_render_output( ( screen->w & ~0xF ) >> upscale_shift, screen->h >> upscale_shift );
							break;
						case SDLK_y:
							show_depth_buffer = !show_depth_buffer;
							break;
						case SDLK_o:
							enable_aoccl = ( enable_aoccl + 1 ) % 3;
							break;
						case SDLK_i:
							enable_dac_method = !enable_dac_method;
							break;
						case SDLK_l:
							moving_light = !moving_light;
							break;
						case SDLK_k:
							rasterize_voxels = !rasterize_voxels;
							break;
						
						case SDLK_ESCAPE:
							quit();
						
						default:
							break;
					}
					break;
					
				case SDL_MOUSEMOTION:
					mouse_x = event.motion.x;
					mouse_y = event.motion.y;
					break;
				
				case SDL_MOUSEBUTTONDOWN:
					if ( event.button.button == SDL_BUTTON_WHEELUP )
						brush_mat += 1;
					else if ( event.button.button == SDL_BUTTON_WHEELDOWN )
						brush_mat -= 1;
					brush_mat = clamp( brush_mat, 1, NUM_MATERIALS - 1 );
					break;
				
				default:
					break;
			}
		}
		
		prev_camera = the_camera;
		process_input( timestep, screen->w >> 1, screen->h >> 1, &the_camera );
		
		if ( rasterize_voxels ) {
			SDL_FillRect( screen, 0, 0 );
			SDL_LockSurface( screen );
			rasterize_octree( the_volume, &the_camera, screen );
			SDL_UnlockSurface( screen );
		} else {
			/* Start rendering the next frame */
			begin_volume_rendering( &the_camera, the_volume );
			
			/* Put the previous frame on screen */
			SDL_LockSurface( screen );
			draw_ui_overlay( screen, &prev_camera );
			if ( upscale_shift )
				blit2x( screen->pixels, render_output_rgba, render_resx, render_resy, screen->pitch );
			else
				memcpy( screen->pixels, render_output_rgba, render_resx*render_resy*4 );
			SDL_UnlockSurface( screen );
		}
		
		draw_text_overlay( screen, perf, &prev_camera );
		SDL_Flip( screen );
		
		if ( !rasterize_voxels ) {
			end_volume_rendering( &perf );
			swap_render_buffers();
		}
	}
	
	quit();
	return 0;
}
