#include <stdlib.h>

#define VOXEL_INTERNALS 1
#include "voxels.h"
#include "voxels_csg.h"

typedef int (*CSG_Function)( const aabb3f *, const void * );
typedef struct CSG_Object
{
	/* Must not be NULL */
	CSG_Function overlaps_aabb;
	const void *data;
	Material_ID material;
} CSG_Object;

static void csg_operation( Octree *oc, OctreeNode *node, int level, const vec3i node_pos, const CSG_Object *csg_obj )
{
	aabb3f node_bounds;
	int size = 1 << level;
	Material_ID mat = csg_obj->material;
	OverlapStatus overlap;
	int u;
	
	get_node_bounds( &node_bounds, node_pos, size );
	overlap = csg_obj->overlaps_aabb( &node_bounds, csg_obj->data );
	
	if ( overlap == NO_TOUCH )
	{
		/* This octree chunk does not touch the CSG object. Ignore. */
		return;
	}
	else if ( overlap == INSIDE )
	{
		/* Node is completely inside the CSG object. No subdivision needed. */
		oc_collapse_node( oc, node );
		node->mat = mat;
		return;
	}
	
	if ( level == 0 )
	{
		/* Leaf node and overlaps the CSG object. Mark as solid.
		And since this is a leaf node it does not need to be collapsed */
		node->mat = mat;
		return;
	}
	
	/* Partial overlap with the CSG object and this is not a leaf node so do a recursive subdivision. */
	size = size >> 1;
	level = level - 1;
	oc_expand_node( oc, node );
	
	for( u=0; u<8; u++ )
	{
		vec3i p;
		int k;
		
		for( k=0; k<3; k++ )
			p[k] = node_pos[k] + ( OC_RECURSION_MASK[u][k] & size );
		
		csg_operation( oc, &node->children[u], level, p, csg_obj );
	}
	
	level = level + 1;
	mat = node->children[0].mat;
	
	/* Check if subnodes are the same */
	for( u=0; u<8; u++ )
	{
		OctreeNode *child = &node->children[u];
		if ( child->mat != mat || child->children )
		{
			/* Children don't share the same material or one of the children is not a leaf -> can not have duplicate data. */
			node->mat = get_mode_material( node );
			return;
		}
	}
	
	/* Delete duplicates */
	oc_collapse_node( oc, node );
	node->mat = mat;
}

void csg_sphere( Octree *oc, const Sphere *sph, Material_ID mat )
{
	const vec3i root_pos = {0, 0, 0};
	CSG_Object ob;
	
	ob.overlaps_aabb = (CSG_Function) aabb_sphere_overlap;
	ob.data = (void*) sph;
	ob.material = mat;
	
	csg_operation( oc, &oc->root, oc->root_level, root_pos, &ob );
}

void csg_box( Octree *oc, const aabb3f *box, Material_ID mat )
{
	const vec3i root_pos = {0, 0, 0};
	CSG_Object ob;
	
	ob.overlaps_aabb = (CSG_Function) aabb_aabb_overlap;
	ob.data = (void*) box;
	ob.material = mat;
	
	csg_operation( oc, &oc->root, oc->root_level, root_pos, &ob );
}
