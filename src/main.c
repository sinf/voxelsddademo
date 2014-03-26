#include <stdlib.h>
#include <signal.h>
#include "SDL.h"

#include "types.h"
#include "vector.h"
#include "ray.h"
#include "aabb.h"

#include "voxels.h"
#include "voxels_io.h"
#include "voxels_csg.h"
#include "city.h"

#include "camera.h"
#include "render_core.h"
#include "render_threads.h"
#include "text.h"

#define RENDER_RESOLUTION_DIV 1
#define DEFAULT_RESX 320
#define DEFAULT_RESY 240
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

static double millis_per_frame = 0;
static SDL_Surface *screen = NULL;

static void quit( /* any number of arguments */ )
{
	stop_render_threads();
	SDL_Delay( 300 );
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
	
	resize_render_output( w, h, screen->pixels );
}

static void setup_test_scene( Octree *volume )
{	
	const float size = volume->size;
	aabb3f box;
	int n;
	
	oc_clear( the_volume, 0 );
	generate_city( volume );
	
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
	camera.pos[0] = 0.5f;
	camera.pos[1] = 0.5f;
	camera.pos[2] = -1.0f;
	
	camera.yaw = 0.0f;
	camera.pitch = 0.0f;
	
	set_projection( &camera, DEFAULT_FOV, screen->w / (float) screen->h );
	update_camera_matrix( &camera );
}

static void shoot( int win_x, int win_y, Material_ID m )
{
	Material_ID mat = 0;
	int x, y;
	float depth;
	Ray ray;
	
	x = win_x / (float) screen->w * render_resx;
	y = win_y / (float) screen->h * render_resy;
	
	get_primary_ray( &ray, &camera, x, y );
	oc_traverse( the_volume, &ray, &mat, &depth, NULL );
	
	if ( mat != 0 )
	{
		Sphere sph;
		int n;
		
		sph.r = brush_radius;
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
void process_input( int screen_centre_x, int screen_centre_y )
{
	float speed = 0.05f;
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
		float fine_zoom = sqrtf( x*x + y*y ) * 0.0005;
		
		if ( y < 0 )
			fine_zoom = -fine_zoom;
		
		motion[2] += fine_zoom;
		should_warp_mouse = 1;
	}
	else if ( hook_mouse )
	{
		/* Mouse look */
		float x = mouse_motion[0] * -0.01f;
		float y = mouse_motion[1] * -0.01f;
		rotate_camera( &camera, x, y );
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
	
	if ( keys[SDLK_LCTRL] )
		speed *= 0.2f;
	
	if ( keys[SDLK_a] ) motion[0] -= speed;
	if ( keys[SDLK_d] ) motion[0] += speed;
	if ( keys[SDLK_s] ) motion[2] -= speed;
	if ( keys[SDLK_w] ) motion[2] += speed;
	if ( keys[SDLK_e] ) motion[1] += speed;
	if ( keys[SDLK_q] ) motion[1] -= speed;
	
	update_camera_matrix( &camera );
	move_camera_local( &camera, motion );
}

#if 0
float dot_product4( const vec3f a, const vec3f b )
{
	float dot = 0.0f;
	int n;
	
	for( n=0; n<3; n++ )
		dot += a[n] * b[n];
	
	return dot;
}

 void multiply_mat4f( mat4f out, const mat4f a, const mat4f b )
{
	int n;
	for( n=0; n<4; n++ )
	{
		const float *b1 = b + 4*n;
		out[n] = dot_product4( a, b1 );
		out[3+n] = dot_product4( a+3, b1 );
		out[6+n] = dot_product4( a+6, b1 );
		out[9+n] = dot_product4( a+9, b1 );
	}
}
#endif

static void draw_ui_overlay( SDL_Surface *surf )
{
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
		0, surf->h - 4*GLYPH_H,
		"Mat=%d\n"
		"%d ms\n"
		"Depth: %d/%d\n"
		"Nodes: %u\n"
		"Resolution: %d^3\n",
		brush_mat,
		(int) millis_per_frame,
		the_volume->root_level - oc_detail_level, the_volume->root_level,
		the_volume->num_nodes,
		the_volume->size );
}

static void load_materials( void )
{
	SDL_Surface *s;
	int x, y, n, m;
	
	printf( "Loading materials...\n" );
	s = SDL_LoadBMP( "data/materials.bmp" );
	if ( !s )
		return;
	
	n = ( s->pitch * s->h / 4 ) & 0xFF;
	SDL_LockSurface( s );
	
	for( m=y=0; y<s->h; y++ )
	{
		for( x=0; x<s->w; x++,m++ )
		{
			uint32 pixel = *(uint32*)( (uint8*) s->pixels + s->format->BytesPerPixel * x + s->pitch * y );
			SDL_GetRGB( pixel, s->format, &materials[m].color[2], &materials[m].color[1], &materials[m].color[0] );
			materials[m].color[3] = 0xFF; /* opacity */
			if ( m == n )
				break;
		}
		if ( m == n )
			break;
	}
	
	materials[0].color[3] = 0;
	
	SDL_UnlockSurface( s );
	SDL_FreeSurface( s );
	printf( "Ok\n" );
}

int main( int argc, char **argv )
{
	int resx = DEFAULT_RESX;
	int resy = DEFAULT_RESY;
	int max_octree_depth = DEFAULT_OCTREE_DEPTH;
	int n_threads = DEFAULT_THREADS;
	
	int vflags = 0;
	char **arg;
	
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 )
	{
		printf( "Failed to initialize SDL, reason: %s\n", SDL_GetError() );
		return 0;
	}
	
	for( arg=argv+argc-1; arg!=argv; arg-- )
	{
		if ( strncmp(*arg, "-res=", 5) == 0 )
			sscanf( *arg, "-res=%dx%d", &resx, &resy );
		else if ( strncmp(*arg, "-d=", 3) == 0 )
			sscanf( *arg, "-d=%d", &max_octree_depth );
		else if ( strncmp(*arg, "-t=", 3) == 0 )
			sscanf( *arg, "-t=%d", &n_threads );
	}
	
	signal( SIGINT, quit );
	load_materials();
	resize( resx, resy, 0 );
	
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
	
	if ( n_threads > 0 ) {
		start_render_threads( n_threads );
	}
	
	for( ;; )
	{
		const Uint32 loop_start_time = SDL_GetTicks();
		SDL_Event event;
		FILE *file;
		
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
							set_projection( &camera, camera.fovx + FOV_INCR, screen->w / (float) screen->h );
							break;
						
						case SDLK_F10:
							set_projection( &camera, camera.fovx - FOV_INCR, screen->w / (float) screen->h );
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
		
		process_input( screen->w >> 1, screen->h >> 1 );
		
		SDL_FillRect( screen, NULL, 0 );
		begin_volume_rendering();
		/* SDL_Delay( 2 ); */
		end_volume_rendering();
		draw_ui_overlay( screen );
		SDL_Flip( screen );
		
		millis_per_frame = ( SDL_GetTicks() - loop_start_time );
		
		#if 0
		millis_per_frame = 
			0.75 * millis_per_frame
			+ 0.25 * ( SDL_GetTicks() - loop_start_time );
		if ( millis_per_frame < 20.0 )
			SDL_Delay( 20 - millis_per_frame );
		#endif
	}
	
	quit();
	return 0;
}
