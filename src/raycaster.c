#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "raycaster.h"

Octree *the_volume = NULL;
Camera camera;

Texture *render_output_m = NULL;
Texture *render_output_z = NULL;
int enable_shadows = 0;

static float screen_uv_scale[2];
static float screen_uv_min[2];

static float calc_raydir_z( void )
{
	return fabsf( screen_uv_min[0] ) / tanf( camera.fovx * 0.5f );
}

void resize_render_output( int w, int h )
{
	float screen_ratio = (float) w / (float) h;
	
	screen_uv_min[0] = -0.5f;
	screen_uv_scale[0] = 1.0f / w;
	
	screen_uv_min[1] = 0.5f / screen_ratio;
	screen_uv_scale[1] = -1.0f / h / screen_ratio;
	
	delete_texture( render_output_m );
	delete_texture( render_output_z );
	render_output_m = alloc_texture( w, h, 1, TEX_BYTE, TEX_RECTANGLE );
	render_output_z = alloc_texture( w, h, 1, TEX_FLOAT, TEX_RECTANGLE );
}

void get_primary_ray( Ray *ray, const Camera *c, int x, int y )
{
	int n;
	
	for( n=0; n<3; n++ )
		ray->o[n] = c->pos[n] * the_volume->size;
	
	ray->d[0] = screen_uv_min[0] + x * screen_uv_scale[0];
	ray->d[1] = screen_uv_min[1] + y * screen_uv_scale[1];
	ray->d[2] = calc_raydir_z();
	
	normalize( ray->d );	
	multiply_vec_mat3f( ray->d, c->eye_to_world, ray->d );
}

/* Starts rendering at start_row, skips every Nth row if row_incr > 1 */
static void render_part( int start_row, int row_incr )
{
	vec3f ray_origin;
	Ray ray;
	int x, y;
	
	float pixel_w;
	float pixel_u, pixel_v;
	float pixel_v_incr;
	
	uint8 *mat_p;
	float *depth_p;
	unsigned mat_skip;
	unsigned depth_skip;
	
	if ( row_incr < 1 )
	{
		printf( "%s:%s: Error: row_incr must be >= 1\n", __FILE__, __func__ );
		abort();
	}
	
	for( x=0; x<PADDED_VEC3_SIZE; x++ )
	{
		/* All rays start from the same coordinates */
		ray_origin[x] = camera.pos[x] * the_volume->size;
	}
	
	/* Precompute ... */
	pixel_w = calc_raydir_z();
	pixel_v = screen_uv_min[1] + start_row * screen_uv_scale[1];
	pixel_v_incr = row_incr * screen_uv_scale[1];
	
	/* Initialize pixel pointers */
	mat_skip = (row_incr-1) * render_output_m->w;
	depth_skip = (row_incr-1) * render_output_z->w;
	mat_p = render_output_m->data.u8 + start_row * render_output_m->w;
	depth_p = render_output_z->data.f32 + start_row * render_output_z->w;
	
	for( y=start_row; y<render_output_m->h; y+=row_incr )
	{
		pixel_u = screen_uv_min[0];
		
		for( x=0; x<render_output_m->w; x++ )
		{
			Material_ID m;
			
			memcpy( ray.o, ray_origin, sizeof(ray_origin) );
			ray.d[0] = pixel_u;
			ray.d[1] = pixel_v;
			ray.d[2] = pixel_w;
			
			normalize( ray.d );
			multiply_vec_mat3f( ray.d, camera.eye_to_world, ray.d );
			
			oc_traverse( the_volume, &ray, &m, depth_p );
			*mat_p = m;
			
			#if 1
			if ( enable_shadows )
			{
				/* Shadow ray */
				vec3f light_pos;
				float z = *depth_p;
				int a;
				
				light_pos[0] = light_pos[2] = -the_volume->size;
				light_pos[1] = 2 * the_volume->size;
				
				/* Avoid self-occlusion */
				z -= 0.001f;
				
				for( a=0; a<PADDED_VEC3_SIZE; a++ )
				{
					ray.o[a] = ray.o[a] + ray.d[a] * z;
					ray.d[a] = light_pos[a] - ray.o[a];
				}
				
				normalize( ray.d );
				oc_traverse( the_volume, &ray, &m, &z );
				
				if ( m != 0 )
					*mat_p |= 0x20;
			}
			#endif
			
			mat_p++;
			depth_p++;
			pixel_u += screen_uv_scale[0];
		}
		
		mat_p += mat_skip;
		depth_p += depth_skip;
		pixel_v += pixel_v_incr;
	}
}

typedef struct RenderThread
{
	int id;
	int start_row;
	int row_incr;
	pthread_t thread;
} RenderThread;

#define MAX_RENDER_THREADS 32
static RenderThread threads[MAX_RENDER_THREADS];
static int num_threads = 0;

static enum {
	R_RENDER=0,
	R_EXIT
} render_state = R_RENDER;

#define INITIAL_FRAME_ID 0
static unsigned current_frame_id = INITIAL_FRAME_ID;

static pthread_cond_t render_state_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t render_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static int finished_parts = 0;
static pthread_cond_t finished_parts_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t finished_parts_mutex = PTHREAD_MUTEX_INITIALIZER;

#if 0
	#define SELF ( (unsigned) pthread_self() )
	
	#define pthread_create(a,b,c,d) do { \
		printf("%u: creating thread\n", SELF ); \
		pthread_create(a,b,c,d); \
	} while(0)

	#define pthread_mutex_lock(x) do { \
		printf("%u: waiting for %s\n", SELF, #x ); \
		pthread_mutex_lock(x); \
		printf("%u: got %s\n", SELF, #x ); \
	} while(0)

	#define pthread_mutex_unlock(x) do { \
		printf("%u: released %s\n", SELF, #x ); \
		pthread_mutex_unlock(x); \
	} while(0)
#endif

static int cond_wait_ms( pthread_cond_t *cond, pthread_mutex_t *mutex, int millis )
{
	struct timespec abstime;
	clock_gettime( CLOCK_REALTIME, &abstime );
	
	abstime.tv_nsec += millis * 1000000;
	if ( abstime.tv_nsec > 999999999 )
	{
		abstime.tv_sec += abstime.tv_nsec / 1000000000;
		abstime.tv_nsec %= 1000000000;
	}
	
	return pthread_cond_timedwait( cond, mutex, &abstime );
}

static void *render_thread_func( void *p )
{
	const RenderThread *self = p;
	int running = 1;
	unsigned old_frame_id = INITIAL_FRAME_ID;
	
	while( running )
	{
		int s;
		
		/* Obtain render_state */
		pthread_mutex_lock( &render_state_mutex );
		/*pthread_cond_wait( &render_state_cond, &render_state_mutex );*/
		cond_wait_ms( &render_state_cond, &render_state_mutex, 1 );
		s = render_state;
		pthread_mutex_unlock( &render_state_mutex );
		
		switch( s )
		{
			case R_RENDER:
				/* Compare last rendered frame ID with the current frame ID.
					We don't want to render the same thing twice. */
				if ( old_frame_id != current_frame_id )
				{
					/* Do some heavy number crunching and recursion */
					render_part( self->start_row, self->row_incr );
					old_frame_id = current_frame_id;
					
					/* Job finished - notify main thread */
					pthread_mutex_lock( &finished_parts_mutex );
					finished_parts += 1;
					pthread_cond_signal( &finished_parts_cond );
					pthread_mutex_unlock( &finished_parts_mutex );
				}
				break;
			
			case R_EXIT:
				running = 0;
				break;
			
			default:
				/* Spurious wakeup */
				break;
		}
	}
	
	return NULL;
}

void stop_render_threads( void )
{
	int n;
	
	if ( num_threads <= 0 )
		return;
	
	printf( "Stopping renderer threads...\n" );
	
	/* Request all threads to terminate */
	pthread_mutex_lock( &render_state_mutex );
	render_state = R_EXIT;
	pthread_cond_broadcast( &render_state_cond );
	pthread_mutex_unlock( &render_state_mutex );
	
	/* Wait until all threads have terminated */
	for( n=0; n<num_threads; n++ )
		pthread_join( threads[n].thread, NULL );
	
	printf( "Threads terminated as expected\n" );
}

void start_render_threads( int count )
{	
	int n;
	
	if ( count <= 0 )
		return;
	
	if ( num_threads > 0 )
	{
		/* Clear the old threads before creating new ones */
		stop_render_threads();
	}
	
	num_threads = count;
	render_state = R_RENDER;
	current_frame_id = INITIAL_FRAME_ID;
	finished_parts = 0;
	
	printf( "Starting renderer threads... (%d)\n", count );
	for( n=0; n<num_threads; n++ )
	{
		threads[n].id = n;
		threads[n].start_row = n;
		threads[n].row_incr = num_threads;
		
		pthread_create( &threads[n].thread, NULL, render_thread_func, threads+n );
	}
}

void render_volume( void )
{
	if ( num_threads <= 0 )
	{
		/* Render alone */
		render_part( 0, 1 );
	}
	else
	{
		pthread_mutex_lock( &finished_parts_mutex );
		finished_parts = 0;
		
		/* Advance frame counter; causes worker threads to re-render */
		pthread_mutex_lock( &render_state_mutex );
		current_frame_id++;
		pthread_cond_broadcast( &render_state_cond );
		pthread_mutex_unlock( &render_state_mutex );
		
		while( finished_parts < num_threads )
		{
			/* Wait until threads are finished */
			pthread_cond_wait( &finished_parts_cond, &finished_parts_mutex );
		}
		
		pthread_mutex_unlock( &finished_parts_mutex );
	}
}
