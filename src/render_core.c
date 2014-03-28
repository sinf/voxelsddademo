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
float materials_rgb[NUM_MATERIALS][4];

int enable_shadows = 0;
int enable_phong = 0;
int show_normals = 0;

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
	double screen_ratio;
	const int nt = num_render_threads;
	int k;
	size_t alloc_pixels;
	
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
	
	alloc_pixels = total_pixels + render_resx + 16; /* Allocate some extra pixels */
	render_output_m = aligned_alloc( 16, alloc_pixels * sizeof render_output_m[0] );
	render_output_z = aligned_alloc( 16, alloc_pixels * sizeof render_output_z[0] );
	render_output_write = aligned_alloc( 16, alloc_pixels * sizeof( uint32 ) );
	render_output_rgba = aligned_alloc( 16, alloc_pixels * sizeof( uint32 ) );
	for( k=0; k<3; k++ )
		render_output_n[k] = aligned_alloc( 16, alloc_pixels * sizeof render_output_n[0][0] );
	
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

static __m128 dot_prod( __m128 ax, __m128 ay, __m128 az, __m128 bx, __m128 by, __m128 bz )
{
	ax = _mm_mul_ps( ax, bx );
	ay = _mm_mul_ps( ay, by );
	az = _mm_mul_ps( az, bz );
	return _mm_add_ps( _mm_add_ps( ax, ay ), az );
}

static void normalize_vec( __m128 *px, __m128 *py, __m128 *pz )
{
	__m128 x=*px, y=*py, z=*pz, t;
	t = dot_prod( x, y, z, x, y, z );
	t = _mm_rsqrt_ps( t );
	*px = _mm_mul_ps( x, t );
	*py = _mm_mul_ps( y, t );
	*pz = _mm_mul_ps( z, t );
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

static void shade_pixels( size_t start_row, size_t end_row, float *wox, float *woy, float *woz )
{
	size_t seek = start_row * render_resx;
	size_t x, y;
	uint8 *mat_p = render_output_m + seek;
	uint32 *out_p = render_output_write + seek;
	float *nx = render_output_n[0] + seek;
	float *ny = render_output_n[1] + seek;
	float *nz = render_output_n[2] + seek;
	
	for( y=start_row; y<end_row; y++ )
	{
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
			if ( enable_phong )
			{
				__m128 lx, ly, lz, max_byte;
				__m128 ldif[3];
				__m128 eye_x, eye_y, eye_z;
				
				max_byte = _mm_set1_ps( 255 );
				
				/* Light position */
				lx = _mm_load_ps( light_x );
				ly = _mm_load_ps( light_y );
				lz = _mm_load_ps( light_z );
				
				/* Light diffuse reflection constants */
				ldif[0] = _mm_set1_ps( 1 );
				ldif[1] = _mm_set1_ps( 1 );
				ldif[2] = _mm_set1_ps( 1 );
				
				eye_x = _mm_set1_ps( camera.pos[0] * the_volume->size );
				eye_y = _mm_set1_ps( camera.pos[1] * the_volume->size );
				eye_z = _mm_set1_ps( camera.pos[2] * the_volume->size );
				
				for( x=0; x<render_resx; x+=4 )
				{
					__m128 nor_x, nor_y, nor_z;
					__m128 vx, vy, vz;
					__m128 tlx, tly, tlz;
					__m128 tex, tey, tez;
					__m128 d;
					__m128 mat_dif[4];
					__m128i colors;
					int k;
					
					/* Compute vector to light */
					tlx = _mm_sub_ps( lx, _mm_load_ps( wox ) );
					tly = _mm_sub_ps( ly, _mm_load_ps( woy ) );
					tlz = _mm_sub_ps( lz, _mm_load_ps( woz ) );
					normalize_vec( &tlx, &tly, &tlz );
					
					/* Vector to eye */
					tex = _mm_sub_ps( eye_x, _mm_load_ps( wox ) );
					tey = _mm_sub_ps( eye_y, _mm_load_ps( woy ) );
					tez = _mm_sub_ps( eye_z, _mm_load_ps( woz ) );
					
					/* Get world space normal vector */
					nor_x = _mm_load_ps( nx );
					nor_y = _mm_load_ps( ny );
					nor_z = _mm_load_ps( nz );
					
					/* Dot product of the diffuse term */
					d = dot_prod( nor_x, nor_y, nor_z, tlx, tly, tlz );
					d = _mm_max_ps( _mm_setzero_ps(), d );
					
					/* Gather material diffuse parameters */
					for( k=0; k<4; k++ ) mat_dif[k] = _mm_load_ps( materials_rgb[mat_p[k]] );
					_MM_TRANSPOSE4_PS( mat_dif[0], mat_dif[1], mat_dif[2], mat_dif[3] );
					
					colors = _mm_setzero_si128();
					
					for( k=0; k<3; k++ )
					{
						__m128 r;
						__m128i i;
						
						r = mat_dif[k];
						
						/* Compute diffuse term */
						r = _mm_mul_ps( r, _mm_mul_ps( ldif[k], d ) );
						
						/* Gamma correction. sqrt is equivalent to pow(value,1/gamma) when gamma==2.0 */
						r = _mm_sqrt_ps( r );
						
						/* Convert to byte and pack RGB */
						i = _mm_cvtps_epi32( _mm_mul_ps( r, max_byte ) );
						colors = _mm_slli_epi32( colors, 8 );
						colors = _mm_or_si128( colors, i );
					}
					
					_mm_store_si128( (void*) out_p, colors );
					
					wox += 4;
					woy += 4;
					woz += 4;
					
					nx += 4;
					ny += 4;
					nz += 4;
					
					out_p += 4;
					mat_p += 4;
				}
			}
			else
			{
				for( x=0; x<render_resx; x++,mat_p++,out_p++ )
				{
					#if 0
					int m = *mat_p;
					int r = materials_r[m];
					int g = materials_g[m];
					int b = materials_b[m];
					*out_p = r << 16 | g << 8 | b;
					#else
					*out_p = materials[*mat_p].color;
					#endif
				}
			}
		}
	}
}

static void reconstruct_normals( size_t first_row, size_t end_row,
	float *ray_dx, float *ray_dy, float *ray_dz, float *depth_p,
	float *wox_p, float *woy_p, float *woz_p )
{
	size_t second_last_row = end_row - 1;
	size_t y, x, seek;
	float *out_nx, *out_ny, *out_nz;
	
	seek = first_row * render_resx;
	out_nx = render_output_n[0] + seek;
	out_ny = render_output_n[1] + seek;
	out_nz = render_output_n[2] + seek;
	
	for( y=first_row; y<second_last_row; y++ )
	{
		__m128 ax0, ay0, az0, lz;
		
		ax0 = _mm_load_ss( ray_dx );
		ay0 = _mm_load_ss( ray_dy );
		az0 = _mm_load_ss( ray_dz );
		lz = _mm_load_ss( depth_p );
		
		CALC_ROW_NORMALS:
		for( x=0; x<render_resx; x+=4 )
		{	
			__m128
			z0, z1, z2,
			ax, ay, az,
			bx, by, bz,
			cx, cy, cz,
			nx, ny, nz;
			
			z0 = _mm_load_ps( depth_p );
			z1 = _mm_move_ss( _mm_shuffle_ps( z0, z0, 0x93 ), lz );
			z2 = _mm_load_ps( depth_p + render_resx );
			
			ax = _mm_load_ps( ray_dx );
			ay = _mm_load_ps( ray_dy );
			az = _mm_load_ps( ray_dz );
			
			/* Left shift ax,ay,az by 32 bits and put ax0,ay0,az0 into the low 32 bits */
			bx = _mm_move_ss( _mm_shuffle_ps( ax, ax, 0x93 ), ax0 );
			by = _mm_move_ss( _mm_shuffle_ps( ay, ay, 0x93 ), ay0 );
			bz = _mm_move_ss( _mm_shuffle_ps( az, az, 0x93 ), az0 );
			
			/* load_ss doesn't care about alignment. yay!! */
			ax0 = _mm_load_ss( ray_dx + 3 );
			ay0 = _mm_load_ss( ray_dy + 3 );
			az0 = _mm_load_ss( ray_dz + 3 );
			lz = _mm_load_ss( depth_p + 3 );
			
			cx = _mm_load_ps( ray_dx + render_resx );
			cy = _mm_load_ps( ray_dy + render_resx );
			cz = _mm_load_ps( ray_dz + render_resx );
			
			/* compute world space coordinates from ray direction and depth */
			ax = _mm_mul_ps( ax, z0 );
			ay = _mm_mul_ps( ay, z0 );
			az = _mm_mul_ps( az, z0 );
			
			/* save world space position of the pixel for later use */
			_mm_store_ps( wox_p, _mm_add_ps( _mm_load_ps( wox_p ), ax ) );
			_mm_store_ps( woy_p, _mm_add_ps( _mm_load_ps( woy_p ), ay ) );
			_mm_store_ps( woz_p, _mm_add_ps( _mm_load_ps( woz_p ), az ) );
			wox_p += 4;
			woy_p += 4;
			woz_p += 4;
			
			bx = _mm_mul_ps( bx, z1 );
			by = _mm_mul_ps( by, z1 );
			bz = _mm_mul_ps( bz, z1 );
			
			cx = _mm_mul_ps( cx, z2 );
			cy = _mm_mul_ps( cy, z2 );
			cz = _mm_mul_ps( cz, z2 );
			
			/* compute vector BA */
			bx = _mm_sub_ps( ax, bx );
			by = _mm_sub_ps( ay, by );
			bz = _mm_sub_ps( az, bz );
			
			/* compute vector AC */
			cx = _mm_sub_ps( cx, ax );
			cy = _mm_sub_ps( cy, ay );
			cz = _mm_sub_ps( cz, az );
			
			/* cross product: BA x AC */
			nx = _mm_sub_ps( _mm_mul_ps( by, cz ), _mm_mul_ps( bz, cy ) );
			ny = _mm_sub_ps( _mm_mul_ps( bz, cx ), _mm_mul_ps( bx, cz ) );
			nz = _mm_sub_ps( _mm_mul_ps( bx, cy ), _mm_mul_ps( by, cx ) );
			
			normalize_vec( &nx, &ny, &nz );
			
			_mm_store_ps( out_nx, nx );
			_mm_store_ps( out_ny, ny );
			_mm_store_ps( out_nz, nz );
			
			out_nx += 4;
			out_ny += 4;
			out_nz += 4;
			
			ray_dx += 4;
			ray_dy += 4;
			ray_dz += 4;
			
			depth_p += 4;
		}
	}
	
	if ( y == second_last_row ) {
		/* Now, the very last row. But compute deltas from the row above instead of the row below
		because the row below belongs to some other thread whose data this thread shouldn't access
		*/
		depth_p -= render_resx;
		ray_dx -= render_resx;
		ray_dy -= render_resx;
		ray_dz -= render_resx;
		goto CALC_ROW_NORMALS;
		/* PS. no clue why the Y component of the very last row doesn't need to be flipped */
	}
}

#define ENABLE_RAY_CAST 1
#if 1
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
		
		oc_traverse_dac( the_volume, num_rays, o, d, mat_p0, depth_p0 );
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
				
				ray.o[0] = ray_ox[r];
				ray.o[1] = ray_oy[r];
				ray.o[2] = ray_oz[r];
				
				ray.d[0] = ray_dx[r];
				ray.d[1] = ray_dy[r];
				ray.d[2] = ray_dz[r];
				
				oc_traverse( the_volume, &ray, &mat, depth_p );
				
				*mat_p++ = mat;
				depth_p++;
			}
		}
	}
	
	/* Fills render_output_n[...] with world space normal vectors
	and also writes world space position of each pixel to ray_ox/oy/oz */
	reconstruct_normals( start_row, end_row, ray_dx, ray_dy, ray_dz, depth_p0, ray_ox, ray_oy, ray_oz );
	
	if ( enable_shadows )
	{
		__m128 lx, ly, lz, dx, dy, dz;
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
				
				dx = _mm_sub_ps( lx, ox );
				dy = _mm_sub_ps( ly, oy );
				dz = _mm_sub_ps( lz, oz );
				normalize_vec( &dx, &dy, &dz );
				
				ox = _mm_add_ps( ox, _mm_mul_ps( dx, depth_offset ) );
				oy = _mm_add_ps( oy, _mm_mul_ps( dy, depth_offset ) );
				oz = _mm_add_ps( oz, _mm_mul_ps( dz, depth_offset ) );
				
				_mm_store_ps( ray_dx+r, dx );
				_mm_store_ps( ray_dy+r, dy );
				_mm_store_ps( ray_dz+r, dz );
				
				_mm_store_ps( ray_ox+r, ox );
				_mm_store_ps( ray_oy+r, oy );
				_mm_store_ps( ray_oz+r, oz );
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
			
			oc_traverse_dac( the_volume, num_rays, o, d, (uint8*) shadow_buf, (float*) depth_p0 );
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
						
						oc_traverse( the_volume, &ray, &m, &z );
						shadow_m[s] = m;
					}
					
					calc_shadow_mat( mat_p, shadow_m );
					mat_p += 16;
				}
			}
		}
	}
	#endif
	
	shade_pixels( start_row, end_row, ray_ox, ray_oy, ray_oz );
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
