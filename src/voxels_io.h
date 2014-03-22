#pragma once
#ifndef _VOXELS_IO_H
#define _VOXELS_IO_H
#include "voxels.h"

/* None of the arguments must be NULL */

Octree *oc_read( FILE *fp );
void oc_write( FILE *fp, Octree *oc );

#endif
