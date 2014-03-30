#include <stdlib.h>
#include <xmmintrin.h>
#include <string.h>
#include "voxels.h"
#include "oc_traverse2.h"
#include "render_core.h"

#define OCTREE_DEPTH_HARDLIMIT 15
#define ALLOW_DEBUG_VISUALS 1

extern int oc_show_travel_depth;
extern int oc_detail_level;

#define BUF_RAY_SIZE (6*sizeof(float)+sizeof(uint32))
typedef struct {
	size_t len;
	float *tmin[3], *tmax[3];
	uint32 *ray_id;
	float *buf_end;
} RayBuffer;

static RayBuffer make_buffer( float *p, size_t max_len )
{
	RayBuffer buf;
	int x;
	for( x=0; x<3; x++ ) {
		buf.tmin[x] = p; p += max_len;
		buf.tmax[x] = p; p += max_len;
	}
	buf.ray_id = (uint32*) p;
	buf.buf_end = p + max_len;
	buf.len = 0;
	return buf;
}

static RayBuffer filter_rays( RayBuffer const in[], int child_id, uint8 const dead_bits[] )
{
	RayBuffer o;
	size_t r, n;
	
	o = make_buffer( in->buf_end, in->len );
	n = 0;
	
	for( r=0; r<in->len; r++ )
	{
		unsigned g;
		float near, far;
		size_t id = in->ray_id[r];
		
		if ( dead_bits[id] )
			continue;
		
		o.ray_id[n] = id;
		
		/* Subdivide the interval */
		for( g=0; g<3; g++ )
		{
			float tmin, tmax, split;
			
			split = ( in->tmin[g][r] + in->tmax[g][r] ) * 0.5f;
			
			if ( child_id & ( 4 >> g ) ) {
				tmin = split;
				tmax = in->tmax[g][r];
			} else {
				tmin = in->tmin[g][r];
				tmax = split;
			}
			
			o.tmin[g][n] = tmin;
			o.tmax[g][n] = tmax;
		}
		
		/* Test for AABB intersection */
		near = fmax( fmax( o.tmin[0][n], o.tmin[1][n] ), o.tmin[2][n] );
		far = fmin( fmin( o.tmax[0][n], o.tmax[1][n] ), o.tmax[2][n] );
		if ( near > far || far < 0.0f ) {
			continue;
		}
		
		n++;
	}
	
	o.len = n;
	return o;
}

static void process_leaf( const OctreeNode *node,
	int octree_level,
	RayBuffer *rays,
	uint8 out_mat[],
	float out_depth[],
	uint8 dead_bits[] )
{
	uint8 mat = node->mat;
	size_t r;
	
	if ( !mat )
		return;
	
	#if ALLOW_DEBUG_VISUALS
	if ( oc_show_travel_depth )
	{
		mat = octree_level + 2;
		mat &= MATERIAL_BITMASK;
	}
	#endif
	
	for( r=0; r<rays->len; r++ )
	{
		size_t id;
		float a, b, c, z;
		
		id = rays->ray_id[r];
		a = rays->tmin[0][r];
		b = rays->tmin[1][r];
		c = rays->tmin[2][r];
		z = fmax( a, fmax( b, c ) );
		
		out_mat[id] = mat;
		out_depth[id] = z;
		dead_bits[id] = 1; /* kill this ray */
	}
}

static void traverse_branch( const OctreeNode *node,
	int octree_level,
	RayBuffer const *rays,
	uint8 out_mat[],
	float out_depth[],
	int iter,
	uint8 dead_bits[] )
{
	int m;
	
	octree_level--;
	
	for( m=0; m<8; m++ )
	{
		int child_id = m ^ iter;
		OctreeNode *child = node->children + child_id;
		RayBuffer subset = filter_rays( rays, m, dead_bits ); /* why pass m instead of child_id??? */
		if ( !subset.len ) {
			/* none of the rays intersected this child node */
			continue;
		}
		if ( !octree_level || !child->children )
			process_leaf( child, octree_level, &subset, out_mat, out_depth, dead_bits );
		else
			traverse_branch( child, octree_level, &subset, out_mat, out_depth, iter, dead_bits );
	}
}

void oc_traverse_dac( const Octree oc[1],
	size_t ray_count,
	float const *ray_o[3],
	float const *ray_d[3],
	uint8 out_mat[],
	float out_depth[] )
{
	uint8 *dead_bits;
	RayBuffer buffers[8];
	float *buf_mem;
	
	size_t r;
	float aabb_min = 0;
	float aabb_max = oc->size;
	int t;
	
	dead_bits = calloc( ray_count, 1 );
	buf_mem = aligned_alloc( 16, 8 * BUF_RAY_SIZE * ray_count * OCTREE_DEPTH_HARDLIMIT );
	
	buffers[0] = make_buffer( buf_mem, ray_count );
	for( t=1; t<8; t++ )
		buffers[t] = make_buffer( buffers[t-1].buf_end, ray_count );
	for( t=0; t<7; t++ )
		buffers[t].buf_end = buffers[7].buf_end;
	
	for( r=0; r<ray_count; r++ )
	{
		size_t o;
		int k;
		int iter = 0;
		float near, far;
		
		out_mat[r] = 0;
		out_depth[r] = MAX_DEPTH_VALUE;
		
		for( k=0; k<3; k++ ) {
			if ( ray_d[k][r] < 0 ) {
				/* Ray direction is negative. Child nodes should be traversed in reverse order along this axis. */
				iter |= 4 >> k;
			}
		}
		
		o = buffers[iter].len;
		
		for( k=0; k<3; k++ ) {
			float t0, t1, tmin, tmax;
			float d;
			
			/* Intersect ray with the bounding box of the root node */
			d = ray_d[k][r];
			t0 = ( aabb_min - ray_o[k][r] ) / d;
			t1 = ( aabb_max - ray_o[k][r] ) / d;
			
			tmin = fmin( t0, t1 );
			tmax = fmax( t0, t1 );
			
			buffers[iter].tmin[k][o] = tmin;
			buffers[iter].tmax[k][o] = tmax;
		}
		
		near = fmax( fmax( buffers[iter].tmin[0][o], buffers[iter].tmin[1][o] ), buffers[iter].tmin[2][o] );
		far = fmin( fmin( buffers[iter].tmax[0][o], buffers[iter].tmax[1][o] ), buffers[iter].tmax[2][o] );
		
		if ( near < far && far > 0.0f )
		{
			buffers[iter].len += 1;
			buffers[iter].ray_id[o] = r;
		}
	}
	
	for( t=0; t<8; t++ ) {
		traverse_branch( &oc->root,
			oc->root_level - oc_detail_level + 1,
			&buffers[t],
			out_mat, out_depth,
			t, dead_bits );
	}
	
	free( buf_mem );
	free( dead_bits );
}
