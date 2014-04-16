#include <emmintrin.h>

enum {
	BYTE_VEC_LEN = 16
};

/* Byte vector */
struct Bytev
{
	__m128i x;
	
	Bytev operator + ( Bytev y ) { return _mm_adds_epu8( x, y.x ); } /* saturating add */
	Bytev operator - ( Bytev y ) { return _mm_subs_epu8( x, y.x ); } /* saturating sub */
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
};

static Bytev set1( int x ) { Bytev b; b.x = _mm_set1_epi8( x ); return b; }
static Bytev min( Bytev a, Bytev b ) { Bytev c; c.x = _mm_min_epu8( a.x, b.x ); return c; }
static Bytev max( Bytev a, Bytev b ) { Bytev c; c.x = _mm_max_epu8( a.x, b.x ); return c; }

/* Returns bits from a where bits in test are 1 */
static Bytev choose( Bytev a, Bytev b, Bytev test )
{
	Bytev c;
	c.x = _mm_or_si128( _mm_and_si128( a, test ), _mm_andnot_si128( test, b ) );
	return c;
}

/* Returns 0xFF if x,y,z inside sphere */
Bytev sphere( Bytev x, Bytev y, Bytev z, int x0, int y0, int z0, int r )
{
	x = x - set1( x0 );
	y = y - set1( y0 );
	z = z - set1( z0 );
	return ( x*x + y*y + z*z ) < set1( r );
}

/* Returns 0xFF if x,y,z inside box */
Bytev box( Bytev x, Bytev y, Bytev z, int x0, int y0, int z0, int x1, int y1, int z1 )
{
	return x > set1( x0 ) & x < set1( x1 )
	& y > set1( y0 ) & y < set1( y1 )
	& z > set1( z0 ) & z < set1( z1 );
}

/* Can be used to produce chessboard-like pattern when maski has a high bit set */
Bytev checkers( Bytev x, Bytev y, Bytev z, int mask )
{
	Bytev m = set1( maski );
	return x & m ^ y & m ^ z & m;
}
