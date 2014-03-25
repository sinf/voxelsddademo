#include <math.h>
#include "normals.h"

/* Number of spiral revolutions multiplied by 2pi.
The distribution of normal vectors depends on this constant;
some values give more uniform distribution than others */
static const float u = 4100.8;

/* Maps integers from range 0-255 to almost uniformly distributed normal vectors (which lie on a spiral) */
void unpack_normal( PNormal t, float n[3] )
{
	float c, z;
	z = t/255.0f*2 - 1;
	c = sqrt( 1 - z*z );
	n[2] = z;
	n[0] = cos( t / 255.0f * u ) * c;
	n[1] = sin( t / 255.0f * u ) * c;
}

/* Finds an integer that (approximately) corresponds to the given normal vector (which must be normalized) */
PNormal pack_normal( float const n[3] )
{
	/* todo */
}
