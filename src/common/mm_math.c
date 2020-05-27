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

static __m128i mm_randi( void *state )
{
	__m128i x = _mm_load_si128( state );
	
#if 0
	const __m128i a=_mm_set1_epi16( 27893 ), c=_mm_set1_epi16( 7777 );
	/* Basic 16-bit linear congruential generator. Kinda bad but also very very fast */
	x = _mm_mullo_epi16( a, x );
	x = _mm_add_epi16( c, x );
#else
	/* This one passed many dieharder tests. More operations though */
	const __m128i
	a = _mm_set1_epi32( 1664525 ),
	b = _mm_set1_epi16( 7777 ),
	c = _mm_set1_epi32( 1013904223 );
	
	__m128i y = _mm_mul_epu32( x, a );
	x = _mm_add_epi16( x, b );
	x = _mm_xor_si128( _mm_slli_epi64( x, 27 ), y );
	x = _mm_add_epi32( x, c );
#endif
	
	_mm_store_si128( state, x );
	return x;
}

/* Returns "low" quality random floats in range [0,1[
Should compile to just 8 instructions even without optimization enabled */
static __m128 mm_rand( void *my_128bit_state )
{
	__m128i x;
	__m128 f;
	
	/* Generate random bits */
	x = mm_randi( my_128bit_state );
	
	x = _mm_srli_epi32( x, 9 ); /* clear sign and exponent */
	x = _mm_or_si128( x, _mm_set1_epi32( 0x40000000 ) ); /* [0,2[ */
	f = _mm_castsi128_ps( x );
	f = _mm_sub_ps( f, _mm_set1_ps( 3.0f ) ); /* [-1,1[ */
	
	return f;
}
