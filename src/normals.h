#ifndef _NORMALS_H
#define _NORMALS_H

typedef unsigned char PNormal;
void unpack_normal( PNormal t, float n[3] );
PNormal pack_normal( float const n[3] );

#endif
