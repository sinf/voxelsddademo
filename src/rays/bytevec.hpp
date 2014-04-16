#include <emmintrin.h>

enum {
	BYTEVEC_LEN = 16
};

struct ByteVec
{
	__m128i x;
	
	ByteVec operator ++ ( ByteVec y ) { return _mm_adds_epu8( x, y.x ); } /* saturating add */
	ByteVec operator + ( ByteVec y ) { return _mm_add_epi8( x, y.x ); }
	ByteVec operator - ( ByteVec y ) { return _mm_sub_epi8( x, y.x ); }
	ByteVec operator & ( ByteVec y ) { return _mm_and_si128( x, y.x ); }
	ByteVec operator | ( ByteVec y ) { return _mm_or_si128( x, y.x ); }
	ByteVec operator ^ ( ByteVec y ) { return _mm_xor_si128( x, y.x ); }
	
	/* Comparisons. These return 0xFF or 0 */
	ByteVec operator == ( ByteVec y ) { return _mm_cmpeq_epi8( x, y.x ); }
	ByteVec operator > ( ByteVec y ) { return _mm_cmpgt_epi8( x, y.x ); }
	ByteVec operator < ( ByteVec y ) { return _mm_cmpgt_epi8( y.x, x ); }
	
	/*  Computes a * b >> 8 */
	ByteVec operator * ( ByteVec y )
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
