#pragma once
#ifndef _BITMAP_LOADER_H
#define _BITMAP_LOADER_H
#include <stdint.h>

#define LOADBMP_VERSION 1

typedef enum BMP_ERROR_CODE {
	BMP_SUCCESS=0,
	BMP_CANTOPEN, /* failed to open file */
	BMP_BADMAGIC, /* wrong file type or unsupported BMP version */
	BMP_UNSUP_H, /* unsupported BMP version */
	BMP_UNSUP_BPP, /* unsupported bit depth */
	BMP_UNSUP_COMPR, /* unsupported compression type (might be huffman, png or jpg) */
	N_BMP_ERRORS
} BMP_ERROR_CODE;

#define BMP_ERRMSG_LEN 64
extern const char BMP_ERROR_MESSAGES[N_BMP_ERRORS][BMP_ERRMSG_LEN];

#define BMP_VERBOSE 1
#define BMP_GRAYSCALE 2
#define BMP_COLORKEY 4
#define BMP_REDALPHA 8
extern BMP_ERROR_CODE load_bmp( const char *filename, uint32_t **output, int *width_p, int *height_p, uint32_t flags );

/*
Short description:
	Loads a BMP image in RGBA format.
	None of the arguments must be NULL.
Supported BMP formats and restrictions:
	-BMP version 1 (uses identifier 'BM' and header size is 40)
	-Bit depth: 1, 4, 8 or 24
	-Compression: none, RLE4 or RLE8
Arguments:
	filename: Filename of the image to be loaded
	output: Pointer to a 1D array, which will be allocated and filled with image data
	width_p, height_p: Image dimensions. Must not be NULL.
	flags: OR'd combination of BMP_* flags.
Flags:
	BMP_VERBOSE:	Enable debug messages (printed to stdout)
	BMP_GRAYSCALE:	A grayscale palette is generated instead of using the original palette
	BMP_COLORKEY:	Use the very first pixel (0,0) as transparency colorkey
	BMP_REDALPHA:	Use red as the alpha value and ignore other channels (255,255,255,red)
Return value:
	Error code, which can be used to retrieve error message from BMP_ERROR_MESSAGES.
	Return value is always 0 on success and >0 on error.
*/

#endif
