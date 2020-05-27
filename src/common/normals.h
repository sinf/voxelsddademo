#ifndef _NORMALS_H
#define _NORMALS_H

#define NUM_NORMALS 256
typedef unsigned char PNor;
void unpack_normal( PNor k, float xp[1], float yp[1], float zp[1] );
PNor pack_normal( float x, float y, float z );

#endif
