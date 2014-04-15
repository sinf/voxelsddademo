#include <math.h>
#include "normals.h"

#define N NUM_NORMALS

void unpack_normal( PNor k, float xp[1], float yp[1], float zp[1] )
{
	const float
		a = 1.0 / N - 1.0,
		b = 2.0 / N,
		c = M_PI * ( 3.0 - sqrt( 5.0 ) );
	
	float z, r, p;
	
	z = b * k + a;
	p = k * c;
	r = sqrt( 1.0f - z*z );
	
	*xp = cos( p ) * r;
	*yp = sin( p ) * r;
	*zp = z;
}

PNor pack_normal( float x, float y, float z )
{
	const float
		a = N / 2.0,
		b = 2.0 / ( N * M_PI ),
		c = 0.5 + N/2.0 + 1.0/N;
	
	return z*a + atan2( y, x )*b + c;
	
	/*
	float k0 = ( z - a ) / b;
	float k = k0 + atan2( y, x ) / M_PI * b - ( 1.0 / N );
	return k;
	*/
}

#if 0
PNor pack_normal( float x, float y, float z )
{
	/* Trial and error. Couldn't be any slower */
	float min_dist = 1000;
	unsigned n1 = 0;
	unsigned n;
	for( n=0; n<N; n++ )
	{
		float nx, ny, nz, dx, dy, dz, dist;
		unpack_normal( n, &nx, &ny, &nz );
		dx = x - nx;
		dy = y - ny;
		dz = z - nz;
		dist = dx*dx + dy*dy + dz*dz;
		if ( dist < min_dist ) {
			min_dist = dist;
			n1 = n;
		}
	}
	return n1;
}
#endif
