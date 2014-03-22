#include <stdlib.h>
#include <stdio.h>

#define VOXEL_INTERNALS 1
#include "voxels.h"
#include "voxels_io.h"

typedef size_t (*IO_Func)(void*, size_t, size_t, FILE*);
typedef struct FileHeader
{
	#define CURRENT_FILE_VERSION 1
	uint32 version;
	uint32 mat_size;
	int32 root_level;
} FileHeader;

#if MATERIAL_BITS > 7
#error Material takes more than 7 bits; can not write to file
#endif

static void process_node( FILE *file, IO_Func pack, Octree *oc, OctreeNode *node, const FileHeader *header )
{
	unsigned have_children = ( node->children != NULL );
	
	uint8 data = ( have_children & 1 ) << 7;
	data |= ( node->mat & MATERIAL_BITMASK );
	
	pack( &data, 1, 1, file );
	
	have_children = data >> 7;
	node->mat = data & MATERIAL_BITMASK;
	
	if ( have_children )
	{
		int n;
		oc_expand_node( oc, node );
		for( n=0; n<8; n++ )
			process_node( file, pack, oc, &node->children[n], header );
	}
}

static void dump_info( Octree *oc )
{
	int res = 1 << oc->root_level;
	
	printf(
		"Level: %d\n"
		"Resolution: %dx%dx%d\n"
		"Nodes: %u\n",
		oc->root_level,
		res, res, res,
		oc->num_nodes );
}

Octree *oc_read( FILE *file )
{
	Octree *oc;
	FileHeader header;
	printf( "Loading octree...\n" );
	
	if ( fread( &header, sizeof(FileHeader), 1, file ) != 1 )
	{
		printf( "Error: file is empty\n" );
		return NULL;
	}
	
	if ( header.version != CURRENT_FILE_VERSION )
	{
		printf( "Error: wrong file version: %d (supported=%d)\n", header.version, CURRENT_FILE_VERSION );
		return NULL;
	}
	
	if ( header.mat_size > sizeof(Material_ID) )
	{
		printf( "Error: too large material indexes (%d)\n", header.mat_size );
		return NULL;
	}
	
	oc = oc_init( header.root_level );
	process_node( file, fread, oc, &oc->root, &header );
	dump_info( oc );
	return oc;
}

void oc_write( FILE *file, Octree *oc )
{
	FileHeader header;
	header.version = CURRENT_FILE_VERSION;
	header.mat_size = sizeof( Material_ID );
	header.root_level = oc->root_level;
	
	printf( "Writing octree...\n" );
	fwrite( &header, sizeof(FileHeader), 1, file );
	
	process_node( file, (IO_Func) fwrite, oc, &oc->root, &header );
	dump_info( oc );
}

