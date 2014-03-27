#include <xmmintrin.h>
#include <emmintrin.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "oc_traverse2.h"
#include "render_core.h"
#include "render_threads.h"

/* Used to prevent self-occlusion */
#define SHADOW_RAY_DEPTH_OFFSET 0.001f

Octree *the_volume = NULL;
Camera camera;

static uint8 *render_output_m = NULL; /* materials */
static float *render_output_z = NULL; /* ray depth (distance to first intersection) */
static float *render_output_n[3] = {NULL,NULL,NULL}; /* surface normals. separate buffers for x,y,z components */

static uint32 *render_output_write = NULL;
uint32 *render_output_rgba = NULL;

Material materials[NUM_MATERIALS];

volatile int enable_shadows = 0;
volatile int show_normals = 0;

static float screen_uv_scale[2];
static float screen_uv_min[2];

size_t render_resx=0, render_resy=0;
static size_t total_pixels = 0;

static float light_x[4], light_y[4], light_z[4];

void set_light_pos( float x, float y, float z )
{
	_mm_store_ps( light_x, _mm_set1_ps(x) );
	_mm_store_ps( light_y, _mm_set1_ps(y) );
	_mm_store_ps( light_z, _mm_set1_ps(z) );
}

void swap_render_buffers( void )
{
	void *p = render_output_rgba;
	render_output_rgba = render_output_write;
	render_output_write = p;
}

static float calc_raydir_z( void )
{
	return fabsf( screen_uv_min[0] ) / tanf( camera.fovx * 0.5f );
}

void resize_render_output( int w, int h )
{
	size_t alignment = 16;
	double screen_ratio;
	const int nt = num_render_threads;
	int k;
	
	stop_render_threads();
	
	render_resx = w & ~0xF; /* width needs to be a multiple of 16 */
	render_resy = h;
	total_pixels = render_resx * render_resy;
	
	if ( render_output_m ) free( render_output_m );
	if ( render_output_z ) free( render_output_z );
	if ( render_output_n[0] ) {
		free( render_output_n[0] );
		free( render_output_n[1] );
		free( render_output_n[2] );
	}
	
	if ( !total_pixels ) {
		render_output_m = NULL;
		render_output_z = NULL;
		render_output_n[0] = render_output_n[1] = render_output_n[2] = NULL;
		return;
	}
	
	screen_ratio = render_resx / (double) render_resy;
	screen_uv_min[0] = -0.5;
	screen_uv_scale[0] = 1.0 / render_resx;
	screen_uv_min[1] = 0.5 / screen_ratio;
	screen_uv_scale[1] = -1.0 / render_resy / screen_ratio;
	
	render_output_m = aligned_alloc( alignment, total_pixels * sizeof render_output_m[0] );
	render_output_z = aligned_alloc( alignment, total_pixels * sizeof render_output_z[0] );
	
	render_output_write = aligned_alloc( alignment, total_pixels * sizeof( uint32 ) );
	render_output_rgba = aligned_alloc( alignment, total_pixels * sizeof( uint32 ) );
	
	for( k=0; k<3; k++ )
		render_output_n[k] = aligned_alloc( alignment, total_pixels * sizeof render_output_n[0][0] );
	
	start_render_threads( nt );
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

#define restrict __restrict
static void translate_vector( void* restrict out_x, void* restrict out_y, void* restrict out_z, __m128 const x, __m128 const y, __m128 const z, __m128 const * restrict m )
{
	__m128 a, b, c;
	a = _mm_mul_ps( x, m[0] );
	a = _mm_add_ps( _mm_mul_ps( y, m[1] ), a );
	a = _mm_add_ps( _mm_mul_ps( z, m[2] ), a );
	b = _mm_mul_ps( x, m[3] );
	b = _mm_add_ps( _mm_mul_ps( y, m[4] ), b );
	b = _mm_add_ps( _mm_mul_ps( z, m[5] ), b );
	c = _mm_mul_ps( x, m[6] );
	c = _mm_add_ps( _mm_mul_ps( y, m[7] ), c );
	c = _mm_add_ps( _mm_mul_ps( z, m[8] ), c );
	_mm_store_ps( out_x, a );
	_mm_store_ps( out_y, b );
	_mm_store_ps( out_z, c );
}

static void normalize_vec( __m128 *x, __m128 *y, __m128 *z )
{
	static const float one[] = {1,1,1,1};
	__m128 xx, yy, zz, sq, inv_sq;
	xx = _mm_mul_ps( *x, *x );
	yy = _mm_mul_ps( *y, *y );
	zz = _mm_mul_ps( *z, *z );
	sq = _mm_add_ps( xx, _mm_add_ps( yy, zz ) );
#if 0
	/* Much less accuracy, slightly faster */
	inv_sq = _mm_rsqrt_ps( sq );
#else
	inv_sq = _mm_div_ps( _mm_load_ps( one ), _mm_sqrt_ps( sq ) );
#endif
	*x = _mm_mul_ps( *x, inv_sq );
	*y = _mm_mul_ps( *y, inv_sq );
	*z = _mm_mul_ps( *z, inv_sq );
}

static void calc_shadow_mat( void* restrict mat_p, void const* restrict shadow_mat_p )
{
	static const uint32 stored_shade_bits[] = {0x20202020, 0x20202020, 0x20202020, 0x20202020};
	__m128i mat, visible, zero, shade_bits;
	
	mat = _mm_load_si128( mat_p );
	zero = _mm_setzero_si128();
	shade_bits = _mm_load_si128( (void*) stored_shade_bits );
	
	visible = _mm_cmpeq_epi8( _mm_load_si128( shadow_mat_p ), zero );
	mat = _mm_or_si128( _mm_andnot_si128( visible, shade_bits ), mat );
	_mm_store_si128( mat_p, mat );
}

static void shade_pixels( size_t start_row, size_t end_row )
{
	size_t seek = start_row * render_resx;
	size_t x, y;
	/*float *depth_p = render_output_z + seek;*/
	uint8 *mat_p = render_output_m + seek;
	uint32 *out_p = render_output_write + seek;
	float *nx = render_output_n[0] + seek;
	float *ny = render_output_n[1] + seek;
	float *nz = render_output_n[2] + seek;
	
	for( y=start_row; y<end_row; y++ )
	{
		
		/**
		for( x=0; x<render_resx; x+=4 )
		{
			z = _mm_load_ps( depth_p );
		}
		
		for( x=0; x<render_resx; x+=4 )
		{
			__m128 z;
			__m128i c;
			
			z = _mm_loadl_ps( depth_p );
			
			
			__m128i c = _mm_set_ps(
				*(uint32*) materials[mat_p[0]].color,
				*(uint32*) materials[mat_p[1]].color,
				*(uint32*) materials[mat_p[2]].color,
				*(uint32*) materials[mat_p[3]].color );
			
			
			
			*out_p = * (uint32*) materials[*mat_p].color;
			
			depth_p += 2;
			out_p += 2;
			mat_p += 2;
		}
		**/
		if ( show_normals )
		{
			for( x=0; x<render_resx; x++,out_p++ ) {
				int r = ( nx[x] + 1.0f ) * 127.5f;
				int g = ( ny[x] + 1.0f ) * 127.5f;
				int b = ( nz[x] + 1.0f ) * 127.5f;
				*out_p = r | g << 8 | b << 16;
			}
			nx += render_resx;
			ny += render_resx;
			nz += render_resx;
		}
		else
		{
			for( x=0; x<render_resx; x++,mat_p++,out_p++ )
				*out_p = materials[*mat_p].color;
		}
	}
}

#define ENABLE_RAY_CAST 1
#if 0
static void generate_primary_rays(
	size_t resx,
	size_t start_row,
	size_t end_row,
	float *ray_ox, float *ray_oy, float *ray_oz,
	float *ray_dx, float *ray_dy, float *ray_dz )
{
	float u0f, duf;
	__m128 u0, u, v, w, du, dv;
	__m128 mvp[9];
	size_t r, y, x;
	__m128 ox, oy, oz;
	
	ox = _mm_set1_ps( camera.pos[0] * the_volume->size );
	oy = _mm_set1_ps( camera.pos[1] * the_volume->size );
	oz = _mm_set1_ps( camera.pos[2] * the_volume->size );
	
	u0f = screen_uv_min[0];
	duf = screen_uv_scale[0];
	u0 = _mm_set_ps( u0f + 3*duf, u0f + 2*duf, u0f + duf, u0f );
	du = _mm_set1_ps( duf*4 );
	v = _mm_set1_ps( screen_uv_min[1] + start_row * screen_uv_scale[1] );
	dv = _mm_set1_ps( screen_uv_scale[1] );
	w = _mm_set1_ps( calc_raydir_z() );
	
	for( x=0; x<9; x++ ) {
		mvp[x] = _mm_set1_ps( camera.eye_to_world[x] );
	}
	
	for( r=0,y=start_row; y<end_row; y++ )
	{
		u = u0;
		for( x=0; x<resx; x+=4,r+=4 )
		{
			__m128 dx, dy, dz;
			
			/* The average (unnormalized) ray length is 0.143738
			This information could be used to make vector normalization faster */
			
			/* Eye space direction */
			dx = u;
			dy = v;
			dz = w;
			normalize_vec( &dx, &dy, &dz );
			
			/* Convert to world space */
			translate_vector( ray_dx+r, ray_dy+r, ray_dz+r, dx, dy, dz, mvp );
			
			/* All rays start from the same coordinates */
			_mm_store_ps( ray_ox+r, ox );
			_mm_store_ps( ray_oy+r, oy );
			_mm_store_ps( ray_oz+r, oz );
			
			u = _mm_add_ps( u, du );
		}
		v = _mm_add_ps( v, dv );
	}
}
void render_part( size_t start_row, size_t end_row, float *ray_buffer )
{
	const int use_dac_method = 0;
	
	float *ray_ox, *ray_oy, *ray_oz, *ray_dx, *ray_dy, *ray_dz;
	
	__m128 ox, oy, oz;
	size_t y, x, r;
	size_t resx = render_resx;
	size_t resy = end_row - start_row;
	size_t num_rays = total_pixels;
	size_t pixel_seek;
	
	float *normal_p[3];
	float *depth_p, *depth_p0;
	uint8 *mat_p, *mat_p0;
	
	size_t ray_attr_skip = resx * resy;
	
	ray_ox = ray_buffer;
	ray_oy = ray_ox + ray_attr_skip;
	ray_oz = ray_oy + ray_attr_skip;
	ray_dx = ray_oz + ray_attr_skip;
	ray_dy = ray_dx + ray_attr_skip;
	ray_dz = ray_dy + ray_attr_skip;
	
	/* Initialize pixel pointers */
	pixel_seek = start_row * render_resx;
	mat_p0 = render_output_m + pixel_seek;
	depth_p0 = render_output_z + pixel_seek;
	mat_p = mat_p0;
	depth_p = depth_p0;
	
	for( x=0; x<3; x++ )
		normal_p[x] = render_output_n[x] + pixel_seek;
	
	generate_primary_rays( resx, start_row, end_row, ray_ox, ray_oy, ray_oz, ray_dx, ray_dy, ray_dz );
	
	#if ENABLE_RAY_CAST
	if ( use_dac_method )
	{
		const float *o[3], *d[3];
		
		o[0] = ray_ox;
		o[1] = ray_oy;
		o[2] = ray_oz;
		
		d[0] = ray_dx;
		d[1] = ray_dy;
		d[2] = ray_dz;
		
		mat_p = mat_p0;
		depth_p = depth_p0;
		
		oc_traverse_dac( the_volume, num_rays, o, d, mat_p0, depth_p0, normal_p );
	}
	else
	{
		/* Trace primary rays */
		for( r=0,y=start_row; y<end_row; y++ )
		{
			for( x=0; x<resx; x++,r++ )
			{
				Ray ray;
				Material_ID mat = 0;
				float *nor_p[3];
				
				ray.o[0] = ray_ox[r];
				ray.o[1] = ray_oy[r];
				ray.o[2] = ray_oz[r];
				
				ray.d[0] = ray_dx[r];
				ray.d[1] = ray_dy[r];
				ray.d[2] = ray_dz[r];
				
				nor_p[0] = normal_p[0] + r;
				nor_p[1] = normal_p[1] + r;
				nor_p[2] = normal_p[2] + r;
				
				oc_traverse( the_volume, &ray, &mat, depth_p, nor_p );
				
				*mat_p++ = mat;
				depth_p++;
			}
		}
	}
	if ( enable_shadows )
	{
		__m128 lx, ly, lz, dx, dy, dz, depth;
		__m128 depth_offset = _mm_set1_ps( SHADOW_RAY_DEPTH_OFFSET );
		
		/* Light origin */
		lx = _mm_load_ps( light_x );
		ly = _mm_load_ps( light_y );
		lz = _mm_load_ps( light_z );
		
		/* Generate shadow rays */
		depth_p = depth_p0;
		for( r=0,y=start_row; y<end_row; y++ )
		{
			for( x=0; x<resx; x+=4,r+=4 )
			{
				ox = _mm_load_ps( ray_ox+r );
				oy = _mm_load_ps( ray_oy+r );
				oz = _mm_load_ps( ray_oz+r );
				
				dx = _mm_load_ps( ray_dx+r );
				dy = _mm_load_ps( ray_dy+r );
				dz = _mm_load_ps( ray_dz+r );
				
				depth = _mm_load_ps( depth_p );
				depth = _mm_sub_ps( depth, depth_offset ); /* To avoid self-occlusion */
				
				ox = _mm_add_ps( ox, _mm_mul_ps( dx, depth ) );
				oy = _mm_add_ps( oy, _mm_mul_ps( dy, depth ) );
				oz = _mm_add_ps( oz, _mm_mul_ps( dz, depth ) );
				
				dx = _mm_sub_ps( lx, ox );
				dy = _mm_sub_ps( ly, oy );
				dz = _mm_sub_ps( lz, oz );
				normalize_vec( &dx, &dy, &dz );
				
				_mm_store_ps( ray_dx+r, dx );
				_mm_store_ps( ray_dy+r, dy );
				_mm_store_ps( ray_dz+r, dz );
				
				_mm_store_ps( ray_ox+r, ox );
				_mm_store_ps( ray_oy+r, oy );
				_mm_store_ps( ray_oz+r, oz );
				
				depth_p += 4;
			}
		}
		
		if ( use_dac_method )
		{
			const float *o[3], *d[3];
			__m128i *shadow_buf = aligned_alloc( sizeof( *shadow_buf ), ( num_rays | 7 ) + 1 );
			__m128i *shadow_p = shadow_buf;
			
			o[0] = ray_ox;
			o[1] = ray_oy;
			o[2] = ray_oz;
			
			d[0] = ray_dx;
			d[1] = ray_dy;
			d[2] = ray_dz;
			
			mat_p = mat_p0;
			depth_p = depth_p0;
			
			oc_traverse_dac( the_volume, num_rays, o, d, (uint8*) shadow_buf, (float*) depth_p0, NULL );
			mat_p = mat_p0;
			
			for( y=start_row; y<end_row; y++ )
			{
				for( x=0; x<resx; x+=16 )
				{
					calc_shadow_mat( mat_p, shadow_p );
					shadow_p++;
					mat_p+=16;
				}
			}
			
			free( shadow_buf );
		}
		else
		{
			/* Trace shadows */
			mat_p = mat_p0;
			for( r=0,y=start_row; y<end_row; y++ )
			{
				for( x=0; x<resx; x+=16 )
				{
					uint8 shadow_m[16] = {0};
					int s;
					
					for( s=0; s<16; s++,r++ )
					{
						Ray ray;
						Material_ID m;
						float z;
						
						if ( mat_p[s] == 0 )
							continue; /* the sky doesn't receive shadows */
						
						ray.o[0] = ray_ox[r];
						ray.o[1] = ray_oy[r];
						ray.o[2] = ray_oz[r];
						
						ray.d[0] = ray_dx[r];
						ray.d[1] = ray_dy[r];
						ray.d[2] = ray_dz[r];
						
						oc_traverse( the_volume, &ray, &m, &z, NULL );
						shadow_m[s] = m;
					}
					
					calc_shadow_mat( mat_p, shadow_m );
					mat_p += 16;
				}
			}
		}
	}
	#endif
	
	shade_pixels( start_row, end_row );
}
#else
void render_part( size_t start_row, size_t end_row, float *ray_buffer )
{
	size_t seek;
	
	vec3f ray_origin;
	Ray ray;
	size_t x, y;
	
	float pixel_w;
	float pixel_u, pixel_v;
	float pixel_v_incr;
	
	uint8 *mat_p;
	float *depth_p;
	float *nor_p[3];
	
	(void) ray_buffer;
	
	for( x=0; x<PADDED_VEC3_SIZE; x++ )
	{
		/* All rays start from the same coordinates */
		ray_origin[x] = camera.pos[x] * the_volume->size;
	}
	
	/* Precompute ... */
	pixel_w = calc_raydir_z();
	pixel_v = screen_uv_min[1] + start_row * screen_uv_scale[1];
	pixel_v_incr = screen_uv_scale[1];
	
	/* Initialize pixel pointers */
	seek = start_row * render_resx;
	mat_p = render_output_m + seek;
	depth_p = render_output_z + seek;
	nor_p[0] = render_output_n[0] + seek;
	nor_p[1] = render_output_n[1] + seek;
	nor_p[2] = render_output_n[2] + seek;
	
	for( y=start_row; y<end_row; y++ )
	{
		pixel_u = screen_uv_min[0];
		
		for( x=0; x<render_resx; x++ )
		{
			Material_ID m;
			
			memcpy( ray.o, ray_origin, sizeof(ray_origin) );
			ray.d[0] = pixel_u;
			ray.d[1] = pixel_v;
			ray.d[2] = pixel_w;
			
			normalize( ray.d );
			multiply_vec_mat3f( ray.d, camera.eye_to_world, ray.d );
			
			#if ENABLE_RAY_CAST
			oc_traverse( the_volume, &ray, &m, depth_p, nor_p );
			#endif
			
			*mat_p = m;
			
			nor_p[0]++;
			nor_p[1]++;
			nor_p[2]++;
			
			#if ENABLE_RAY_CAST
			if ( enable_shadows )
			{
				/* Shadow ray */
				float z = *depth_p;
				int a;
				
				/* Avoid self-occlusion */
				z -= SHADOW_RAY_DEPTH_OFFSET;
				
				for( a=0; a<PADDED_VEC3_SIZE; a++ )
					ray.o[a] = ray.o[a] + ray.d[a] * z;
				
				ray.d[0] = light_x[0] - ray.o[0];
				ray.d[1] = light_y[0] - ray.o[1];
				ray.d[2] = light_z[0] - ray.o[2];
				
				normalize( ray.d );
				oc_traverse( the_volume, &ray, &m, &z, NULL );
				
				if ( m != 0 )
					*mat_p |= 0x20;
			}
			#endif
			
			mat_p++;
			depth_p++;
			pixel_u += screen_uv_scale[0];
		}
		pixel_v += pixel_v_incr;
	}
	
	shade_pixels( start_row, end_row );
}
#endif
