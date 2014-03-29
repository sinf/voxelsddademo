#include <xmmintrin.h>
#include <emmintrin.h>
#include <pmmintrin.h>
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
float materials_diff[NUM_MATERIALS][4];
float materials_spec[NUM_MATERIALS][4];

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

/* 1 thread, 2000x1000, takes about 20 ms per frame */
static void shade_pixels( size_t start_row, size_t end_row, float *wox_p, float *woy_p, float *woz_p )
{
	size_t seek = start_row * render_resx;
	size_t x, y;
	uint8 *mat_p = render_output_m + seek;
	uint32 *out_p = render_output_write + seek;
	float *nx_p = render_output_n[0] + seek;
	float *ny_p = render_output_n[1] + seek;
	float *nz_p = render_output_n[2] + seek;
	
	for( y=start_row; y<end_row; y++ )
	{
		if ( show_normals )
		{
			__m128 byte_half = _mm_set1_ps( 127.5 );
			for( x=0; x<render_resx; x+=4 )
			{
				__m128i r, g, b, rgb;
				r = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( _mm_load_ps( nx_p ), byte_half ), byte_half ) );
				g = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( _mm_load_ps( ny_p ), byte_half ), byte_half ) );
				b = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( _mm_load_ps( nz_p ), byte_half ), byte_half ) );
				r = _mm_slli_si128( r, 2 );
				g = _mm_slli_si128( g, 1 );
				rgb = _mm_or_si128( g, _mm_or_si128( r, b ) );
				_mm_store_si128( (void*) out_p, rgb );
				out_p+=4;
				nx_p+=4;
				ny_p+=4;
				nz_p+=4;
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
				__m128
				lx, ly, lz,
				max_byte,
				ldif, lamb;
				
				#if ENABLE_SPECULAR_TERM
				__m128 lspec, eye_x, eye_y, eye_z;
				#endif
				
				max_byte = _mm_set1_ps( 255 );
				
				/* Light position */
				lx = _mm_load_ps( light_x );
				ly = _mm_load_ps( light_y );
				lz = _mm_load_ps( light_z );
				
				/* Light diffuse reflection constant. Could use 3 components for colored light */
				ldif = _mm_set1_ps( 1.0 );
				
				#if ENABLE_SPECULAR_TERM
				/* Light specular constants */
				lspec = _mm_set1_ps( 1.0 ); /* could use 3 components instead of 1 */
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
					wx, wy, wz, /* world space pixel position */
					nx, ny, nz, /* world space surface normal */
					tlx, tly, tlz, /* vector to light */
					tl_norm,
					/*hx, hy, hz, * halfway vector between to-light vector and to-eye vector */
					d, /* diffuse term */
					mat_dif[4];
					
					#if ENABLE_SPECULAR_TERM
					__m128 tex, tey, tez, refx, refy, refz, s, mat_spec[4];
					#endif
					
					/* World space pixel position */
					wx = _mm_load_ps( wox_p );
					wy = _mm_load_ps( woy_p );
					wz = _mm_load_ps( woz_p );
					wox_p += 4;
					woy_p += 4;
					woz_p += 4;
					
					/* Compute vector to light (unnormalized!) */
					tlx = _mm_sub_ps( lx, wx );
					tly = _mm_sub_ps( ly, wy );
					tlz = _mm_sub_ps( lz, wz );
					tl_norm = _mm_rsqrt_ps( vec_len_squared( tlx, tly, tlz ) );
					/** normalize_vec( &tlx, &tly, &tlz, tlx, tly, tlz ); **/
					
					/* Get world space normal vector */
					nx = _mm_load_ps( nx_p );
					ny = _mm_load_ps( ny_p );
					nz = _mm_load_ps( nz_p );
					nx_p += 4;
					ny_p += 4;
					nz_p += 4;
					
					/* Dot product of the diffuse term */
					d = dot_prod( nx, ny, nz, tlx, tly, tlz );
					d = _mm_mul_ps( d, tl_norm ); /* normalize */
					
					#if ENABLE_SPECULAR_TERM
					
					/* Vector to eye */
					tex = _mm_sub_ps( eye_x, wx );
					tey = _mm_sub_ps( eye_y, wy );
					tez = _mm_sub_ps( eye_z, wz );
					normalize_vec( &tex, &tey, &tez, tex, tey, tez );
					
					#error tlx,tly,tlz not normalized
					hx = _mm_add_ps( tlx, tex );
					hy = _mm_add_ps( tly, tey );
					hz = _mm_add_ps( tlz, tez );
					normalize_vec( &hx, &hy, &hz, hx, hy, hz );
					
					/* Then the specular term */
					s = dot_prod( nx, ny, nz, hx, hy, hz );
					s = _mm_max_ps( _mm_setzero_ps(), s );
					s = _mm_and_ps( s, _mm_cmpgt_ps( d, _mm_setzero_ps() ) ); /* only include specular term when diffuse term is + */
					
					for( k=0; k<4; k++ ) mat_spec[k] = _mm_load_ps( materials_spec[mat_p[k]] );
					_MM_TRANSPOSE4_PS( mat_spec[0], mat_spec[1], mat_spec[2], mat_spec[3] );
					
					/* now mat_spec[3] has the specular exponent
					todo: use it */
					
					/* Hard coded specular exponent */
					for( k=0; k<10; k++ )
						s = _mm_mul_ps( s, s );
					
					#endif
					
					/* Gather material diffuse parameters */
					for( k=0; k<4; k++ ) mat_dif[k] = _mm_load_ps( materials_diff[mat_p[k]] );
					_MM_TRANSPOSE4_PS( mat_dif[0], mat_dif[1], mat_dif[2], mat_dif[3] );
					
					d = _mm_max_ps( _mm_setzero_ps(), d ); /* clamp dot product */
					d = _mm_add_ps( d, lamb ); /* ambient term */
					colors = _mm_setzero_si128(); /* accumulate bytes into this register */
					
					for( k=0; k<3; k++ )
					{
						__m128 r;
						__m128i i;
						
						/* Diffuse term */
						r = _mm_mul_ps( mat_dif[k], _mm_mul_ps( ldif, d ) );
						
						#if ENABLE_SPECULAR_TERM
						/* Add specular term (todo: material specular parameters) */
						r = _mm_add_ps( _mm_mul_ps( s, _mm_mul_ps( mat_spec[k], lspec ) ), r );
						#endif
						
						#if ENABLE_GAMMA_CORRECTION
						/* Gamma correction. sqrt is equivalent to pow(value,1/gamma) when gamma==2.0 */
						r = _mm_sqrt_ps( r );
						#endif
						
						/* Convert to byte and pack RGB */
						r = _mm_mul_ps( r, max_byte );
						r = _mm_min_ps( r, max_byte );
						i = _mm_cvtps_epi32( r );
						colors = _mm_slli_si128( colors, 1 );
						colors = _mm_or_si128( colors, i );
					}
					
					_mm_store_si128( (void*) out_p, colors );
					
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
Note:
	This function alone takes about 16 ms per frame with 1 thread at 2000x1000 resolution 
*/
static void reconstruct_normals( size_t first_row, size_t end_row,
	float const *ray_dx, float const *ray_dy, float const *ray_dz,
	float const *depth_p,
	float *wox_p, float *woy_p, float *woz_p )
{
	int need_normals = enable_phong;
	
	/* prevents self-occlusion for shadow rays */
	__m128 depth_offset = _mm_set1_ps( 0.001f );
	
	size_t second_last_row = end_row - 1;
	size_t y, x, seek;
	float *out_nx, *out_ny, *out_nz;
	
	seek = first_row * render_resx;
	out_nx = render_output_n[0] + seek;
	out_ny = render_output_n[1] + seek;
	out_nz = render_output_n[2] + seek;
	
	for( y=first_row; y<second_last_row; y++ )
	{
		__m128
		ldx_suf, ldy_suf, ldz_suf, /* previous ray direction on the left with the highest slot shuffled into the lowest slot */
		lz_suf; /* previous depth on the left with the highest slow shuffled into the lowest slot */
		
		#if 0
		/* Leaving these uninitialized works just fine.
		The leftmost pixel column won't have proper normals either way */
		ldx_suf = ldy_suf = ldz_suf = lz_suf = _mm_setzero_ps();
		#endif
		
		CALC_ROW_NORMALS:
		for( x=0; x<render_resx; x+=4 )
		{
			__m128
			dx, dy, dz, /* ray direction */
			ldx, ldy, ldz, /* ray direction on the left */
			bdx, bdy, bdz, /* ray direction below */
			z, /* depth */
			lz, /* depth on the left */
			bz, /* depth below */
			wx, wy, wz, /* world position */
			lwx, lwy, lwz, /* world position on the left */
			bwx, bwy, bwz, /* world position below */
			ux, uy, uz, /* vector u */
			vx, vy, vz, /* vector v */
			nx, ny, nz; /* normal vector */
			
			z = _mm_load_ps( depth_p );
			z = _mm_sub_ps( z, depth_offset );
			
			dx = _mm_load_ps( ray_dx );
			dy = _mm_load_ps( ray_dy );
			dz = _mm_load_ps( ray_dz );
			
			/* compute world space coordinates from ray direction and depth
			(these coordinates are incorrect because ray origin isn't added, but they work anyway because all primary rays share the same origin */
			wx = _mm_mul_ps( dx, z );
			wy = _mm_mul_ps( dy, z );
			wz = _mm_mul_ps( dz, z );
			
			/* compute the correct world space position of the pixel for later use */
			_mm_store_ps( wox_p, _mm_add_ps( _mm_load_ps( wox_p ), wx ) );
			_mm_store_ps( woy_p, _mm_add_ps( _mm_load_ps( woy_p ), wy ) );
			_mm_store_ps( woz_p, _mm_add_ps( _mm_load_ps( woz_p ), wz ) );
			wox_p += 4;
			woy_p += 4;
			woz_p += 4;
			
			if ( need_normals ) {
				
				lz = lz_suf;
				lz = _mm_move_ss( lz_suf = _mm_shuffle_ps( z, z, 0x93 ), lz );
				
				bz = _mm_load_ps( depth_p + render_resx );
				bz = _mm_sub_ps( bz, depth_offset );
				
				/* Ray directions on the left.
				Left shift ax0,ay0,az0 by 32 bits and put lax,lay,laz into the low 32 bits */
				
				ldx = ldx_suf;
				ldy = ldy_suf;
				ldz = ldz_suf;
				ldx = _mm_move_ss( ldx_suf = _mm_shuffle_ps( dx, dx, 0x93 ), ldx );
				ldy = _mm_move_ss( ldy_suf = _mm_shuffle_ps( dy, dy, 0x93 ), ldy );
				ldz = _mm_move_ss( ldz_suf = _mm_shuffle_ps( dz, dz, 0x93 ), ldz );
				
				/* Ray directions below */
				bdx = _mm_load_ps( ray_dx + render_resx );
				bdy = _mm_load_ps( ray_dy + render_resx );
				bdz = _mm_load_ps( ray_dz + render_resx );
				
				/* compute world space position of the pixels on the left */
				lwx = _mm_mul_ps( ldx, lz );
				lwy = _mm_mul_ps( ldy, lz );
				lwz = _mm_mul_ps( ldz, lz );
				
				/* compute world space position of the pixels below */
				bwx = _mm_mul_ps( bdx, bz );
				bwy = _mm_mul_ps( bdy, bz );
				bwz = _mm_mul_ps( bdz, bz );
				
				/* u = world pos - world pos on the left */
				ux = _mm_sub_ps( wx, lwx );
				uy = _mm_sub_ps( wy, lwy );
				uz = _mm_sub_ps( wz, lwz );
				
				/* v = world pos below - world pos */
				vx = _mm_sub_ps( bwx, wx );
				vy = _mm_sub_ps( bwy, wy );
				vz = _mm_sub_ps( bwz, wz );
				
				/* cross product: u x v */
				nx = _mm_sub_ps( _mm_mul_ps( uy, vz ), _mm_mul_ps( uz, vy ) );
				ny = _mm_sub_ps( _mm_mul_ps( uz, vx ), _mm_mul_ps( ux, vz ) );
				nz = _mm_sub_ps( _mm_mul_ps( ux, vy ), _mm_mul_ps( uy, vx ) );
				
				#if 1
				normalize_vec( out_nx, out_ny, out_nz, nx, ny, nz );
				#else
				_mm_store_ps( out_nx, nx );
				_mm_store_ps( out_ny, ny );
				_mm_store_ps( out_nz, nz );
				#endif
				
				out_nx += 4;
				out_ny += 4;
				out_nz += 4;
			}
			
			depth_p += 4;
			ray_dx += 4;
			ray_dy += 4;
			ray_dz += 4;
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
			
			/* For a horizontal field of view of 65 degrees the average unnormalized ray length is 0.143738
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
	const int enable_raycast = ENABLE_RAYCAST;
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
