///
///	@file image.c	@brief image loading functions.
///
///	Copyright (c) 2009 - 2011, 2021 by Lutz Sammer.	 All Rights Reserved.
///
///	Contributor(s):
///
///	License: AGPLv3
///
///	This program is free software: you can redistribute it and/or modify
///	it under the terms of the GNU Affero General Public License as
///	published by the Free Software Foundation, either version 3 of the
///	License.
///
///	This program is distributed in the hope that it will be useful,
///	but WITHOUT ANY WARRANTY; without even the implied warranty of
///	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///	GNU Affero General Public License for more details.
///
///	$Id$
//////////////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE	1		///< fix stpcpy strchrnul

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <endian.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <xcb/xcb_aux.h>

#ifdef USE_ICON				// {

#include <setjmp.h>			// needed by jpg/png

#include "image.h"
    // I don't want to include complete draw.h
    /// Look up a color by name
extern int ColorGetByName(const char *, xcb_coloritem_t *);

///
///	@defgroup image The image loading module.
///
///	This module contains the image loading functions.
///
///	These functions and all dependencies are only available if compiled
///	with #USE_ICON.
///
///	The internal format is uncompressed ARGB.
///
///	@see #USE_JPEG #USE_PNG #USE_XPM for supported "image" formats.
///
/// @{

/**
**	Create a new random filled image.
**
**	@param width	width of image in pixel
**	@param height	height of image in pixel
**
**	@returns allocated image structure, NULL on failures.
*/
static Image *ImageNew(unsigned width, unsigned height)
{
    Image *image;

    image = malloc(sizeof(*image) - sizeof(image->Data) + width * height * 4);
    if (!image) {
	Error("out of memory\n");
	return image;
    }
    image->Width = width;
    image->Height = height;

    return image;
}

/**
**	Destroy an image.
**
**	@param image	image to destroy
*/
void ImageDel(Image * image)
{
    if (image) {
	free(image);
    }
}

#ifdef USE_JPEG				// {

#include <jpeglib.h>

    // funny construct to have noreturn attribute with stupid gcc
static void ImageJPEGErrorExit(j_common_ptr cinfo) __attribute__((noreturn));

/**
**	Called from jpeg on errors by error_exit method.
**
**	@param cinfo	JPEG common info structure
*/
static void ImageJPEGErrorExit(j_common_ptr cinfo)
{
    longjmp(cinfo->client_data, 1);
}

/**
**	Load a JPEG image from given file name.
**
**	@param name	file name to open JPEG file
**	@param width	requested output size (or 0)
**	@param height	requested output size (or 0)
**
**	@returns loaded ARGB image, NULL if failure.
**
**	@see file:///usr/share/doc/jpeg-6b-r8/libjpeg.doc.bz2
**	@see file:///usr/share/doc/jpeg-7/libjpeg.txt.bz2
*/
Image *ImageLoadJPEG0(const char *name, unsigned width, unsigned height)
{
    FILE *fd;
    Image *image;
    jmp_buf jmpbuf;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPARRAY buffer;
    uint8_t *argb;

    // open the file
    if (!(fd = fopen(name, "rb"))) {
	Debug(3, "%s: can't open %s: %s\n", __FUNCTION__, name,
	    strerror(errno));
	return NULL;
    }

    image = NULL;
    if (setjmp(jmpbuf)) {		// return here, if any errors
	if (image) {
	    ImageDel(image);
	}

	jpeg_destroy_decompress(&cinfo);
	fclose(fd);

	return NULL;
    }
    // setup the error handler
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = ImageJPEGErrorExit;

    // prepare to load the file
    jpeg_create_decompress(&cinfo);
    cinfo.client_data = jmpbuf;
    jpeg_stdio_src(&cinfo, fd);

    // check the header
    jpeg_read_header(&cinfo, TRUE);

    // setup decoder for greater speed
    cinfo.do_fancy_upsampling = FALSE;
    cinfo.do_block_smoothing = FALSE;
    cinfo.dct_method = JDCT_IFAST;
    cinfo.dither_mode = JDITHER_ORDERED;

    //
    //	prescale to requested output size
    //
    if (width || height) {
	int scale_w;
	int scale_h;

	// jpeg docs says: n 1/8 .. 16/8 are supported
	scale_w = (width * cinfo.scale_denom) / cinfo.image_width;
	scale_h = (height * cinfo.scale_denom) / cinfo.image_height;

	cinfo.scale_num = scale_w;
	if (scale_h > scale_w) {
	    cinfo.scale_num = scale_h;
	}
	if (cinfo.scale_num >= 16) {
	    cinfo.scale_num = 16;
	} else if (cinfo.scale_num < 1) {
	    cinfo.scale_num = 1;
	}
#if 0
	cinfo.scale_num = 1;

	scale_w = cinfo.image_width / width;
	scale_h = cinfo.image_height / height;

	cinfo.scale_denom = scale_w;
	if (scale_h < scale_w) {
	    cinfo.scale_denom = scale_h;
	}
	if (cinfo.scale_denom >= 8) {
	    cinfo.scale_denom = 8;
	} else if (cinfo.scale_denom < 1) {
	    cinfo.scale_denom = 1;
	} else if (cinfo.scale_denom == 3) {
	    cinfo.scale_denom = 2;
	} else if (cinfo.scale_denom == 5) {
	    cinfo.scale_denom = 4;
	} else if (cinfo.scale_denom == 6) {
	    cinfo.scale_denom = 4;
	} else if (cinfo.scale_denom == 7) {
	    cinfo.scale_denom = 4;
	}
#endif
    }
    jpeg_calc_output_dimensions(&cinfo);

    // start decompression
    jpeg_start_decompress(&cinfo);

    // dimension is known after jpeg_start_decompress
    buffer =
	cinfo.mem->alloc_sarray((j_common_ptr) & cinfo, JPOOL_IMAGE,
	cinfo.output_width * cinfo.output_components, cinfo.rec_outbuf_height);
    if (!(image = ImageNew(cinfo.output_width, cinfo.output_height))) {
	jpeg_destroy_decompress(&cinfo);
	fclose(fd);
	return NULL;
    }
    argb = image->Data;

    // FIXME: loading can be done quicker, f.e. more line at once...

    // read lines
    switch (cinfo.output_components) {
	case 1:			// grayscale
	    while (cinfo.output_scanline < cinfo.output_height) {
		int x;

		jpeg_read_scanlines(&cinfo, buffer, 1);
		for (x = 0; x < image->Width; x++) {
		    *argb++ = 0xFF;	// alpha
		    *argb++ = GETJSAMPLE(buffer[0][x]);
		    *argb++ = GETJSAMPLE(buffer[0][x]);
		    *argb++ = GETJSAMPLE(buffer[0][x]);
		}
	    }
	    break;
	default:			// RGB
	    while (cinfo.output_scanline < cinfo.output_height) {
		int x;

		jpeg_read_scanlines(&cinfo, buffer, 1);
		for (x = 0; x < image->Width; x++) {
		    *argb++ = 0xFF;	// alpha
		    *argb++ = GETJSAMPLE(buffer[0][x * 3 + 0]);
		    *argb++ = GETJSAMPLE(buffer[0][x * 3 + 1]);
		    *argb++ = GETJSAMPLE(buffer[0][x * 3 + 2]);
		}
	    }
	    break;
    }

    // clean up
    jpeg_destroy_decompress(&cinfo);
    fclose(fd);

    return image;
}

/**
**	Load a JPEG image from given file name.
**
**	@param name	file name to open JPEG file
**
**	@returns loaded ARGB image, NULL if failure.
*/
static Image *ImageLoadJPEG(const char *name)
{
    return ImageLoadJPEG0(name, 0, 0);
}

#else // }{ USE_JPEG

    /// Dummy for load a JPEG image from given file name.
#define ImageLoadJPEG(name)	NULL

#endif // } USE_JPEG

#ifdef USE_PNG				// {

/// png includes setjmp self and fails if already included!
#define PNG_SKIP_SETJMP_CHECK
#include <png.h>

/**
**	Load a PNG image from given file name.
**
**	@param name	file name to open PNG file.
**
**	@returns loaded ARGB image, NULL if failure.
**
**	@see http://libpng.org/pub/png/libpng-manual.html
*/
static Image *ImageLoadPNG(const char *name)
{
    FILE *fd;
    Image *image;
    uint8_t header[8];
    int n;
    png_structp png_ptr;
    png_infop info_ptr;
    png_infop end_info;
    uint8_t **rows;
    int bit_depth;
    int color_type;
    unsigned u;
    png_uint_32 width;
    png_uint_32 height;
    int has_alpha;

    // open the file
    if (!(fd = fopen(name, "rb"))) {
	Debug(3, "can't open %s: %s\n", name, strerror(errno));
	return NULL;
    }
    n = fread(header, 1, sizeof(header), fd);
    if (n != sizeof(header) || png_sig_cmp(header, 0, sizeof(header))) {
	fclose(fd);
	return NULL;
    }

    if (!(png_ptr =
	    png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
	Warning("couldn't create read struct for PNG %s\n", name);
	fclose(fd);
	return NULL;
    }
    if (!(info_ptr = png_create_info_struct(png_ptr))) {
	Warning("couldn't create info struct for PNG %s\n", name);
	png_destroy_read_struct(&png_ptr, NULL, NULL);
	fclose(fd);
	return NULL;
    }
    if (!(end_info = png_create_info_struct(png_ptr))) {
	Warning("couldn't create end info struct for PNG %s\n", name);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(fd);
	return NULL;
    }

    image = NULL;
    if (setjmp(png_jmpbuf(png_ptr))) {
	Warning("error reading PNG %s\n", name);
	if (image) {
	    ImageDel(image);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	if (fd) {
	    fclose(fd);
	}
	return NULL;
    }

    png_init_io(png_ptr, fd);
    png_set_sig_bytes(png_ptr, sizeof(header));

    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
	NULL, NULL, NULL);

    // check alpha
    has_alpha = 0;
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)
	|| color_type == PNG_COLOR_TYPE_RGB_ALPHA
	|| color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
	has_alpha = 1;
    }
    // expand palette -> RGB if necessary
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
	png_set_palette_to_rgb(png_ptr);
	// expand gray (w/reduced bits) -> 8-bit RGB if necessary
    } else if (color_type == PNG_COLOR_TYPE_GRAY
	|| color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
	png_set_gray_to_rgb(png_ptr);
	if (bit_depth < 8) {
	    png_set_expand_gray_1_2_4_to_8(png_ptr);
	}
    }
    // expand transparency entry -> alpha channel if present
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
	png_set_tRNS_to_alpha(png_ptr);
    }
    // png_set_expand(png_ptr);

    if (bit_depth > 8) {		// reduce 16bit to 8bit
	png_set_strip_16(png_ptr);
    } else if (bit_depth < 8) {		// pack all pixels at byte boundaries
	png_set_packing(png_ptr);
    }
    // we want ARGB
#if __BYTE_ORDER == __LITTLE_ENDIAN
    png_set_swap_alpha(png_ptr);
    if (!has_alpha) {
	png_set_filler(png_ptr, 0xFF, PNG_FILLER_BEFORE);
    }
#else
    png_set_bgr(png_ptr);
    if (!has_alpha) {
	png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
#endif

    png_read_update_info(png_ptr, info_ptr);

    if (png_get_rowbytes(png_ptr, info_ptr) != 4 * width) {
	Warning("png image result must be 4 bytes / pixel\n");
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	if (fd) {
	    fclose(fd);
	}
	return NULL;
    }
    if (!(image = ImageNew(width, height))) {
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	if (fd) {
	    fclose(fd);
	}
	return NULL;
    }
    //
    //	Prepare rows for png_read_image
    //
    rows = alloca(height * sizeof(*rows));
    for (n = 0, u = 0; u < height; n += width * 4, u++) {
	rows[u] = &image->Data[n];
    }

    png_read_image(png_ptr, rows);

    // alpha premultiply
    for (u = 0; u < width * height * 4; u += 4) {
	image->Data[u + 1] =
	    (image->Data[u + 1] * (image->Data[u + 0] + 1)) >> 8;
	image->Data[u + 2] =
	    (image->Data[u + 2] * (image->Data[u + 0] + 1)) >> 8;
	image->Data[u + 3] =
	    (image->Data[u + 3] * (image->Data[u + 0] + 1)) >> 8;
    }

    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    fclose(fd);

    return image;
}

#else // }{ USE_PNG

    /// Dummy for load a PNG image from given file name.
#define ImageLoadPNG(name)	NULL

#endif // } !USE_PNG

#ifdef USE_XPM				// {

/**
**	Parse XPM data.
**
**	@param data	an array of strings with XPM image data
*/
static void *ImageParseXPM(const char *const *data)
{
    int width;
    int height;
    int colors;
    int chars_per_color;
    char dummy;
    const char *line;
    uint8_t *argb;
    struct _color_
    {
	char code[5];			// chars per color id
	union
	{
	    uint8_t argb[4];		// argb color pixel
	    xcb_lookup_color_cookie_t cookie;	// cookie for lookup
	};
    } *ctable;
    Image *image;
    int i;

    // header
    if (sscanf(*data, "%d %d %d %d %c", &width, &height, &colors,
	    &chars_per_color, &dummy) != 4) {
	Error("XPM: malformed header\n");
	return NULL;
    }
    if (colors < 1 || colors > 32766) {
	Error("XPM: wrong number of colors %d\n", colors);
	return NULL;
    }
    if (chars_per_color < 1 || chars_per_color > 5) {
	Error("XPM: file with %d-characters per pixel not supported\n",
	    chars_per_color);
	return NULL;
    }
    if (width > 8192 || width < 1) {
	Error("XPM: invalid image width of %d\n", width);
	return NULL;
    }
    if (height > 8192 || height < 1) {
	Error("XPM: invalid image height of %d\n", height);
	return NULL;
    }
    data++;

    Debug(3, "XPM: %dx%d #%d*%d\n", width, height, colors, chars_per_color);

    ctable = alloca(colors * sizeof(*ctable));

    //
    //	Read color table, send lookup color requests
    //
    for (i = 0; i < colors; i++) {
	int j;

	line = *data;
	for (j = 0; j < chars_per_color; ++j) {
	    int id;

	    // note: fetch here is int8_t, not char for unsigned char platforms
	    id = (int8_t) line[j];
	    ctable[i].code[j] = id < 32 ? 32 : id;
	}
	line += chars_per_color;

	while (*line) {			// multiple choices for color
	    int n;
	    int type;

	    n = strspn(line, " \t");	// skip white space
	    type = line[n];
	    if (!type) {
		continue;		// whitespace upto end of line
	    }
	    if (type != 's' && type != 'c' && type != 'm') {
		Error("unknown XPM pixel type '%c' in \"%s\"\n", type, *data);
		type = 'c';
	    }
	    line += n + 1;
	    n = strspn(line, " \t");	// skip white space
	    line += n;
	    if (!strncasecmp(line, "none", 4)) {
		line += 4;
		ctable[i].argb[0] = 0x00;
		ctable[i].argb[1] = 0x00;
		ctable[i].argb[2] = 0x00;
		ctable[i].argb[3] = 0x00;
		continue;
	    } else {
		int j;
		char colstr[256];
		xcb_coloritem_t c;

		j = 0;
		// copy color upto next " c " or " m "
		while (*line && !(j && (line[-1] == ' ' || line[-1] == '\t')
			&& (*line == 'c' || *line == 'm') && (line[1] == ' '
			    || line[1] == '\t'))) {
		    if (j < 254) {	// short enough for buffer
			colstr[j++] = *line++;
		    }
		}
		colstr[j] = '\0';
		if (type == 's') {	// ignore symbolic
		    continue;
		}

		if (xcb_aux_parse_color(colstr, &c.red, &c.green, &c.blue)
		    || ColorGetByName(colstr, &c)) {
		    ctable[i].argb[0] = 0xFF;
		    ctable[i].argb[1] = c.red / 256;
		    ctable[i].argb[2] = c.green / 256;
		    ctable[i].argb[3] = c.blue / 256;
		} else {
		    Error("unparsable XPM color spec: '%s' in \"%s\"\n",
			colstr, *data);
		}
		Debug(4, "Color %5.*s %c %s=%d %d %d\n", chars_per_color,
		    ctable[i].code, type, colstr, c.red, c.green, c.blue);
	    }
	}
	data++;
    }
    //
    //	Color table, get lookup color replies
    //
    for (i = 0; i < colors; i++) {
	// FIXME: ...
    }

    image = ImageNew(width, height);
    argb = image->Data;

    i = 0;
    while ((line = *data++)) {
	while (*line) {
	    int j;

	    // search color code
	    // FIXME: faster lookup for 1 and 2 character codes
	    for (j = 0; j < colors; ++j) {
		if (!strncmp(line, ctable[j].code, chars_per_color)) {
		    argb[0] = ctable[j].argb[0];
		    argb[1] = ctable[j].argb[1];
		    argb[2] = ctable[j].argb[2];
		    argb[3] = ctable[j].argb[3];
		    break;
		}
	    }
	    if (j == colors) {
		Error("color '%s' not found in XPM\n", line);
	    }
	    argb += 4;
	    if (argb > image->Data + width * height * 4) {
		Error("too many pixel in XPM\n");
		return image;
	    }
	    line += chars_per_color;
	}
	if (argb == image->Data + width * height * 4) {
	    return image;
	}
    }
    if (argb != image->Data + width * height * 4) {
	Error("too few pixel in XPM\n");
    }

    return image;
}

/**
**	Load an image from XPM data.
**
**	@param data XPM image data
**
**	@returns A new image or NULL if there were errors.
*/
Image *ImageFromData(const char *const *data)
{
    return ImageParseXPM(data);
}

/**
**	Load a XPM image from given file name.
**
**	@param name	file name to open XPM file.
**
**	@returns loaded ARGB image, NULL if failure.
*/
static Image *ImageLoadXPM(const char *name)
{
    FILE *fd;
    char *buffer;
    struct stat sb;
    Image *image;
    char header[10];
    char *s;
    const char **data;
    int n;

    // open the file
    if (!(fd = fopen(name, "rb"))) {
	Debug(3, "%s: can't open %s: %s\n", __FUNCTION__, name,
	    strerror(errno));
	return NULL;
    }
    // fast check if the file is XPM
    if (fread(header, 9, 1, fd) != 1) {
	fclose(fd);
	return NULL;
    }
    if (strncmp("/* XPM */", header, 9)) {
	fclose(fd);
	return NULL;
    }
    rewind(fd);

    if (fstat(fileno(fd), &sb)) {
	Error("Can't stat XPM file '%s': %s\n", name, strerror(errno));
	fclose(fd);
	return NULL;
    }
    // use malloc, latest system (linux or glib) didn't like big alloca
    if (!(buffer = malloc(sb.st_size + 1))) {
	Error("Out of memory for XPM file\n");
	fclose(fd);
	return NULL;
    }
    if ((off_t) fread(buffer, 1, sb.st_size, fd) != sb.st_size) {
	Error("fread XPM file failed\n");
	free(buffer);
	fclose(fd);
	return NULL;
    }
    buffer[sb.st_size] = '\0';
    fclose(fd);

    //
    //	parse buffer into array of lines
    //
    n = 0;
    data = malloc(n * sizeof(*data));
    s = buffer;
    while (*s) {
	// only take "... lines
	s += strspn(s, "\t\n\r ");
	if (*s == '"') {
	    data = realloc(data, ++n * sizeof(*data));
	    data[n - 1] = ++s;
	    s = strchrnul(s, '"');
	    if (*s == '"') {		// remove any behind ..."
		*s++ = '\0';
	    }
	}
	s = strchrnul(s, '\n');
    }

    image = ImageParseXPM(data);

    free(data);
    free(buffer);

    return image;
}

#else // }{ USE_XPM

    /// Dummy for load a XPM image from given file name.
#define ImageLoadXPM(name)	NULL

#endif // } !USE_XPM

/**
**	Create an image from ARGB data.
**
**	ARGB with high byte being A, low byte being B.	Data is in rows, left
**	to right and top to bottom.
**
**	Used in EMWH hint _NET_WM_ICON.
**
**	@param width	width of ARGB data
**	@param height	height of ARGB data
**	@param data	ARGB data [width * height]
**
**	@returns image from the ARGB data.
*/
Image *ImageFromARGB(unsigned width, unsigned height, const uint32_t * data)
{
    Image *image;
    uint8_t *argb;
    const uint32_t *end;

    image = ImageNew(width, height);
    argb = image->Data;
    for (end = data + width * height; data < end; data++) {
	*argb++ = *data >> 24;
	*argb++ = *data >> 16;
	*argb++ = *data >> 8;
	*argb++ = *data;
    }
    return image;
}

/**
**	Load an image from the specified file.
**
**	@param name	file containing the image.
**
**	@return A new image node (NULL if the image could not be loaded).
*/
Image *ImageLoadFile(const char *name)
{
    Image *image;

    if (!name) {
	return NULL;
    }
    // attempt to load the file as a JPEG image
    if ((image = ImageLoadJPEG(name))) {
	return image;
    }
    // attempt to load the file as a PNG image
    if ((image = ImageLoadPNG(name))) {
	return image;
    }
    // attempt to load the file as an XPM image
    if ((image = ImageLoadXPM(name))) {
	return image;
    }

    return NULL;
}

/// @}

#endif // } USE_ICON
