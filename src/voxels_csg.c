#include <string.h>
#include <stdlib.h>

#define VOXEL_INTERNALS 1
#include "voxels.h"
#include "voxels_csg.h"

static const float NOR_BLEED = 1.0f;

typedef int (*CSG_Function)( const aabb3f *, const void * );
typedef void (*Normal_Function)( float nor[3], const void *, float px, float py, float pz );

typedef struct CSG_Object
{
	CSG_Function overlaps_aabb; /* Must not be NULL. Returns NO_TOUCH, INSIDE or OVERLAP, depending on how the AABB collides the object */
	Normal_Function calc_normal; /* Computes a normal vector */
	void const *data;
	int material;
} CSG_Object;

static void update_normal( OctreeNode *node, aabb3f *box, const CSG_Object *csg_obj, int mat )
{
	csg_obj->calc_normal( node->nor, csg_obj->data,
	( box->min[0] + box->max[0] ) * 0.5f,
	( box->min[1] + box->max[1] ) * 0.5f,
	( box->min[2] + box->max[2] ) * 0.5f );
	if ( mat < 0 && !node->nor[0] && !node->nor[1] && !node->nor[2] ) {
		node->nor[0] = -node->nor[0];
		node->nor[1] = -node->nor[1];
		node->nor[2] = -node->nor[2];
	}
}

static void csg_operation( Octree *oc, OctreeNode *node, int level, const vec3i node_pos, const CSG_Object *csg_obj )
{
	aabb3f node_bounds;
	int size = 1 << level;
	int mat = csg_obj->material;
	OverlapStatus overlap;
	int u;
	
	get_node_bounds( &node_bounds, node_pos, size );
	overlap = csg_obj->overlaps_aabb( &node_bounds, csg_obj->data );
	
	if ( overlap == NO_TOUCH )
	{
		/* This octree node does not even touch the CSG object. Ignore. */
		return;
	}
	
	if ( overlap == INSIDE )
	{
		/* The node is completely inside the CSG object. No subdivision needed. */
		oc_collapse_node( oc, node );
		if ( mat >= 0 ) node->mat = mat;
		update_normal( node, &node_bounds, csg_obj, mat ); /** memset( node->nor, 0, sizeof node->nor ); **/
		return;
	}
	
	if ( level == 0 )
	{
		/* Leaf node and overlaps the CSG object. Mark as solid.
		And since this is a leaf node it does not need to be collapsed */
		if ( mat >= 0 ) node->mat = mat;
		update_normal( node, &node_bounds, csg_obj, mat );
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
		if ( (int) child->mat != mat || child->children )
		{
			/* Children don't share the same material or one of the children is not a leaf -> can not have duplicate data. */
			node->mat = get_mode_material( node );
			return;
		}
	}
	
	/* Delete duplicates */
	oc_collapse_node( oc, node );
	if ( mat >= 0 ) node->mat = mat;
	update_normal( node, &node_bounds, csg_obj, mat );
}

static void calc_sphere_normal( float nor[3], const Sphere *s, float x, float y, float z )
{
	float t;
	
	x = x - s->o[0];
	y = y - s->o[1];
	z = z - s->o[2];
	
	t = sqrt( x*x + y*y + z*z );
	x /= t;
	y /= t;
	z /= t;
	
	nor[0] = x;
	nor[1] = y;
	nor[2] = z;
}

static void calc_box_normal( float nor[3], const aabb3f *box, float x, float y, float z )
{	
	x = fabs( x - ( box->min[0] + box->max[0] ) * 0.5f );
	y = fabs( y - ( box->min[1] + box->max[1] ) * 0.5f );
	z = fabs( z - ( box->min[2] + box->max[2] ) * 0.5f );
	
	if ( x < z ) {
		nor[2] = 0;
		if ( x < y ) {
			nor[0] = x < 0 ? -1 : 1;
			nor[1] = 0;
		} else {
			nor[0] = 0;
			nor[1] = y < 0 ? -1 : 1;
		}
	} else {
		nor[0] = 0;
		if ( y < z ) {
			nor[1] = y < 0 ? -1 : 1;
			nor[2] = 0;
		} else {
			nor[1] = 0;
			nor[2] = z < 0 ? -1 : 1;
		}
	}
}

void csg_sphere( Octree *oc, const Sphere *sph, Material_ID mat )
{
	const vec3i root_pos = {0, 0, 0};
	CSG_Object ob;
	
	ob.overlaps_aabb = (CSG_Function) aabb_sphere_overlap;
	ob.calc_normal = (Normal_Function) calc_sphere_normal;
	ob.data = sph;
	ob.material = mat;
	csg_operation( oc, &oc->root, oc->root_level, root_pos, &ob );
	
	if ( !mat ) {
		Sphere sph2 = *sph;
		sph2.r += NOR_BLEED; /* Since we're digging instead of filling, write flipped normals for the exposed surface */
		ob.data  = &sph2;
		ob.material = -1; /* Negative value prevents materials from being altered */
		csg_operation( oc, &oc->root, oc->root_level, root_pos, &ob );
	}
}

void csg_box( Octree *oc, const aabb3f *box, Material_ID mat )
{
	const vec3i root_pos = {0, 0, 0};
	CSG_Object ob;
	
	ob.overlaps_aabb = (CSG_Function) aabb_aabb_overlap;
	ob.calc_normal = (Normal_Function) calc_box_normal;
	ob.data = box;
	ob.material = mat;
	csg_operation( oc, &oc->root, oc->root_level, root_pos, &ob );
	
	if ( !mat ) {
		aabb3f box2;
		int k;
		for( k=0; k<3; k++ ) {
			box2.min[k] = box->min[k] - NOR_BLEED;
			box2.max[k] = box->max[k] + NOR_BLEED;
		}
		ob.material = -1;
		ob.data = &box2;
		csg_operation( oc, &oc->root, oc->root_level, root_pos, &ob );
	}
}
