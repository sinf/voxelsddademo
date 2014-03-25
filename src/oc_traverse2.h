#ifndef _OCTREE_DAC_TRAVERSE_H
#define _OCTREE_DAC_TRAVERSE_H

#include <stddef.h>
#include "materials.h"
#include "voxels.h"

void oc_traverse_dac( const Octree oc[1],
	size_t ray_count,
	float const *ray_o[3],
	float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[] );

#endif
