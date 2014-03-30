#pragma once
#ifndef _VOXELS_CSG_H
#define _VOXELS_CSG_H
#include "aabb.h"
#include "voxels.h"

/* CSG operations. Use a nonzero material to add and 0 to subtract */
void csg_sphere( Octree *oc, const Sphere *sph, int mat );
void csg_box( Octree *oc, const aabb3f *box, int mat );

#endif
