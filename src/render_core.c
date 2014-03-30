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

static uint32 *render_output_write = NULL;
uint32 *render_output_rgba = NULL;

uint32 materials_rgb[NUM_MATERIALS];
float materials_diff[NUM_MATERIALS][4];
float materials_spec[NUM_MATERIALS][4];

int enable_shadows = 0;
int enable_phong = 1;
int show_normals = 0;
static const int enable_aoccl = 0; /* ambient occlusion */

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
	/* int k; */
	size_t alloc_pixels;
	
	stop_render_threads();
	
	render_resx = w & ~0xF; /* width needs to be a multiple of 16 */
	render_resy = h;
	total_pixels = render_resx * render_resy;
	
	if ( render_output_m ) free( render_output_m );
	if ( render_output_z ) free( render_output_z );
	
	if ( !total_pixels ) {
		render_output_m = NULL;
		render_output_z = NULL;
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

static void calc_shadow_mat( void* restrict mat_p, void const* restrict shadow_mat_p, __m128i shade_bits )
{
	__m128i mat, visible, zero;
	mat = _mm_load_si128( mat_p );
	zero = _mm_setzero_si128();
	visible = _mm_cmpeq_epi8( _mm_load_si128( shadow_mat_p ), zero );
	mat = _mm_or_si128( _mm_andnot_si128( visible, shade_bits ), mat );
	_mm_store_si128( mat_p, mat );
}

/* Returns 255>>shr broadcasted to all 4 slots */
#define get_255_shr(junk,shr) \
_mm_cvtepi32_ps( _mm_srli_epi32( _mm_cmpeq_epi32(junk,junk), 24+(shr) ) )

static __m128i normal_to_color( __m128 nx, __m128 ny, __m128 nz )
{
	__m128i r, g, b;
	__m128 byte_half = get_255_shr( r, 1 ); /* get 127 */
	r = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( nx, byte_half ), byte_half ) );
	g = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( ny, byte_half ), byte_half ) );
	b = _mm_cvtps_epi32( _mm_add_ps( _mm_mul_ps( nz, byte_half ), byte_half ) );
	r = _mm_slli_si128( r, 2 );
	g = _mm_slli_si128( g, 1 );
	return _mm_or_si128( g, _mm_or_si128( r, b ) );
}

static __m128i calculate_phong(
__m128 ldif, /* light diffuse intensity */
__m128 lamb, /* global ambient light */
uint8 m0, uint8 m1, uint8 m2, uint8 m3, /* material indices */
__m128 nx, __m128 ny, __m128 nz, /* world space surface normal */
__m128 tlx, __m128 tly, __m128 tlz /* vector to light */
)
{
	int k;
	__m128i colors;
	__m128 d, mat_dif[4],
	max_byte;
	
	/* Dot product of the diffuse term */
	d = dot_prod( nx, ny, nz, tlx, tly, tlz );
	
	/* Gather material diffuse parameters */
	mat_dif[0] = _mm_load_ps( materials_diff[m0] );
	mat_dif[1] = _mm_load_ps( materials_diff[m1] );
	mat_dif[2] = _mm_load_ps( materials_diff[m2] );
	mat_dif[3] = _mm_load_ps( materials_diff[m3] );
	_MM_TRANSPOSE4_PS( mat_dif[0], mat_dif[1], mat_dif[2], mat_dif[3] );
	
	d = _mm_max_ps( _mm_setzero_ps(), d ); /* clamp dot product */
	d = _mm_mul_ps( d, ldif );
	d = _mm_add_ps( d, lamb ); /* ambient term */
	
	/* accumulate bytes into this register */
	colors = _mm_setzero_si128();
	
	/* get 255.0 */
	max_byte = get_255_shr( colors, 0 );
	
	for( k=0; k<3; k++ )
	{
		__m128 r;
		__m128i i;
		
		/* Diffuse term */
		r = _mm_mul_ps( mat_dif[k], d );
		
		/* (Add specular to r ) */
		
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
	
	return colors;
}

static uint64 fnv1a_hash( uint64 x )
{
	uint64 h = 2166136261;
	int t;
	for( t=0; t<8; t++ ) {
		x >>= 4;
		h = h ^ ( x & 0xF );
		h *= 16777619;
		h &= 0xFFFFFFFF;
	}
	return h;
}

static void calc_ao_rays( void *xp, void *yp, void *zp, __m128 nx, __m128 ny, __m128 nz, int k )
{
	static const float
	lut_x[] = {
0.17608480733726006, -0.22311073318820154, 0.03387636667527373, 0.27668057899237797, -0.5035291840643238, 0.4729615227533923, -0.1568384393943196, -0.296495567066451, 0.6375586537073917, -0.657270942922291, 0.3139277056945738, 0.22980628205915257, -0.6860105272412337, 0.7969168510214679, -0.48150656533876957, -0.11010980532366277, 0.6689611871564978, -0.8906857415156827, 0.6426630271806866, -0.042521530715078804, -0.5979054381466168, 0.9361981733488518, -0.7838510465201524, 0.21159681451541415, 0.4833325303255987, -0.9328318142586943, 0.8942820818781708, -0.38222476698907004, -0.3364217546663529, 0.8824839190100603, -0.9659070760328131, 0.5407717140962143, 0.16935549543945927, -0.7897540927167819, 0.9935396814284652, -0.6750052493244947, 0.004829801391683504, 0.6618866869707114, -0.9749744764429643, 0.7743722856595951, -0.17255385351723249, -0.5085937894852695, 0.9110404352134207, -0.8302492360063659, 0.3199986260206788, 0.34184723273622036, -0.8055612930008001, 0.8360278347780077, -0.4332244981219977, -0.1757751277808731, 0.6651993770836362, -0.7867953829706278, 0.497699465479315, 0.02699220371529747, -0.49910714050739646, 0.6778584573739476, -0.4959101352871499, 0.08348071572637865, 0.3178957953001336, -0.49832414649111745, 0.3956506863211547, -0.11957042111618797, -0.12556607825354404, 0.16209996356578596
	}, lut_y[] = {
0.0, 0.20438770782809604, -0.38600372557254214, 0.360880820470474, -0.08906722276189095, -0.30085940631762265, 0.5834311770066409, -0.57088417221442, 0.2328353977766658, 0.27131189057069055, -0.6708452727509833, 0.7326586395458319, -0.3975571228002394, -0.1751999341695868, 0.6848935405671901, -0.8497097681835751, 0.5638011967468403, 0.03683265175979679, -0.6395355485584487, 0.9195673052042719, -0.7164905417447618, 0.12596414010849727, 0.545383141694224, -0.940569055126698, 0.8434797119694427, -0.2975987914656791, -0.41318100441167965, 0.9133065404754042, -0.9353381807465213, 0.4638088152069185, 0.25461000539008943, -0.8410242639831756, 0.9854311622530797, -0.6116299595442433, -0.0823127317128023, 0.729660895731282, -0.9900511514025508, 0.7296334682468557, -0.09036041533640489, -0.5877209563877525, 0.9485085803572646, -0.8082064041270729, 0.24967841272553218, 0.42607020018518765, -0.8631413492122121, 0.8397392177608405, -0.3817705300227861, -0.25775631680212285, 0.7392208016566791, -0.818554114783267, 0.4725264787316605, 0.09805679329400425, -0.5847177536526527, 0.7401725510403583, -0.5064654940561399, 0.03465935864398168, 0.40974564926851953, -0.5983487314909354, 0.4616830597991392, -0.11466322164887624, -0.2241598621265619, 0.36857753427073864, -0.27529237978379434, 0.06877107812860624
	}, lut_z[] = {
-0.984375, -0.953125, -0.921875, -0.890625, -0.859375, -0.828125, -0.796875, -0.765625, -0.734375, -0.703125, -0.671875, -0.640625, -0.609375, -0.578125, -0.546875, -0.515625, -0.484375, -0.453125, -0.421875, -0.390625, -0.359375, -0.328125, -0.296875, -0.265625, -0.234375, -0.203125, -0.171875, -0.140625, -0.109375, -0.078125, -0.046875, -0.015625, 0.015625, 0.046875, 0.078125, 0.109375, 0.140625, 0.171875, 0.203125, 0.234375, 0.265625, 0.296875, 0.328125, 0.359375, 0.390625, 0.421875, 0.453125, 0.484375, 0.515625, 0.546875, 0.578125, 0.609375, 0.640625, 0.671875, 0.703125, 0.734375, 0.765625, 0.796875, 0.828125, 0.859375, 0.890625, 0.921875, 0.953125, 0.984375
	};
	__m128 x, y, z, d;
	__m128i di;
	
	k &= 0xF;
	k <<= 2;
	
	/* Get a unit length vector */
	x = _mm_load_ps( lut_x + k );
	y = _mm_load_ps( lut_y + k );
	z = _mm_load_ps( lut_z + k );
	
	/* Compare against the normal */
	d = dot_prod( x, y, z, nx, ny, nz );
	
	/* Negate the vector if the dot product is negative */
	di = _mm_castps_si128( d );
	di = _mm_srli_epi32( di, 31 ); /* clear all but the sign bit */
	di = _mm_slli_epi32( di, 31 );
	d = _mm_castsi128_ps( di );
	x = _mm_xor_ps( x, d );
	y = _mm_xor_ps( y, d );
	z = _mm_xor_ps( z, d );
	
	_mm_store_ps( xp, x );
	_mm_store_ps( yp, y );
	_mm_store_ps( zp, z );
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
static void shade_pixels( size_t first_row, size_t end_row,
	float *tlx_p, float *tly_p, float *tlz_p, /* vectors to light */
	float *wox_p, float *woy_p, float *woz_p, /* world space coords */
	uint8 const *mat_p, uint32 *pixel_p )
{
	size_t second_last_row = end_row - 1;
	size_t y, x;
	
	for( y=first_row; y<second_last_row; y++ )
	{
		/* previous world coords on the left with the highest slot shuffled into the lowest slot
		Leaving these uninitialized works just fine.
		The leftmost pixel column won't have proper normals either way
		*/
		__m128 lwx_suf, lwy_suf, lwz_suf;
		
		PROCESS_SCANLINE:
		for( x=0; x<render_resx; x+=4 )
		{
			uint32 mats;
			__m128
			wx, wy, wz, /* world position */
			lwx, lwy, lwz, /* world position on the left */
			bwx, bwy, bwz, /* world position below */
			ux, uy, uz, /* vector u */
			vx, vy, vz, /* vector v */
			nx, ny, nz; /* normal vector */
			__m128i rgb; /* pixel color */
			
			mats = *(uint32*) mat_p;
			
			if ( mats == 0 ) {
				/* all 4 pixels got zero material */
				rgb = _mm_setzero_si128();
			}
			else
			{
				/* World space coords */
				wx = _mm_load_ps( wox_p );
				wy = _mm_load_ps( woy_p );
				wz = _mm_load_ps( woz_p );
				
				/* World space coords of the left neighbours */
				lwx = lwx_suf;
				lwy = lwy_suf;
				lwz = lwz_suf;
				lwx = _mm_move_ss( lwx_suf = _mm_shuffle_ps( wx, wx, 0x93 ), lwx );
				lwy = _mm_move_ss( lwy_suf = _mm_shuffle_ps( wy, wy, 0x93 ), lwy );
				lwz = _mm_move_ss( lwz_suf = _mm_shuffle_ps( wz, wz, 0x93 ), lwz );
				
				/* World space coords of the neighbours below */
				bwx = _mm_load_ps( wox_p + render_resx );
				bwy = _mm_load_ps( woy_p + render_resx );
				bwz = _mm_load_ps( woz_p + render_resx );
				
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
				normalize_vec( &nx, &ny, &nz, nx, ny, nz );
				
				/* Compute pixel color */
				if ( show_normals )
				{
					rgb = normal_to_color( nx, ny, nz );
				}
				else
				{
					__m128 tlx, tly, tlz, ldif, lamb;
					
					/* Vector to light */
					tlx = _mm_load_ps( tlx_p );
					tly = _mm_load_ps( tly_p );
					tlz = _mm_load_ps( tlz_p );
					
					/* Light diffuse intensity = 1.0 */
					ldif = _mm_mul_ps( tlx, _mm_rcp_ps(tlx) );
					
					/* Global ambient light = 0.25 */
					lamb = _mm_add_ps( _mm_add_ps( tlx, tlx ), _mm_add_ps( tlx, tlx ) );
					lamb = _mm_mul_ps( tlx, _mm_rcp_ps(lamb) );
					
					if ( enable_aoccl ) {
						/* Save normal vector for later use */
						_mm_store_ps( tlx_p, nx );
						_mm_store_ps( tly_p, ny );
						_mm_store_ps( tlz_p, nz );
					}
					
					rgb = calculate_phong( ldif, lamb,
					mat_p[0], mat_p[1], mat_p[2], mat_p[3],
					nx, ny, nz,
					tlx, tly, tlz );
				}
			}
			
			_mm_store_si128( (void*) pixel_p, rgb );
			
			tlx_p += 4;
			tly_p += 4;
			tlz_p += 4;
			
			wox_p += 4;
			woy_p += 4;
			woz_p += 4;
			
			pixel_p += 4;
			mat_p += 4;
		}
	}
	
	if ( y == second_last_row ) {
		/* Now, the very last row. But compute deltas from the row above instead of the row below
		because the row below belongs to some other thread whose data this thread shouldn't access
		*/
		wox_p -= render_resx;
		woy_p -= render_resx;
		woz_p -= render_resx;
		goto PROCESS_SCANLINE;
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
	__m128 m0, m1, m2, m3, m4, m5, m6, m7, m8;
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
	
	m0 = _mm_set1_ps( camera.eye_to_world[0] );
	m1 = _mm_set1_ps( camera.eye_to_world[1] );
	m2 = _mm_set1_ps( camera.eye_to_world[2] );
	m3 = _mm_set1_ps( camera.eye_to_world[3] );
	m4 = _mm_set1_ps( camera.eye_to_world[4] );
	m5 = _mm_set1_ps( camera.eye_to_world[5] );
	m6 = _mm_set1_ps( camera.eye_to_world[6] );
	m7 = _mm_set1_ps( camera.eye_to_world[7] );
	m8 = _mm_set1_ps( camera.eye_to_world[8] );
	
	for( r=0,y=start_row; y<end_row; y++ )
	{
		u = u0;
		for( x=0; x<resx; x+=4,r+=4 )
		{
			__m128 dx, dy, dz,
			wdx, wdy, wdz;
			
			/* For a horizontal field of view of 65 degrees the average unnormalized ray length is 0.143738
			This information could be used to make vector normalization faster */
			
			/* Eye space direction */
			dx = u;
			dy = v;
			dz = w;
			
			/* Multiply to the eye-to-world rotation matrix */
			wdx = dot_prod( dx, dy, dz, m0, m1, m2 );
			wdy = dot_prod( dx, dy, dz, m3, m4, m5 );
			wdz = dot_prod( dx, dy, dz, m6, m7, m8 );
			
			/* Normalize and store */
			normalize_vec( ray_dx+r, ray_dy+r, ray_dz+r, wdx, wdy, wdz );
			
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
	const int use_dac_method = ENABLE_DAC;
	
	float *ray_ox, *ray_oy, *ray_oz, *ray_dx, *ray_dy, *ray_dz;
	
	size_t r;
	size_t resx = render_resx;
	size_t resy = end_row - start_row;
	size_t num_rays;
	size_t pixel_seek;
	
	float *depth_p0;
	uint8 *mat_p0;
	
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
	
	generate_primary_rays( resx, start_row, end_row, ray_ox, ray_oy, ray_oz, ray_dx, ray_dy, ray_dz );
	
	if ( enable_raycast ) {
		/* Trace primary rays */
		if ( use_dac_method )
		{
			const float *o[3], *d[3];
			o[0]=ray_ox; o[1]=ray_oy; o[2]=ray_oz;
			d[0]=ray_dx; d[1]=ray_dy; d[2]=ray_dz;
			oc_traverse_dac( the_volume, num_rays, o, d, mat_p0, depth_p0 );
		} else {
			for( r=0; r<num_rays; r++ )
			{
				Ray ray;
				ray.o[0]=ray_ox[r]; ray.o[1]=ray_oy[r]; ray.o[2]=ray_oz[r];
				ray.d[0]=ray_dx[r]; ray.d[1]=ray_dy[r]; ray.d[2]=ray_dz[r];
				oc_traverse( the_volume, &ray, mat_p0+r, depth_p0+r );
			}
		}
	}
	
	if ( !( enable_shadows || enable_phong || show_normals ) )
	{
		/* Just put the material color to screen */
		uint32 *out_p = render_output_write + pixel_seek;
		for( r=0; r<num_rays; r++ )
			out_p[r] = materials_rgb[mat_p0[r]];
	}
	else
	{
		__m128 lx, ly, lz, depth_offset;
		
		/* Light origin */
		lx = _mm_load_ps( light_x );
		ly = _mm_load_ps( light_y );
		lz = _mm_load_ps( light_z );
		
		/* Prevents self-occlusion problem with shadows */
		depth_offset = _mm_set1_ps( 0.001f / 512.0 * ( 1 << the_volume->root_level ) );
		
		/* Generate shadow rays */
		for( r=0; r<num_rays; r+=4 )
		{
			__m128
			ox, oy, oz,
			dx, dy, dz,
			wx, wy, wz,
			depth;
			
			ox = _mm_load_ps( ray_ox+r );
			oy = _mm_load_ps( ray_oy+r );
			oz = _mm_load_ps( ray_oz+r );
			
			dx = _mm_load_ps( ray_dx+r );
			dy = _mm_load_ps( ray_dy+r );
			dz = _mm_load_ps( ray_dz+r );
			
			/* Compute world space coordinates of the primary ray intersection */
			depth = _mm_load_ps( depth_p0+r );
			depth = _mm_sub_ps( depth, depth_offset );
			wx = _mm_add_ps( ox, _mm_mul_ps( dx, depth ) );
			wy = _mm_add_ps( oy, _mm_mul_ps( dy, depth ) );
			wz = _mm_add_ps( oz, _mm_mul_ps( dz, depth ) );
			_mm_store_ps( ray_ox+r, wx );
			_mm_store_ps( ray_oy+r, wy );
			_mm_store_ps( ray_oz+r, wz );
			
			/* Compute vector to light */
			dx = _mm_sub_ps( lx, wx );
			dy = _mm_sub_ps( ly, wy );
			dz = _mm_sub_ps( lz, wz );
			normalize_vec( ray_dx+r, ray_dy+r, ray_dz+r, dx, dy, dz );
		}
	
		if ( enable_shadows )
		{
			static const uint32 stored_shade_bits[] = {0x20202020, 0x20202020, 0x20202020, 0x20202020};
			__m128i shade_bits;
			
			shade_bits = _mm_load_si128( (void*) stored_shade_bits );
			
			if ( enable_raycast ) {
				if ( use_dac_method )
				{
					const float *o[3], *d[3];
					__m128i *shadow_buf = aligned_alloc( 16, num_rays );
					
					o[0]=ray_ox; o[1]=ray_oy; o[2]=ray_oz;
					d[0]=ray_dx; d[1]=ray_dy; d[2]=ray_dz;
					oc_traverse_dac( the_volume, num_rays, o, d, (uint8*) shadow_buf, (float*) depth_p0 );
					
					for( r=0; r<num_rays; r+=16 )
						calc_shadow_mat( mat_p0+r, shadow_buf+r, shade_bits );
					
					free( shadow_buf );
				}
				else
				{
					/* Trace shadows */
					for( r=0; r<num_rays; r+=16 )
					{
						uint8 shadow_m[16] = {0};
						int s;
						
						for( s=0; s<16; s++ )
						{
							Ray ray;
							float z;
							int k = r + s;
							
							if ( mat_p0[k] == 0 )
								continue; /* the sky doesn't receive shadows */
							
							ray.o[0]=ray_ox[k]; ray.o[1]=ray_oy[k]; ray.o[2]=ray_oz[k];
							ray.d[0]=ray_dx[k]; ray.d[1]=ray_dy[k]; ray.d[2]=ray_dz[k];
							oc_traverse( the_volume, &ray, shadow_m+s, &z );
						}
						
						calc_shadow_mat( mat_p0+r, shadow_m, shade_bits );
					}
				}
			}
		}
		
		shade_pixels( start_row, end_row,
		ray_dx, ray_dy, ray_dz, /* vectors to light */
		ray_ox, ray_oy, ray_oz, /* world space coords */
		mat_p0, render_output_write+pixel_seek );
		
		if ( enable_aoccl )
		{
			/* Trace ambient occlusion */
			uint32 *pixel_p = render_output_write + pixel_seek;
			for( r=0; r<num_rays; r+=4,pixel_p+=4 )
			{
				#define AO_SAMP 8
				__m128 nx, ny, nz;
				int s, rr;
				int o[4];
				__m128i p, u;
				
				if ( *(uint32*)(mat_p0+r) == 0 )
					continue;
				
				nx = _mm_load_ps( ray_dx+r );
				ny = _mm_load_ps( ray_dy+r );
				ny = _mm_load_ps( ray_dz+r );
				
				/*
				_mm_store_si128( (void*) (pixel_p+r), normal_to_color( _mm_load_ps(dx), _mm_load_ps(dy), _mm_load_ps(dz) ) );
				continue;
				*/
				
				for( rr=0; rr<4; rr++ ) {
					Ray ray;
					int hits = 0;
					float dx[AO_SAMP], dy[AO_SAMP], dz[AO_SAMP];
					
					ray.o[0]=ray_ox[r+rr];
					ray.o[1]=ray_oy[r+rr];
					ray.o[2]=ray_oz[r+rr];
					
					for( s=0; s<AO_SAMP; s+=4 )
						calc_ao_rays( dx+s, dy+s, dz+s, nx, ny, nz, fnv1a_hash( s*r ) + rr + s*s + s );
					
					for( s=0; s<AO_SAMP; s++ )
					{
						float z;
						uint8 m;
						ray.d[0]=dx[s];
						ray.d[1]=dy[s];
						ray.d[2]=dz[s];
						oc_traverse( the_volume, &ray, &m, &z );
						hits += ( m != 0 );
					}
					
					o[rr] = ( hits*hits << 4 ) / AO_SAMP;
				}
				
				u = _mm_load_si128( (void*) o );
				u = _mm_or_si128( u, _mm_slli_si128( u, 1 ) );
				u = _mm_or_si128( u, _mm_slli_si128( u, 2 ) );
				
				#if 1
				_mm_store_si128( (void*) pixel_p, u );
				#else
				p = _mm_load_si128( (void*) pixel_p );
				p = _mm_subs_epu8( p, u );
				_mm_store_si128( (void*) pixel_p, p );
				#endif
			}
		}
	}
}
