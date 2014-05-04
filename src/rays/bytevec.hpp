#ifndef _BYTEVEC_H
#define _BYTEVEC_H

#include <emmintrin.h>

enum {
	BYTE_VEC_LEN = 16
};

struct Bytev;
typedef struct Bytev Bytev;

/* Byte vector */
struct Bytev
{
	__m128i x;
	
	Bytev(){};
	Bytev( __m128i value ) { x=value; }
	Bytev( int value ) { x=_mm_set1_epi8( value ); }
	void clear( void ) { x=_mm_setzero_si128(); }
	
	#if 0
	Bytev operator + ( Bytev y ) { return _mm_adds_epu8( x, y.x ); } /* saturating add */
	Bytev operator - ( Bytev y ) { return _mm_subs_epu8( x, y.x ); } /* saturating sub */
	#else
	Bytev operator + ( Bytev y ) { return _mm_add_epi8( x, y.x ); }
	Bytev operator - ( Bytev y ) { return _mm_sub_epi8( x, y.x ); }
	#endif
	
	Bytev operator & ( Bytev y ) { return _mm_and_si128( x, y.x ); }
	Bytev operator | ( Bytev y ) { return _mm_or_si128( x, y.x ); }
	Bytev operator ^ ( Bytev y ) { return _mm_xor_si128( x, y.x ); }
	
	/* Comparisons. These return 0xFF or 0 */
	Bytev operator == ( Bytev y ) { return _mm_cmpeq_epi8( x, y.x ); }
	Bytev operator > ( Bytev y ) { return _mm_cmpgt_epi8( x, y.x ); }
	Bytev operator < ( Bytev y ) { return _mm_cmpgt_epi8( y.x, x ); }
	
	/*  Computes a * b >> 8 */
	Bytev operator * ( Bytev y )
	{
		__m128i a0, b0, a1, b1, c0, c1;
		a1 = _mm_srli_epi16( x, 8 );
		b1 = _mm_srli_epi16( y.x, 8 );
		a0 = _mm_slli_epi16( x, 8 );
		b0 = _mm_slli_epi16( y.x, 8 );
		c0 = _mm_srli_epi16( _mm_mulhi_epu16( a0, b0 ), 8 );
		c1 = _mm_srli_epi16( _mm_mullo_epi16( a1, b1 ), 8 );
		c0 = _mm_packus_epi16( c0, c1 );
		c1 = _mm_srli_si128( c0, 8 );
		return _mm_unpacklo_epi8( c0, c1 );
	}
	
	/* Computes ( x << 8 ) * b1 >> 24 */
	Bytev operator * ( short b1 )
	{
		__m128i al, ah, b;
		
		b = _mm_set1_epi16( b1 );
		al = _mm_slli_epi16( x, 8 ); /* expand low byte to 16 bit */
		ah = x; /* ignore the low 8 bits; they only cause a small error */
		al = _mm_srli_epi16( _mm_mulhi_epu16( al, b ), 8 ); /* mulhi does a hidden right shift by 16 bits */
		ah = _mm_srli_epi16( _mm_mulhi_epu16( ah, b ), 8 );
		al = _mm_packus_epi16( al, ah );
		ah = _mm_srli_si128( al, 8 );
		return _mm_unpacklo_epi16( al, ah );
	}
	
	Bytev operator >> ( int y ) {
		__m128i m, z;
		m = _mm_set1_epi8( 0xFF >> y );
		z = _mm_srli_epi16( x, y );
		z = _mm_and_si128( z, m );
		return z;
	}
	
	Bytev operator << ( int y ) {
		__m128i m, z;
		m = _mm_set1_epi8( 0xFF << y );
		z = _mm_slli_epi16( x, y );
		z = _mm_and_si128( z, m );
		return z;
	}
	
	Bytev operator + ( int b ) { return x + _mm_set1_epi8( b ); }
	Bytev operator - ( int b ) { return x - _mm_set1_epi8( b ); }
	Bytev operator & ( int b ) { return x & _mm_set1_epi8( b ); }
	Bytev operator | ( int b ) { return x | _mm_set1_epi8( b ); }
	Bytev operator ^ ( int b ) { return x ^ _mm_set1_epi8( b ); }
	Bytev operator == ( int b ) { return _mm_cmpeq_epi8( _mm_set1_epi8( b ), x ); }
	Bytev operator > ( int b ) { return _mm_cmpgt_epi8( x, _mm_set1_epi8( b ) ); }
	Bytev operator < ( int b ) { return _mm_cmpgt_epi8( _mm_set1_epi8( b ), x ); }
	
	void operator += ( Bytev b ) { *this = *this + b; }
	void operator -= ( Bytev b ) { *this = *this - b; }
	void operator *= ( Bytev b ) { *this = *this * b; }
	void operator &= ( Bytev b ) { *this = *this & b; }
	void operator |= ( Bytev b ) { *this = *this | b; }
	void operator ^= ( Bytev b ) { *this = *this ^ b; }
	void operator <<= ( int b ) { *this = *this << b; }
	void operator >>= ( int b ) { *this = *this >> b; }
};

/**
static Bytev random( void )
{
	static uint32 state[] = {0x7ad6d567, 0xc47b484e, 0x8ff432ab, 0xd8bad6eb};
	const __m128i a = _mm_set1_epi16( 0xe9a9 );
	const __m128i c = _mm_set1_epi16( 
	
}
**/

static Bytev min( Bytev a, Bytev b ) { return _mm_min_epu8( a.x, b.x ); }
static Bytev max( Bytev a, Bytev b ) { return _mm_max_epu8( a.x, b.x ); }

/* Returns bits from a where bits in test are 1 */
static Bytev choose( Bytev a, Bytev b, Bytev test ) {
	return _mm_or_si128( _mm_and_si128( a.x, test.x ), _mm_andnot_si128( test.x, b.x ) );
}

/* Returns 0xFF if x,y,z inside sphere */
static Bytev sphere( Bytev x, Bytev y, Bytev z, int x0, int y0, int z0, int r )
{
	x = x - x0;
	y = y - y0;
	z = z - z0;
	return ( x*x + y*y + z*z ) < r;
}

/* Returns 0xFF if x,y,z inside box */
static Bytev box( Bytev x, Bytev y, Bytev z, int x0, int y0, int z0, int x1, int y1, int z1 )
{
	return x > x0 & x < x1
	& y > y0 & y < y1
	& z > z0 & z < z1;
}

/* Can be used to produce chessboard-like pattern when maski has a high bit set */
static Bytev checkers( Bytev x, Bytev y, Bytev z, int mask )
{
	Bytev m = Bytev( mask );
	return x & m ^ y & m ^ z & m;
}

#endif
