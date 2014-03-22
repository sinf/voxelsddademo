/* Update log:
?.?.2011: First version of loadbmp created.
3.1.2012: Fixed row_size and row_padding calculations. Now works with all image shapes and dimensions.
*/

/* TODO:
load_bmp fails to load a 32x29 uncompressed 4bit image because row_size is calculated incorrectly. Fix this!
Support BMPv4 and BMPv5 headers?
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "loadbmp.h"

/* Always pass BMP_VERBOSE to load_bmp() when nonzero */
#define FORCE_VERBOSE 0

typedef struct {
	/* 14 bytes */
	uint16_t identifier;
	uint32_t filesize;
	uint16_t creator1;
	uint16_t creator2;
	uint32_t bmp_offset;
} BITMAPFILEHEADER;

enum {
	BI_RGB=0,
	BI_RLE8,
	BI_RLE4,
	BI_BITFIELDS,
	BI_JPEG,
	BI_PNG,
	BI_ALPHABITFIELDS
};

typedef struct {
	/* 40 bytes */
	uint32_t size;
	int32_t width;
	int32_t height;
	uint16_t nplanes;
	uint16_t bitspp;
	uint32_t compress_type;
	uint32_t bmp_bytesz;
	int32_t hres;
	int32_t vres;
	uint32_t ncolors;
	uint32_t nimpcolors;
} BITMAPINFOHEADER;

const char BMP_ERROR_MESSAGES[N_BMP_ERRORS][BMP_ERRMSG_LEN] = {
	"success",
	"failed to open file",
	"identifier mismatch (this is not a BMP)",
	"header size is not 40 (unsupported BMP version)",
	"unsupported bit depth (must be 1, 4 or 8)",
	"unsupported compression type",
};

static void dump_header( BITMAPINFOHEADER *h )
{
	printf( "BITMAPINFOHEADER dump:\n" );
	printf( "  header size: %u\n", h->size );
	printf( "  size: %dx%d\n", h->width, h->height );
	printf( "  nplanes: %u\n", h->nplanes );
	printf( "  bitspp: %u\n", h->bitspp );
	printf( "  compress type: 0x%02x\n", h->compress_type );
	printf( "  bmp size: %u\n", h->bmp_bytesz );
	#if 0
	printf( "  resolution: %dx%d\n", h->hres, h->vres );
	#endif
	printf( "  ncolors: %u\n", h->ncolors );
	printf( "  nimpcolors: %u\n", h->nimpcolors );
}

static void fill_pattern( int img_w, int array_boundary, uint32_t *pixels, int x, int y, int count, uint32_t colors[2] )
{
	int offset;
	int n, c;
	
	offset = y * img_w + x;
	
	if ( offset < 0 || offset >= array_boundary )
		return;
	
	c = 1;
	for( n=(offset+count-1); n>=offset; n-- )
	{
		pixels[n] = colors[c];
		c = !c;
	}
}

static int rle_decompress( FILE *fp, const BITMAPINFOHEADER *infoh, const uint32_t *palette, uint32_t *rgb_pixels )
{
	int img_area;
	uint8_t a, b;
	int x, y;
	uint32_t colors[2];
	
	if ( infoh->compress_type != BI_RLE8 && infoh->compress_type != BI_RLE4 )
		return 0;
	
	img_area = infoh->width * infoh->height;
	x = 0;
	y = infoh->height - 1;
	
	while( y >= 0 )
	{
		a = fgetc( fp );
		b = fgetc( fp );
		
		if ( a > 0 )
		{
			/*
			-- Encoded mode --
			RLE8:
				a: count
				b: index 
			RLE4:
				a: count
				b: 2 indexes
			*/
			
			if ( infoh->compress_type == BI_RLE4 )
			{
				/* RLE4 */
				colors[0] = palette[ (b & 0xF0) >> 4 ];
				colors[1] = palette[ b & 0x0F ];
				fill_pattern( infoh->width, img_area, rgb_pixels, x, y, a, colors );
			}
			else
			{
				/* RLE8 */
				colors[0] = colors[1] = palette[b];
				fill_pattern( infoh->width, img_area, rgb_pixels, x, y, a, colors );
			}
			
			x += a;
		}
		else
		{
			/*
				a: 0
				b: whattodo
			*/
			
			if ( b == 0 )
			{
				/* end of line */
				x = 0;
				y--;
			}
			else if ( b == 1 )
			{
				/* end of bitmap */
				break;
			}
			else if ( b == 2 )
			{
				/* delta */
				x += fgetc( fp );
				y -= fgetc( fp );
			}
			else
			{
				/*
				-- Absolute mode --
					a: 0x03
					b: count
				*/
				
				int n, k;
				int pad;
				
				if ( infoh->compress_type == BI_RLE4 )
				{
					/* RLE4 */
					pad = b % 2;
					b >>= 1;
					for( n=0; n<b; n++ )
					{
						a = fgetc( fp );
						k = y * infoh->width + x;
						rgb_pixels[k] = palette[ (a&0xF0) >> 4 ];
						rgb_pixels[k+1] = palette[ a & 0x0F ];
						x += 2;
					}	
					if ( pad )
						fgetc( fp );
				}
				else
				{
					/* RLE8 */
					for( n=0; n<b; n++ )
					{
						a = fgetc( fp );
						rgb_pixels[ y * infoh->width + x ] = palette[a];
						x++;
					}
				}
				
				if ( b % 2 )
					fgetc( fp );
			}
		}
	}
	
	return 1;
}

static int unpack_row( int image_width, int bits_per_pixel, uint8_t *buffer, int bufsize, uint32_t *rgb_pixels, uint32_t *palette )
{
	uint8_t byte;
	int index;
	int index2;
	int x, b, n;
	
	switch( bits_per_pixel )
	{
		case 1:
			/* 8 pixels per byte */
			n = 0;
			for( x=0; x<bufsize; x++ )
			{
				byte = buffer[x];
				for( b=7; b>=0; b-- )
				{
					index = ( byte & (1<<b) ) >> b;
					rgb_pixels[n++] = palette[index];
					if ( n>= image_width )
						return 1;
				}
			}
			break;
			
		case 4:
			/* 2 pixels per byte */
			n = 0;
			for( x=0; x<bufsize; x++ )
			{
				byte = buffer[x];
				index = ( byte & 0xF0 ) >> 4;
				index2 = ( byte & 0x0F );
				
				rgb_pixels[n++] = palette[index];
				if ( n >= image_width )
					break;
				
				rgb_pixels[n++] = palette[index2];
				if ( n >= image_width )
					break;
			}
			break;
			
		case 8:
			/* 1 pixel per byte */
			for( x=0; x<bufsize; x++ )
				*(rgb_pixels++) = palette[ buffer[x] ];
			break;
			
		default:
			return 0;
	}
	
	return 1;
}

/* the main bmp loading routine */
BMP_ERROR_CODE load_bmp( const char *filename, uint32_t **output, int *width_p, int *height_p, uint32_t flags )
{
	BMP_ERROR_CODE errcode = 0;
	FILE *fp = NULL;
	
	/* temporary BMP stuff */
	BITMAPFILEHEADER fileh;
	BITMAPINFOHEADER infoh;
	uint32_t *palette = NULL;
	uint32_t num32;
	
	/* output (RGB + 1 pad byte) */
	size_t rgb_pixels_size;
	uint32_t *rgb_pixels = NULL;
	
	/* used while reading pixels */
	uint8_t *buffer = NULL;
	uint32_t *row_p;
	int row_size;
	int row_padding;
	int cur_row;
	int x;
	
	#if FORCE_VERBOSE
	flags |= BMP_VERBOSE;
	#endif
	
	if ( flags & BMP_VERBOSE )
		printf( "Loading BMP: %s\n", filename );
	
	#define HANDLE_ERROR( expression, error_code ) \
		if (( expression )) { errcode = error_code; goto error_label; }
	
	/* ........................................ */
	fp = fopen( filename, "rb" );
	HANDLE_ERROR( fp == NULL, BMP_CANTOPEN );
	
	/* Identifier should be "BM" */
	fread( &fileh, 14, 1, fp );
	HANDLE_ERROR( fileh.identifier != 0x4D42, BMP_BADMAGIC );
	
	/* read BITMAPINFOHEADER.size */
	fread( &num32, 4, 1, fp );
	HANDLE_ERROR( num32 != 40, BMP_UNSUP_H );
	
	/* read BITMAPINFOHEADER */
	fseek( fp, -4, SEEK_CUR );
	fread( &infoh, num32, 1, fp );
	
	/* print out useful info */
	if ( flags & BMP_VERBOSE )
		dump_header( &infoh );
	
	HANDLE_ERROR( infoh.bitspp > 8 && infoh.bitspp != 24, BMP_UNSUP_BPP );
	HANDLE_ERROR( infoh.compress_type > BI_RLE4, BMP_UNSUP_COMPR );
	
	/*
	if ( compression == BI_BITFIELDS )
		then read 3 or 4 bytes
	*/
	
	if ( infoh.bitspp <= 8 )
	{
		/* allocate palette */
		if ( flags & BMP_GRAYSCALE )
		{
			/* skip original palette */
			fseek( fp, infoh.ncolors << 2, SEEK_CUR );
			
			/* and generate a grayscale palette */
			palette = malloc( infoh.ncolors << 2 );
			for( num32=0; num32<infoh.ncolors; num32++ )
			{
				int value;
				value = num32 * ( 255 / infoh.ncolors );
				value = value & 0xFF;
				palette[num32] = value | ( value << 8 ) | ( value << 16 );
			}
		}
		else
		{
			/* read teh palette from file */
			palette = malloc( infoh.ncolors << 2 );
			fread( palette, 4, infoh.ncolors, fp );
		}
	}
	
	/* allocate additional 64 bytes to protect against buffer overflow */
	rgb_pixels_size = infoh.width * infoh.height * 4 + 64;
	rgb_pixels = malloc( rgb_pixels_size );
	memset( rgb_pixels, 0, rgb_pixels_size );
	
	if ( infoh.bitspp < 8 )
	{
		/* More than 1 pixel per byte */
		row_size = infoh.width * infoh.bitspp; /* bits per row */
		
		if ( row_size % 8 > 0 )
		{
			/* pad to multiples of 8 */
			row_size = row_size + ( 8 - ( row_size % 8 ) );
		}
		
		row_size /= 8; /* convert bits to bytes */
	}
	else
	{
		/* At least 1 byte per pixel */
		row_size = infoh.width * infoh.bitspp / 8;
	}
	
	row_padding = ( row_size % 4 == 0 ) ? 0 : 4 - ( row_size % 4 );
	buffer = malloc( row_size );
	
	if ( flags & BMP_VERBOSE )
	{
		puts( "Reading pixel data..." );
		printf( "  Row size: %d\n", row_size );
		printf( "  Row padding: %d\n", row_padding );
	}
	
	if ( infoh.compress_type == BI_RGB )
	{
		/* Uncompressed. Read one row at a time and convert to RGB on the fly */
		for( cur_row=0; cur_row<infoh.height; cur_row++ )
		{
			fread( buffer, row_size, 1, fp );
			fseek( fp, row_padding, SEEK_CUR );
			
			/* Image in a BMP is vertically flipped, so read rows in reverse order */
			row_p = rgb_pixels + ( infoh.height - 1 - cur_row ) * infoh.width;
			
			if ( infoh.bitspp <= 8 )
				unpack_row( infoh.width, infoh.bitspp, buffer, row_size, row_p, palette );
			else if ( infoh.bitspp == 24 )
			{
				int x;
				for( x=0; x<infoh.width; x++ )
					row_p[x] = *( (uint32_t*) (buffer+3*x) ) & 0xFFFFFF;
			}
		}
	}
	else
	{
		/* RLE4 or RLE8 */
		if ( !rle_decompress( fp, &infoh, palette, rgb_pixels ) )
			HANDLE_ERROR( 1, BMP_UNSUP_COMPR );
	}
	
	if ( palette )
		free( palette );
	
	free( buffer );
	fclose( fp );
	
	/* generate alpha channel */
	for( x=(infoh.width*infoh.height-1); x>=0; x-- )
	{
		uint32_t *pixel32 = rgb_pixels + x;
		uint8_t *pixel8 = (uint8_t*) pixel32;
		
		if ( flags & BMP_COLORKEY && rgb_pixels[x] == rgb_pixels[0] )
		{
			/* if this pixel is same as the very first pixel, it becomes transparent */
			pixel8[3] = 0;
		}
		else
		{
			if ( flags & BMP_REDALPHA )
			{
				/* use red channel as alpha channel */
				pixel8[3] = pixel8[0];
				/* and full intensity for RGB */
				pixel8[0] = pixel8[1] = pixel8[2] = 255;
			}
			else
			{
				/* constant alpha */
				pixel8[3] = 255;
			}
		}
	}
	
	/* success */
	*output = rgb_pixels;
	*width_p = infoh.width;
	*height_p = infoh.height;
	return BMP_SUCCESS;
	
	/* failure */
	error_label:;
	if ( fp ) fclose( fp );
	if ( palette ) free( palette );
	if ( rgb_pixels ) free( rgb_pixels );
	if ( buffer ) free( buffer );
	return errcode;
}
