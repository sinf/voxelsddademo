#include <xmmintrin.h>
#include "types.h"

static __m128 vec_len_squared( __m128 x, __m128 y, __m128 z )
{
	x = _mm_mul_ps( x, x );
	y = _mm_mul_ps( y, y );
	z = _mm_mul_ps( z, z );
	return _mm_add_ps( _mm_add_ps( x, y ), z );
}

static __m128 dot_prod( __m128 ax, __m128 ay, __m128 az, __m128 bx, __m128 by, __m128 bz )
{
	ax = _mm_mul_ps( ax, bx );
	ay = _mm_mul_ps( ay, by );
	az = _mm_mul_ps( az, bz );
	return _mm_add_ps( _mm_add_ps( ax, ay ), az );
}

static void normalize_vec( void* restrict px, void* restrict py, void* restrict pz, __m128 x, __m128 y, __m128 z )
{
	__m128 t;
	t = dot_prod( x, y, z, x, y, z );
	t = _mm_rsqrt_ps( t );
	_mm_store_ps( px, _mm_mul_ps( x, t ) );
	_mm_store_ps( py, _mm_mul_ps( y, t ) );
	_mm_store_ps( pz, _mm_mul_ps( z, t ) );
}

static void translate_vector( void* restrict out_x, void* restrict out_y, void* restrict out_z, __m128 const x, __m128 const y, __m128 const z, __m128 const * restrict m )
{
	_mm_store_ps( out_x, dot_prod( x, y, z, m[0], m[1], m[2] ) );
	_mm_store_ps( out_y, dot_prod( x, y, z, m[3], m[4], m[5] ) );
	_mm_store_ps( out_z, dot_prod( x, y, z, m[6], m[7], m[8] ) );
}
