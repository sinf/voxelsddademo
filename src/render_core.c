#include <xmmintrin.h>
#include <emmintrin.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "oc_traverse2.h"
#include "render_core.h"
#include "render_threads.h"
#include "mm_math.c"

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
int enable_phong = 1;
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

static float calc_raydir_z( void ) {
	return fabs( screen_uv_min[0] ) / tanf( camera.fovx * 0.5f );
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
			__m128 byte_half = _mm_set1_ps( 127.5 );
			for( x=0; x<render_resx; x+=4 )
			{
				__m128i r, g, b, rgb;
				r = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( _mm_load_ps( nx ), byte_half ), byte_half ) );
				g = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( _mm_load_ps( ny ), byte_half ), byte_half ) );
				b = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( _mm_load_ps( nz ), byte_half ), byte_half ) );
				r = _mm_slli_si128( r, 2 );
				g = _mm_slli_si128( g, 1 );
				rgb = _mm_or_si128( g, _mm_or_si128( r, b ) );
				_mm_store_si128( (void*) out_p, rgb );
				out_p+=4;
				nx+=4;
				ny+=4;
				nz+=4;
			}
		}
		else
		{
			if ( !enable_phong )
			{
				for( x=0; x<render_resx; x++,mat_p++,out_p++ )
					*out_p = materials[*mat_p].color;
			}
			else
			{
				#define ENABLE_SPECULAR_TERM 0
				
				__m128
				lx, ly, lz,
				max_byte,
				ldif, lamb;
				
				#if ENABLE_SPECULAR_TERM
				__m128 lspec[3], eye_x, eye_y, eye_z;
				#endif
				
				max_byte = _mm_set1_ps( 255 );
				
				/* Light position */
				lx = _mm_load_ps( light_x );
				ly = _mm_load_ps( light_y );
				lz = _mm_load_ps( light_z );
				
				/* Light diffuse reflection constant. Could use 3 constants for colored light */
				ldif = _mm_set1_ps( 1.0 );
				
				#if ENABLE_SPECULAR_TERM
				/* Light specular constants */
				lspec[0] = lspec[1] = lspec[2] = _mm_set1_ps( 1.0 );
				eye_x = _mm_set1_ps( camera.pos[0] * the_volume->size );
				eye_y = _mm_set1_ps( camera.pos[1] * the_volume->size );
				eye_z = _mm_set1_ps( camera.pos[2] * the_volume->size );
				#endif
				
				/* Ambient term. Note: not the same thing as in Phong reflection model */
				lamb = _mm_set1_ps( 0.2 );
				
				for( x=0; x<render_resx; x+=4 )
				{
					int k;
					__m128i colors;
					__m128
					nor_x, nor_y, nor_z,
					tlx, tly, tlz, tl_inv_len,
					d,
					mat_dif[4];
					
					#if ENABLE_SPECULAR_TERM
					__m128 tex, tey, tez, refx, refy, refz, s;
					#endif
					
					/* Compute vector to light (unnormalized) */
					tlx = _mm_sub_ps( lx, _mm_load_ps( wox ) );
					tly = _mm_sub_ps( ly, _mm_load_ps( woy ) );
					tlz = _mm_sub_ps( lz, _mm_load_ps( woz ) );
					tl_inv_len = _mm_rsqrt_ps( vec_len_squared( tlx, tly, tlz ) );
					
					/* Get world space normal vector (normalized with bad precision) */
					nor_x = _mm_load_ps( nx );
					nor_y = _mm_load_ps( ny );
					nor_z = _mm_load_ps( nz );
					
					#if ENABLE_SPECULAR_TERM
					/* Vector to eye (unnormalized!) */
					tex = _mm_sub_ps( eye_x, _mm_load_ps( wox ) );
					tey = _mm_sub_ps( eye_y, _mm_load_ps( woy ) );
					tez = _mm_sub_ps( eye_z, _mm_load_ps( woz ) );
					
					/* Reflect by normal */
					refx = tex;
					refy = tey;
					refz = tez;
					reflect( &refx, &refy, &refz, nor_x, nor_y, nor_z );
					
					/* Then the specular term */
					s = dot_prod( tex, tey, tez, refx, refy, refz );
					#if 1
					/* Normalize */
					s = _mm_div_ps( s, _mm_sqrt_ps(
						_mm_mul_ps(
							vec_len_squared( tex, tey, tez ),
							vec_len_squared( refx, refy, refz ))
						)
					);
					#else
					/* Normalize, assuming that the lengths of the 2 vectors are equal */
					s = _mm_div_ps( s, vec_len_squared( tex, tey, tez ) );
					#endif
					s = _mm_max_ps( _mm_setzero_ps(), s );
					
					/* Hard coded specular exponent */
					for( k=0; k<10; k++ )
						s = _mm_mul_ps( s, s );
					#endif
					
					/* Dot product of the diffuse term */
					d = dot_prod( nor_x, nor_y, nor_z, tlx, tly, tlz );
					d = _mm_mul_ps( d, tl_inv_len ); /* normalize */
					d = _mm_max_ps( _mm_setzero_ps(), d );
					d = _mm_add_ps( d, lamb ); /* ambient term */
					
					/* Gather material diffuse parameters */
					for( k=0; k<4; k++ ) mat_dif[k] = _mm_load_ps( materials_rgb[mat_p[k]] );
					_MM_TRANSPOSE4_PS( mat_dif[0], mat_dif[1], mat_dif[2], mat_dif[3] );
					
					colors = _mm_setzero_si128();
					
					for( k=0; k<3; k++ )
					{
						__m128 r;
						__m128i i;
						
						/* Diffuse term */
						r = _mm_mul_ps( mat_dif[k], _mm_mul_ps( ldif, d ) );
						
						#if ENABLE_SPECULAR_TERM
						/* Add specular term (todo: material specular parameters) */
						r = _mm_add_ps( _mm_mul_ps( s, lspec[k] ), r );
						#endif
						
						/* Gamma correction. sqrt is equivalent to pow(value,1/gamma) when gamma==2.0 */
						r = _mm_sqrt_ps( r );
						
						/* Convert to byte and pack RGB */
						r = _mm_mul_ps( r, max_byte );
						r = _mm_min_ps( r, max_byte );
						i = _mm_cvtps_epi32( r );
						colors = _mm_slli_si128( colors, 1 );
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
		}
	}
}

/*
Inputs:
	wox_p, woy_p, woz_p    Ray origin
	ray_dx, ray_dy, ray_dz  Ray direction
	depth_p                       Ray hit depth
Outputs:
	render_output_n[0..2]  World space surface normals
	wox_p, woy_p, woz_p    World space coordinates of ray intersections (=ray origin + ray direction * depth * depth_offset)
*/
static void reconstruct_normals( size_t first_row, size_t end_row,
	float const *ray_dx, float const *ray_dy, float const *ray_dz,
	float const *depth_p,
	float *wox_p, float *woy_p, float *woz_p )
{
	int need_normals = enable_phong;
	
	/* prevents self-occlusion for shadow rays */
	__m128 depth_offset = _mm_set1_ps( 0.9999f );
	
	size_t second_last_row = end_row - 1;
	size_t y, x, seek;
	float *out_nx, *out_ny, *out_nz;
	
	seek = first_row * render_resx;
	out_nx = render_output_n[0] + seek;
	out_ny = render_output_n[1] + seek;
	out_nz = render_output_n[2] + seek;
	
	for( y=first_row; y<second_last_row; y++ )
	{
		__m128 lax, lay, laz, lz;
		lax = lay = laz = lz = _mm_setzero_ps();
		
		CALC_ROW_NORMALS:
		for( x=0; x<render_resx; x+=4 )
		{
			__m128
			z0, z1, z2,
			ax, ay, az,
			ax0, ay0, az0,
			bx, by, bz,
			cx, cy, cz,
			nx, ny, nz;
			
			z0 = _mm_load_ps( depth_p );
			
			ax0 = _mm_load_ps( ray_dx );
			ay0 = _mm_load_ps( ray_dy );
			az0 = _mm_load_ps( ray_dz );
			
			/* compute world space coordinates from ray direction and depth */
			ax = _mm_mul_ps( ax0, z0 );
			ay = _mm_mul_ps( ay0, z0 );
			az = _mm_mul_ps( az0, z0 );
			
			/* save world space position of the pixel for later use */
			_mm_store_ps( wox_p, _mm_add_ps( _mm_load_ps( wox_p ), _mm_mul_ps( ax, depth_offset ) ) );
			_mm_store_ps( woy_p, _mm_add_ps( _mm_load_ps( woy_p ), _mm_mul_ps( ay, depth_offset ) ) );
			_mm_store_ps( woz_p, _mm_add_ps( _mm_load_ps( woz_p ), _mm_mul_ps( az, depth_offset ) ) );
			wox_p += 4;
			woy_p += 4;
			woz_p += 4;
			
			if ( need_normals ) {
				z1 = _mm_move_ss( _mm_shuffle_ps( z0, z0, 0x93 ), lz );
				z2 = _mm_load_ps( depth_p + render_resx );
				
				/* Left shift ax0,ay0,az0 by 32 bits and put lax,lay,laz into the low 32 bits */
				bx = _mm_move_ss( _mm_shuffle_ps( ax0, ax0, 0x93 ), lax );
				by = _mm_move_ss( _mm_shuffle_ps( ay0, ay0, 0x93 ), lay );
				bz = _mm_move_ss( _mm_shuffle_ps( az0, az0, 0x93 ), laz );
				
				cx = _mm_load_ps( ray_dx + render_resx );
				cy = _mm_load_ps( ray_dy + render_resx );
				cz = _mm_load_ps( ray_dz + render_resx );
			
				/* compute world space position of the pixels on the left */
				bx = _mm_mul_ps( bx, z1 );
				by = _mm_mul_ps( by, z1 );
				bz = _mm_mul_ps( bz, z1 );
				
				/* compute world space position of the pixels below */
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
				
				normalize_vec( out_nx, out_ny, out_nz, nx, ny, nz );
				
				/* load_ss doesn't care about alignment. yay!! */
				lax = _mm_load_ss( ray_dx + 3 );
				lay = _mm_load_ss( ray_dy + 3 );
				laz = _mm_load_ss( ray_dz + 3 );
				lz = _mm_load_ss( depth_p + 3 );
				
				out_nx += 4;
				out_ny += 4;
				out_nz += 4;
			}
			
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
			normalize_vec( &dx, &dy, &dz, dx, dy, dz );
			
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
	const int enable_raycast = 0;
	const int use_dac_method = 0;
	float *ray_ox, *ray_oy, *ray_oz, *ray_dx, *ray_dy, *ray_dz;
	
	size_t y, x, r;
	size_t resx = render_resx;
	size_t resy = end_row - start_row;
	size_t num_rays;
	size_t pixel_seek;
	
	float *depth_p0;
	uint8 *mat_p, *mat_p0;
	
	num_rays = resx * resy;
	ray_ox = ray_buffer;
	ray_oy = ray_ox + num_rays;
	ray_oz = ray_oy + num_rays;
	ray_dx = ray_oz + num_rays;
	ray_dy = ray_dx + num_rays;
	ray_dz = ray_dy + num_rays;
	
	/* Initialize pixel pointers */
	pixel_seek = start_row * render_resx;
	mat_p0 = render_output_m + pixel_seek;
	depth_p0 = render_output_z + pixel_seek;
	mat_p = mat_p0;
	
	generate_primary_rays( resx, start_row, end_row, ray_ox, ray_oy, ray_oz, ray_dx, ray_dy, ray_dz );
	
	num_rays &= ~7;
	
	if ( enable_raycast ) {
		/* Trace primary rays */
		if ( use_dac_method )
		{
			const float *o[3], *d[3];
			o[0]=ray_ox; o[1]=ray_oy; o[2]=ray_oz;
			d[0]=ray_dx; d[1]=ray_dy; d[2]=ray_dz;
			oc_traverse_dac( the_volume, num_rays, o, d, mat_p0, depth_p0 );
		} else {
			float *depth_p = depth_p0;
			for( r=0,y=start_row; y<end_row; y++ )
			{
				for( x=0; x<resx; x++,r++ )
				{
					Ray ray;
					Material_ID mat = 0;
					
					ray.o[0]=ray_ox[r]; ray.o[1]=ray_oy[r]; ray.o[2]=ray_oz[r];
					ray.d[0]=ray_dx[r]; ray.d[1]=ray_dy[r]; ray.d[2]=ray_dz[r];
					oc_traverse( the_volume, &ray, &mat, depth_p );
					
					*mat_p++ = mat;
					depth_p++;
				}
			}
		}
	}
	
	/* Compute surface normals and world space positions */
	reconstruct_normals( start_row, end_row, ray_dx, ray_dy, ray_dz, depth_p0, ray_ox, ray_oy, ray_oz );
	
	if ( enable_shadows )
	{
		__m128 lx, ly, lz;
		
		/* Light origin */
		lx = _mm_load_ps( light_x );
		ly = _mm_load_ps( light_y );
		lz = _mm_load_ps( light_z );
		
		/* Generate shadow rays */
		for( r=0,y=start_row; y<end_row; y++ )
		{
			for( x=0; x<resx; x+=4,r+=4 )
			{
				__m128 dx, dy, dz;
				dx = _mm_sub_ps( lx, _mm_load_ps( ray_ox+r ) );
				dy = _mm_sub_ps( ly, _mm_load_ps( ray_oy+r ) );
				dz = _mm_sub_ps( lz, _mm_load_ps( ray_oz+r ) );
				normalize_vec( ray_dx+r, ray_dy+r, ray_dz+r, dx, dy, dz );
			}
		}
		
		if ( enable_raycast ) {
			if ( use_dac_method )
			{
				const float *o[3], *d[3];
				__m128i *shadow_buf = aligned_alloc( 16, num_rays );
				__m128i *shadow_p = shadow_buf;
				
				o[0]=ray_ox; o[1]=ray_oy; o[2]=ray_oz;
				d[0]=ray_dx; d[1]=ray_dy; d[2]=ray_dz;
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
							
							ray.o[0]=ray_ox[r]; ray.o[1]=ray_oy[r]; ray.o[2]=ray_oz[r];
							ray.d[0]=ray_dx[r]; ray.d[1]=ray_dy[r]; ray.d[2]=ray_dz[r];
							oc_traverse( the_volume, &ray, &m, &z );
							shadow_m[s] = m;
						}
						
						calc_shadow_mat( mat_p, shadow_m );
						mat_p += 16;
					}
				}
			}
		}
	}
	
	shade_pixels( start_row, end_row, ray_ox, ray_oy, ray_oz );
}
