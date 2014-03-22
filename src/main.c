#include <stdlib.h>
#include <signal.h>
#include "SDL.h"
#include "opengl.h"
#include "shader.h"

#include "types.h"
#include "vector.h"
#include "ray.h"
#include "aabb.h"

#include "voxels.h"
#include "voxels_io.h"
#include "voxels_csg.h"
#include "city.h"

#include "camera.h"
#include "raycaster.h"
#include "trimesh.h"
#include "text.h"

#define RENDER_RESOLUTION_DIV 1
#define DEFAULT_RESX 800
#define DEFAULT_RESY 600
#define DEFAULT_OCTREE_DEPTH 9
#define DEFAULT_THREADS 6

#define DEFAULT_FOV radians(65)
#define FOV_INCR radians(5)

#define BRUSH_RADIUS_INCR 5
#define BRUSH_DEFAULT_RADIUS 1.4
#define BRUSH_DEFAULT_MAT 15

#define SHOW_HELP 1

static int brush_mat = BRUSH_DEFAULT_MAT;
static float brush_radius = BRUSH_DEFAULT_RADIUS;

static Trimesh *unit_cube = NULL;

static Texture *font_texture = NULL;
static Texture *material_texture = NULL;

static GLuint gui_prog = 0;
static GLuint polygon_prog = 0;
static GLuint combine_prog = 0;

static int screen_w = 0;
static int screen_h = 0;
static int mouse_x = 0;
static int mouse_y = 0;

static double millis_per_frame = 0;


static void quit( /* any number of arguments */ )
{
	stop_render_threads();
	SDL_Delay( 300 );
	SDL_Quit();
	exit(0);
}

static void resize( int w, int h, int extra_flags )
{
	SDL_Surface *surf;
	int flags;
	
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	
	flags = SDL_OPENGL;
	flags |= SDL_RESIZABLE;
	flags |= extra_flags;
	surf = SDL_SetVideoMode( w, h, 0, flags );
	
	if ( !surf )
	{
		printf( "Failed to set %dx%d video mode, reason: %s\n", w, h, SDL_GetError() );
		exit(0);
	}
	
	screen_w = w;
	screen_h = h;
	glViewport( 0, 0, w, h );
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
	
	set_projection( &camera, DEFAULT_FOV, screen_w / (float) screen_h );
	update_camera_matrix( &camera );
}

static void shoot( int win_x, int win_y, Material_ID m )
{
	Material_ID mat = 0;
	int x, y;
	float depth;
	Ray ray;
	
	x = win_x / (float) screen_w * render_output_m->w;
	y = win_y / (float) screen_h * render_output_m->h;
	
	get_primary_ray( &ray, &camera, x, y );
	oc_traverse( the_volume, &ray, &mat, &depth );
	
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

static void render( void )
{
	glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
	
	#if 0
	/* Test */
	{
		float ratio = (float) screen_w / (float) screen_h;
		float fovy = camera.fovx / ratio;
		
		glMatrixMode( GL_PROJECTION );
		glLoadIdentity();
		gluPerspective( degrees(fovy), ratio, ZNEAR, ZFAR );
		glScalef( 1.0, 1.0, -1.0 );
		glMatrixMode( GL_MODELVIEW );
		glLoadIdentity();
		
		glRotatef( degrees(camera.pitch), -1, 0, 0 );
		glRotatef( degrees(camera.yaw), 0, -1, 0 );
		
		glTranslatef(
			-camera.pos[0],
			-camera.pos[1],
			-camera.pos[2] );
		
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		
		#if 1
		glBegin( GL_LINES );
			glColor3ub( 255, 0, 0 );
			glVertex3s( 0, 0, 0 );
			glVertex3s( 1, 0, 0 );
			glColor3ub( 0, 255, 0 );
			glVertex3s( 0, 0, 0 );
			glVertex3s( 0, 1, 0 );
			glColor3ub( 0, 0, 255 );
			glVertex3s( 0, 0, 0 );
			glVertex3s( 0, 0, 1 );
		glEnd();
		#endif
		
		#if 0
		glColor3ub( 255, 255, 255 );
		/*glPolygonMode( GL_FRONT_AND_BACK, GL_LINE ); */
		select_mesh( unit_cube );
			glUseProgram( polygon_prog );
				glUniform1f( glGetUniformLocation( combine_prog, "zNear" ), ZNEAR );
				glUniform1f( glGetUniformLocation( combine_prog, "zFar" ), ZFAR );
				draw_mesh();
			glUseProgram( 0 );
		select_mesh( NULL );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		#endif
		
		glDisable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		
		/*
		do_splat( the_volume, &camera );
		*/
	}
	#endif
	
	#if 0
	/* Stage 1: Rasterize triangles */
	{
		float modelview[16];
		int n;
		
		for( n=0; n<3; n++ )
		{
			/* Inverse rotation */
			memcpy( modelview+4*n, camera.eye_to_world+3*n, sizeof(float)*3 );
			modelview[4*n+3] = 0.0f;
		}
		
		/* Rotated translation */
		modelview[12] = -dot_product( camera.world_to_eye, camera.pos );
		modelview[13] = -dot_product( camera.world_to_eye+3, camera.pos );
		modelview[14] = -dot_product( camera.world_to_eye+6, camera.pos );
		
		/* Scale */
		modelview[15] = 1.0f;
		
		glMatrixMode( GL_PROJECTION );
		glLoadMatrixf( camera.eye_to_view );
		glMatrixMode( GL_MODELVIEW );
		glLoadMatrixf( modelview );
		
		glColor3ub( 255, 0, 0 );
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		select_mesh( unit_cube );
			glUseProgram( polygon_prog );
				glUniform1f( glGetUniformLocation( combine_prog, "zNear" ), ZNEAR );
				glUniform1f( glGetUniformLocation( combine_prog, "zFar" ), ZFAR );
				draw_mesh();
			glUseProgram( 0 );
		select_mesh( NULL );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		
		glColor3ub( 255, 255, 255 );
	}
	#endif
	
	#if 1
	/* Stage 2: Combine voxels + triangles. And shading */
	{
		render_volume();
		
		glActiveTexture( GL_TEXTURE2 );
		glPixelTransferf( GL_RED_SCALE, 1.0f / the_volume->size );
		upload_texture( render_output_z );
		glBindTexture( render_output_z->gl_tex_target, render_output_z->gl_tex_id );
		glPixelTransferf( GL_RED_SCALE, 1.0f );
		
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( material_texture->gl_tex_target, material_texture->gl_tex_id );
		
		glActiveTexture( GL_TEXTURE0 );
		upload_texture( render_output_m );
		glBindTexture( render_output_m->gl_tex_target, render_output_m->gl_tex_id );
		
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LEQUAL );
		
		glUseProgram( combine_prog );
		{
			GLint verts[4*2];
			float fov[2];
			int res[2];
			
			res[0] = screen_w / RENDER_RESOLUTION_DIV;
			res[1] = screen_h / RENDER_RESOLUTION_DIV;
			glUniform2iv( glGetUniformLocation( combine_prog, "tex_resolution" ), 1, res );
			
			res[0] = screen_w;
			res[1] = screen_h;
			glUniform2iv( glGetUniformLocation( combine_prog, "resolution" ), 1, res );
			
			fov[0] = camera.fovx;
			fov[1] = screen_h / (float) screen_w * fov[0];
			glUniform2fv( glGetUniformLocation( combine_prog, "fov" ), 1, fov );
			
			glUniform1f( glGetUniformLocation( combine_prog, "zNear" ), ZNEAR );
			glUniform1f( glGetUniformLocation( combine_prog, "zFar" ), ZFAR );
			
			/*
			glUniformMatrix3fv( glGetUniformLocation( combine_prog, "eye_to_world" ), 1, GL_FALSE, camera.eye_to_world );
			glUniformMatrix3fv( glGetUniformLocation( combine_prog, "world_to_eye" ), 1, GL_FALSE, camera.world_to_eye );
			glUniformMatrix4fv( glGetUniformLocation( combine_prog, "eye_to_ndc" ), 1, GL_FALSE, camera.eye_to_ndc );
			*/
			
			verts[0] = verts[1] = verts[3] = verts[6] = 0;
			verts[2] = verts[4] = screen_w;
			verts[5] = verts[7] = screen_h;
			
			/* Draw a screen aligned quad */
			glEnableClientState( GL_VERTEX_ARRAY );
			glVertexPointer( 2, GL_INT, 0, verts );
			glDrawArrays( GL_QUADS, 0, 4 );
			glDisableClientState( GL_VERTEX_ARRAY );
		}
		glUseProgram( 0 );
		
		glActiveTexture( GL_TEXTURE2 );
		glBindTexture( render_output_z->gl_tex_target, 0 );
		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( material_texture->gl_tex_target, 0 );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( render_output_m->gl_tex_target, 0 );
		
		glDisable( GL_DEPTH_TEST );
	}
	#endif
	
	#if 1
	/* Stage 3: 2D overlay */
	glUseProgram( gui_prog );
	{
		int res[2];
		res[0] = screen_w;
		res[1] = screen_h;
		glUniform2iv( glGetUniformLocation( gui_prog, "resolution" ), 1, res );
		
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( font_texture->gl_tex_target, font_texture->gl_tex_id );
		
		#if SHOW_HELP
		draw_text( 0, 0,
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
		
		draw_text_f( 0, screen_h - 4*GLYPH_H,
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
		
		glBindTexture( font_texture->gl_tex_target, 0 );
	}
	glUseProgram( 0 );
	#endif
}

static void init_GL( void )
{
	GLuint vs_projection, vs_2D;
	GLuint fs_combine, fs_tex;
	
	glClearDepth( 1.0f );
	glDisable( GL_DEPTH_TEST );
	
	glShadeModel( GL_FLAT );
	glDisable( GL_LIGHTING );
	glDisable( GL_LIGHT0 );
	
	material_texture = load_texture( "data/materials.bmp", TEX_RECTANGLE );
	upload_texture( material_texture );
	
	/* Use the very first material color as the background color */
	glClearColor(
		material_texture->data.u8[0] / 255.0,
		material_texture->data.u8[1] / 255.0,
		material_texture->data.u8[2] / 255.0, 0.0 );
	
	/* Texturing units:
		0: Material indexes
		1: Material table (initialized only once)
		2: Linear Z buffer in eye space
	It is OK if the hardware doesn't support 3 textures
	since only the Z buffer based shading gets dropped.
	*/
	
	/* Load and compile some shaders */
	
	vs_projection = compile_shader( "data/projection.vert", GL_VERTEX_SHADER );
	vs_2D = compile_shader( "data/2d.vert", GL_VERTEX_SHADER );
	fs_tex = compile_shader( "data/textured.frag", GL_FRAGMENT_SHADER );
	fs_combine = compile_shader( "data/combine.frag", GL_FRAGMENT_SHADER );
	
	gui_prog = create_shader_program( vs_2D, fs_tex );
	polygon_prog = create_shader_program( vs_projection, 0 );
	combine_prog = create_shader_program( vs_2D, fs_combine );
	
	glUseProgram( gui_prog );
		glUniform1i( glGetUniformLocation( gui_prog, "colormap" ), 0 );
	glUseProgram( polygon_prog );
		glUniform1i( glGetUniformLocation( polygon_prog, "colormap" ), 0 );
	glUseProgram( combine_prog );
		glUniform1i( glGetUniformLocation( combine_prog, "input_mat_table" ), 1 );
		glUniform1i( glGetUniformLocation( combine_prog, "input_tex_m" ), 0 );
		glUniform1i( glGetUniformLocation( combine_prog, "input_tex_z" ), 2 );
	glUseProgram( 0 );
	
	unit_cube = load_wavefront_obj( "data/cube.obj" );
	
	font_texture = load_texture( "data/le_font.bmp", 0 );
	upload_texture( font_texture );
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
	resize( resx, resy, 0 );
	resize_render_output( resx / RENDER_RESOLUTION_DIV, resy / RENDER_RESOLUTION_DIV );
	
	printf( "Render resolution: %dx%d (%dx%d)\n", resx, resy, render_output_m->w, render_output_m->h );
	printf( "Rendering threads: %d\n", n_threads );
	printf( "Max octree depth: %d\n", max_octree_depth );
	printf( "Max voxel resolution: %d\n", 1 << max_octree_depth );
	
	the_volume = oc_init( max_octree_depth );
	setup_test_scene( the_volume );
	printf( "Generated initial octree nodes: %u\n", the_volume->num_nodes );
	
	init_GL();
	reset_camera();
	
	if ( n_threads > 0 )
		start_render_threads( n_threads );
	
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
							resize( screen_w, screen_h, vflags );
							break;
						
						case SDLK_F6:
							{
								static int mode = 0;
								GLint loc;
								glUseProgram( combine_prog );
								loc = glGetUniformLocation( combine_prog, "show_normals" );
								switch( ++mode )
								{
									case 2:
										/* Show eye space normals */
										oc_show_travel_depth = 0;
										glUniform1i( loc, 1 );
										break;
									
									case 1:
										/* Visualize octree depth */
										oc_show_travel_depth = 1;
										glUniform1i( loc, 0 );
										break;
									
									default:
										/* Larger than 2; reset to 0 */
										mode = 0;
									case 0:
										/* Show shaded materials */
										oc_show_travel_depth = 0;
										glUniform1i( loc, 0 );
										break;
								}
								glUseProgram( 0 );
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
							set_projection( &camera, camera.fovx + FOV_INCR, screen_w / (float) screen_h );
							break;
						
						case SDLK_F10:
							set_projection( &camera, camera.fovx - FOV_INCR, screen_w / (float) screen_h );
							break;
						
						case SDLK_F11:
							enable_shadows = !enable_shadows;
							break;
						
						case SDLK_SPACE:
							SDL_ShowCursor( hook_mouse );
							hook_mouse = !hook_mouse;
							if ( hook_mouse )
							{
								mouse_x = screen_w >> 1;
								mouse_y = screen_h >> 1;
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
					brush_mat = clamp( brush_mat, 1, N_MATERIALS - 1 );
					break;
				
				default:
					break;
			}
		}
		
		process_input( screen_w >> 1, screen_h >> 1 );
		render();
		SDL_GL_SwapBuffers();
		
		millis_per_frame = 
			0.75 * millis_per_frame
			+ 0.25 * ( SDL_GetTicks() - loop_start_time );
		
		if ( millis_per_frame < 20.0 )
			SDL_Delay( 20 - millis_per_frame );
	}
	
	quit();
	return 0;
}
