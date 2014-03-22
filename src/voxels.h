#pragma once
#ifndef _VOXELS_H
#define _VOXELS_H
#include "types.h"
#include "vector.h"
#include "aabb.h"
#include "materials.h"

struct OctreeNode;
typedef struct OctreeNode
{
	/* Pointer to 8 child nodes (NULL for leaf nodes) */
	struct OctreeNode *children;
	/*	For leaf nodes:
			The material index.
		For non-leaf nodes:
			The most common (=mode) material in child nodes */
	Material_ID mat;
} OctreeNode;

typedef struct Octree
{
	unsigned num_nodes; /* All nodes including root node. Should never be 0. */
	int size; /* Bounding box size for root node; 1 << root_level */
	int root_level; /* Highest (root) octree level */
	OctreeNode root;
} Octree;

#ifdef VOXEL_INTERNALS
extern const int OC_RECURSION_MASK[8][3];
void oc_expand_node( Octree *oc, OctreeNode *node ); /* Allocate child nodes if NULL */
void oc_collapse_node( Octree *oc, OctreeNode *node ); /* Delete child nodes if have any */
void get_node_bounds( aabb3f *bounds, const vec3i pos, int size );
Material_ID get_mode_material( OctreeNode *node );
#endif

/* Memory management */
Octree *oc_init( int toplevel );
void oc_free( Octree *oc );
void oc_clear( Octree *oc, Material_ID m );

/* Use 0 to disable and 1 to enable */
extern int oc_show_travel_depth; /* Replaces material with travel depth. Won't exceed MAX_MATERIALS */
extern int oc_detail_level; /* Maximum recursion level. Used for global LOD. Use 0 for full detail  */

/* Ray traversal - used by raycaster.c and main.c */
void oc_traverse( const Octree *oc, const Ray *ray, Material_ID *output_mat, float *output_z );

#endif
