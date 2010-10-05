///
///	@file dia.c	@brief dia show application functions
///
///	Copyright (c) 2009, 2010 by Lutz Sammer.  All Rights Reserved.
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

///
///	@defgroup dia The dia show application module.
///
///	This module handles showing pictures.  Didn't belongs into uwm,
///	but I needed a place to play around.
///
///	@todo keyboard handling is missing
///	@todo hide cursor
///	@todo recursive mode
///	@todo setting slide-show delay
///	@todo fullscreen zoom mode
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_DIA				// {

#include <sys/queue.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_pixel.h>
#include <xcb/xcb_image.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "event.h"
#include "client.h"

#include "draw.h"
#include "image.h"
#include "icon.h"

#include "dia.h"

// ------------------------------------------------------------------------ //

    /// default film strip thumbnail width
#define DIA_FILM_STRIP_WIDTH 170
    /// default film strip thumbnail height
#define DIA_FILM_STRIP_HEIGHT 96

    /// default index thumbnail width
#define DIA_INDEX_WIDTH 170
    /// default index thumbnail height
#define DIA_INDEX_HEIGHT 96

    /// default corner command width
#define DIA_CORNER_WIDTH (170 / 2)
    /// default corner command height
#define DIA_CORNER_HEIGHT (96 / 2)

#if 0

#define DIA_CACHE_SIZE	10

/**
**	Thumbnail image cache.
*/
typedef struct _dia_cache_
{
    const char *Name;			///< image file name
    Icon *Icon;				///< icon image
} DiaCache;

#endif

/**
**	Dia display layout mode.
*/
typedef enum _dia_layout_
{
    DIA_LAYOUT_SINGLE,			///< single picture
    DIA_LAYOUT_INDEX,			///< index page
} DiaLayout;

/**
**	Dia global variables structure typedef.
*/
typedef struct _dia_ Dia;

/**
**	Dia global variables structure.
*/
struct _dia_
{
    xcb_window_t Window;		///< dia-show window
    xcb_pixmap_t Pixmap;		///< current pixmap (double buffer)
    xcb_pixmap_t Working;		///< working pixmap (double buffer)

    uint16_t Width;			///< window width
    uint16_t Height;			///< window height

    uint16_t CornerWidth;		///< corner command area width
    uint16_t CornerHeight;		///< corner command area height

    uint16_t FilmStripWidth;		///< film-strip thumbnail width
    uint16_t FilmStripHeight;		///< film-strip thumbnail height

    uint16_t IndexWidth;		///< index-page thumbnail width
    uint16_t IndexHeight;		///< index-page thumbnail height

    uint16_t AspectNum;			///< thumbnail aspect ratio
    uint16_t AspectDen;			///< thumbnail aspect ratio

    uint16_t SlideShowDelay;		///< slide show delay

    unsigned NeedRedraw:1;		///< need to redraw
    unsigned SlideShow:1;		///< slide show running
    unsigned IndexLabel:1;		///< print file names on thumbnails

    DiaLayout Layout:1;			///< dia window layout
    int FilmStrip:1;			///< flag show film strip
    int Backdrop:1;			///< flag become desktop background

    uint32_t LastTick;			///< last handled tick
    int16_t LastX;			///< last x-coordinate
    int16_t LastY;			///< last y-coordinate
    int State;				///< state for pointer buttons handle

    char *Path;				///< current path
    Array *FilesInDir;			///< files in directory
    size_t CurrentIndex;		///< current file index
    size_t FirstIndex;			///< first index of film-strip/index
    // Array *DirTree;			///< directory tree

    // DiacAche Cache[DIA_CACHE_SIZE];	///< image cache
};

static Dia DiaVars[1];			///< dia-show globals

// ------------------------------------------------------------------------ //

/**
**	Filter for scandir, only image files.
**
**	@param dirent	current directory entry
**
**	@returns true if the #dirent is an image.
*/
static int DiaIsImage(const struct dirent *dirent)
{
    if (dirent->d_type == DT_REG) {	// only regular files
	size_t len;

	len = strlen(dirent->d_name);	// only supported image files
	if (len > 4 && (!strcasecmp(dirent->d_name + len - 4, ".jpg")
		|| !strcasecmp(dirent->d_name + len - 4, ".png"))) {
	    return 1;
	}
    }
    return 0;
}

/**
**	Read directory in array.
**
**	array[] -> names
**
**	@param name	directory name
**
**	@returns array with only image file names.
*/
static Array *DiaDirNew(const char *name)
{
    struct dirent **namelist;
    Array *array;
    size_t index;
    int n;
    int i;

    array = ArrayNew();
    n = scandir(name, &namelist, DiaIsImage, alphasort);

    if (n > 0) {
	index = 0;
	for (i = 0; i < n; ++i) {
	    ArrayIns(&array, index++, (size_t) strdup(namelist[i]->d_name));
	    free(namelist[i]);
	}
	free(namelist);
    } else {
	printf("Can't scan dir '%s'\n", name);
    }
    return array;
}

/**
**	Free directory array.
**
**	@param array	array with malloced strings
*/
static void DiaDirDel(Array * array)
{
    size_t index;
    size_t *value;

    index = 0;
    value = ArrayFirst(array, &index);
    while (value) {
	free((char *)*value);
	value = ArrayNext(array, &index);
    }
    ArrayFree(array);
}

// ------------------------------------------------------------------------ //

#if 0

/**
**	Get the scaled size of an icon for dia.
*/
static void DiaScaledIconSize(const Icon * icon, unsigned max_width,
    unsigned max_height, unsigned *width, unsigned *height)
{
    unsigned long ratio;

    // width to height
    ratio = (unsigned long)(icon->Image->Width * 65536) / icon->Image->Height;
    if (icon->Image->Width > icon->Image->Height) {	// compute size wrt width
	*width = (max_width * ratio) / 65536;
	*height = (*width * 65536) / ratio;
    } else {				// compute size wrt height
	*height = (max_height * 65536) / ratio;
	*width = (*height * ratio) / 65536;
    }
}
#endif

/**
**	Draw image.
**
**	@param image	image to render
**	@param x	x-offset on drawable to render image
**	@param y	y-offset on drawable to render image
**	@param width	width of image to display
**	@param height	height of image to display
**
**	@see IconDraw
**
**	@todo use shared memory, cache image
*/
static void DiaDrawImage(const Image * image, int x, int y, unsigned width,
    unsigned height)
{
    xcb_image_t *xcb_image;
    unsigned scale_x;
    unsigned scale_y;
    unsigned src_x;
    unsigned src_y;
    unsigned dst_x;
    unsigned dst_y;
    const uint8_t *argb;

    // create a temporary xcb_image for scaling
    xcb_image =
	xcb_image_create_native(Connection, width, height, (RootDepth == 1)
	? XCB_IMAGE_FORMAT_XY_BITMAP : XCB_IMAGE_FORMAT_Z_PIXMAP, RootDepth,
	NULL, 0L, NULL);

    // determine scale factor
    scale_x = (65536 * image->Width) / width;
    scale_y = (65536 * image->Height) / height;

    argb = image->Data;
    src_y = 0;

    //
    //	fast 32bit versions
    if (xcb_image->bpp == 32) {
	if (xcb_image->byte_order == XCB_IMAGE_ORDER_LSB_FIRST) {
	    for (dst_y = 0; dst_y < height; dst_y++) {
		int n;

		src_x = 0;
		n = (src_y / 65536) * image->Width;

		for (dst_x = 0; dst_x < width; dst_x++) {
		    int i;
		    xcb_coloritem_t color;

		    i = 4 * (n + (src_x / 65536));

		    color.red = (65535 * (argb[i + 1]) / 255);
		    color.green = (65535 * (argb[i + 2]) / 255);
		    color.blue = (65535 * (argb[i + 3]) / 255);
		    ColorGetPixel(&color);

		    xcb_image_put_pixel_Z32L(xcb_image, dst_x, dst_y,
			color.pixel);

		    src_x += scale_x;
		}

		src_y += scale_y;
	    }
	} else {
	    for (dst_y = 0; dst_y < height; dst_y++) {
		int n;

		src_x = 0;
		n = (src_y / 65536) * image->Width;

		for (dst_x = 0; dst_x < width; dst_x++) {
		    int i;
		    xcb_coloritem_t color;

		    i = 4 * (n + (src_x / 65536));

		    color.red = (65535 * (argb[i + 1]) / 255);
		    color.green = (65535 * (argb[i + 2]) / 255);
		    color.blue = (65535 * (argb[i + 3]) / 255);
		    ColorGetPixel(&color);

		    xcb_image_put_pixel_Z32M(xcb_image, dst_x, dst_y,
			color.pixel);

		    src_x += scale_x;
		}

		src_y += scale_y;
	    }
	}
    } else {
	for (dst_y = 0; dst_y < height; dst_y++) {
	    int n;

	    src_x = 0;
	    n = (src_y / 65536) * image->Width;

	    for (dst_x = 0; dst_x < width; dst_x++) {
		int i;
		xcb_coloritem_t color;

		i = 4 * (n + (src_x / 65536));

		color.red = (65535 * (argb[i + 1]) / 255);
		color.green = (65535 * (argb[i + 2]) / 255);
		color.blue = (65535 * (argb[i + 3]) / 255);
		ColorGetPixel(&color);

		xcb_image_put_pixel(xcb_image, dst_x, dst_y, color.pixel);

		src_x += scale_x;
	    }

	    src_y += scale_y;
	}
    }

    // render xcb_image to window
    xcb_image_put(Connection, DiaVars->Working, RootGC, xcb_image, x, y, 0);
    // release xcb_image
    xcb_image_destroy(xcb_image);
}

/**
**	Draw image,
**
**	@param icon	icon to render
**	@param x	x-offset on drawable to render icon
**	@param y	y-offset on drawable to render icon
**	@param width	width of icon to display
**	@param height	height of icon to display
*/
void DiaDrawIcon(Icon * icon, int x, int y, unsigned width, unsigned height)
{
    // we can't use render on these
    icon->UseRender = 0;
    // draw icon on window
    IconDraw(icon, DiaVars->Working, x, y, width, height);
}

/**
**	Show image.
**
**	@param name	file name of image to draw
**	@param x	x-offset on drawable to render image
**	@param y	y-offset on drawable to render image
**	@param width	width of image to display
**	@param height	height of image to display
**
**	@todo cache images, can use icon cache
*/
void DiaShowImage(const char *name, int x, int y, unsigned width,
    unsigned height)
{
    Icon *icon;

    // load icon
    if (!(icon = IconLoadSuffixed(DiaVars->Path, name, ""))) {
	Warning("dia image not found: \"%s\"", name);
	return;
    }
    DiaDrawIcon(icon, x, y, width, height);

    // we don't need icon anymore
    IconDel(icon);
}

/**
**	Show thumbnail.
**
**	Short cut for thumbnails
**
**	@param name	file name of thumnail to draw
**	@param x	x-offset on drawable to thumnail image
**	@param y	y-offset on drawable to thumnail image
**	@param width	width of thumnail to display
**	@param height	height of thumnail to display
**
**	@todo cache thumbnails
*/
void DiaShowThumbnail(const char *name, int x, int y, unsigned width,
    unsigned height)
{
    Image *image;
    char *buf;

    buf = alloca(strlen(name) + strlen(DiaVars->Path) + 2);
    stpcpy(stpcpy(stpcpy(buf, DiaVars->Path), "/"), name);
    if ((image = ImageLoadJPEG0(buf, width, height))) {
	int i;
	unsigned w;
	unsigned h;

	// keep image aspect ratio
	i = (image->Width * 65536) / image->Height;
	w = MIN(width * 65536, height * i);
	h = MIN(height, w / i);
	w = (h * i) / 65536;
	if (w < 1) {
	    w = 1;
	}
	if (h < 1) {
	    h = 1;
	}
	DiaDrawImage(image, x + (width - w) / 2, y + (height - h) / 2, w, h);
	ImageDel(image);
    } else {
	DiaShowImage(name, x, y, width, height);
    }
}

/**
**	Draw vertical film strip.
*/
static void DiaDrawVerticalFilmStrip(void)
{
    int x;
    int y;
    size_t index;
    size_t *value;

    x = 0;
    y = 0;
    index = DiaVars->FirstIndex;
    value = ArrayFirst(DiaVars->FilesInDir, &index);
    do {
	const char *file;

	if (!value) {
	    break;
	}
	file = (const char *)*value;
	if (!file) {
	    break;
	}
	// highlight current file
	if (index == DiaVars->CurrentIndex) {
	    xcb_rectangle_t rectangle;

	    rectangle.x = x;
	    rectangle.y = y;
	    rectangle.width = DiaVars->FilmStripWidth;
	    rectangle.height = DiaVars->FilmStripHeight;
	    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
		&Colors.PanelFG.Pixel);
	    xcb_poly_fill_rectangle(Connection, DiaVars->Working, RootGC, 1,
		&rectangle);
	}

	DiaShowThumbnail(file, x + 1, y + 1, DiaVars->FilmStripWidth - 2,
	    DiaVars->FilmStripHeight - 2);

	if (DiaVars->IndexLabel) {
	    FontDrawString(DiaVars->Working, &Fonts.Panel, 0UL, x + 2, y + 2,
		DiaVars->FilmStripWidth - 3, NULL, file);
	    FontDrawString(DiaVars->Working, &Fonts.Panel,
		Colors.PanelFG.Pixel, x + 1, y + 1,
		DiaVars->FilmStripWidth - 4, NULL, file);
	}

	value = ArrayNext(DiaVars->FilesInDir, &index);
	y += DiaVars->FilmStripHeight;
    } while (y < DiaVars->Height);
}

/**
**	Draw single image.
**
**	Single image with or without film strip.
*/
static void DiaDrawSingle(void)
{
    int x;
    int y;
    const char *file;

    x = 0;
    y = 0;
    if (DiaVars->FilmStrip) {
	DiaDrawVerticalFilmStrip();
	x = DiaVars->FilmStripWidth;
    }

    file = (const char *)ArrayGet(DiaVars->FilesInDir, DiaVars->CurrentIndex);
    if (file) {
	DiaShowImage(file, x, y, DiaVars->Width - x, DiaVars->Height - y);
    }
}

/**
**	Draw index page.
*/
static void DiaDrawIndex(void)
{
    int x;
    int y;
    size_t index;
    size_t *value;

    index = DiaVars->FirstIndex;
    value = ArrayFirst(DiaVars->FilesInDir, &index);
    // center index
    y = (DiaVars->Height % DiaVars->IndexHeight) / 2;
    do {
	x = (DiaVars->Width % DiaVars->IndexWidth) / 2;
	do {
	    if (value) {
		const char *file;

		file = (const char *)*value;
		if (!file) {
		    break;
		}
		// highlight current file
		if (index == DiaVars->CurrentIndex) {
		    xcb_rectangle_t rectangle;

		    rectangle.x = x;
		    rectangle.y = y;
		    rectangle.width = DiaVars->IndexWidth;
		    rectangle.height = DiaVars->IndexHeight;
		    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
			&Colors.PanelFG.Pixel);
		    xcb_poly_fill_rectangle(Connection, DiaVars->Working,
			RootGC, 1, &rectangle);
		}
		DiaShowThumbnail(file, x + 1, y + 1, DiaVars->IndexWidth - 2,
		    DiaVars->IndexHeight - 2);

		if (DiaVars->IndexLabel) {
		    FontDrawString(DiaVars->Working, &Fonts.Panel, 0UL, x + 2,
			y + 2, DiaVars->IndexWidth - 3, NULL, file);
		    FontDrawString(DiaVars->Working, &Fonts.Panel,
			Colors.PanelFG.Pixel, x + 1, y + 1,
			DiaVars->IndexWidth - 4, NULL, file);
		}

		value = ArrayNext(DiaVars->FilesInDir, &index);
	    }
	    x += DiaVars->IndexWidth;
	} while (x <= DiaVars->Width - DiaVars->IndexWidth);
	y += DiaVars->IndexHeight;
    } while (y <= DiaVars->Height - DiaVars->IndexHeight);
}

/**
**	Draw dia.
**
**	Modes:
**		- 1 picture
**		- film role + 1 picture
**		- n x m pictures
**
**	@param expose	true called from expose event.
*/
static void DiaDraw(int expose)
{
    uint32_t values[1];
    xcb_rectangle_t rectangle;

    if (IsNextEventAvail()) {
	DiaVars->NeedRedraw = 1;
	return;
    }
    if (expose) {
	xcb_aux_clear_window(Connection, DiaVars->Window);
	return;
    }

    DiaVars->NeedRedraw = 0;

    // clear with background
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	&Colors.PanelBG.Pixel);
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = DiaVars->Width;
    rectangle.height = DiaVars->Height;
    xcb_poly_fill_rectangle(Connection, DiaVars->Working, RootGC, 1,
	&rectangle);

    if (DiaVars->Layout == DIA_LAYOUT_SINGLE) {
	DiaDrawSingle();
    } else {
	DiaDrawIndex();
    }

    //	swap pixmaps
    values[0] = DiaVars->Working;
    xcb_change_window_attributes(Connection, DiaVars->Window,
	XCB_CW_BACK_PIXMAP, values);
    xcb_aux_clear_window(Connection, DiaVars->Window);
    DiaVars->Working = DiaVars->Pixmap;
    DiaVars->Pixmap = values[0];
}

/**
**	Close and destroy dia show window.
*/
static void DiaDestroy(void)
{
    free(DiaVars->Path);
    DiaVars->Path = NULL;

    DiaDirDel(DiaVars->FilesInDir);
    DiaVars->FilesInDir = NULL;

    xcb_destroy_window(Connection, DiaVars->Window);
    DiaVars->Window = XCB_NONE;

    xcb_free_pixmap(Connection, DiaVars->Pixmap);
    DiaVars->Pixmap = XCB_NONE;
    xcb_free_pixmap(Connection, DiaVars->Working);
    DiaVars->Working = XCB_NONE;
}

/**
**	Create the dia show window.
**
**	@param name	start file or directory.
*/
void DiaCreate(const char *name)
{
    uint32_t values[2];
    xcb_window_t window;
    xcb_size_hints_t size_hints;
    int x;
    int y;
    int width;
    int height;

    if (DiaVars->Window) {		// already running
	DiaDestroy();
	return;
    }

    DiaVars->Path = ExpandPath(name);
    DiaVars->FilesInDir = DiaDirNew(DiaVars->Path);
    DiaVars->CurrentIndex = DiaVars->FirstIndex = 0;
    DiaVars->NeedRedraw = 0;
    DiaVars->State = 0;

    x = 0;
    y = 0;
    width = RootWidth;
    height = RootHeight;

    window = xcb_generate_id(Connection);
    values[0] =
	XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(Connection, XCB_COPY_FROM_PARENT, window, RootWindow, x,
	y, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
	XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, values);

    // display pixmap
    DiaVars->Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, RootDepth, DiaVars->Pixmap, window, width,
	height);
    // working pixmap
    DiaVars->Working = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, RootDepth, DiaVars->Working, window, width,
	height);

    size_hints.flags = 0;
    xcb_size_hints_set_position(&size_hints, 0, x, y);
    xcb_set_wm_size_hints(Connection, window, WM_NORMAL_HINTS, &size_hints);
    xcb_set_wm_name(Connection, window, STRING, sizeof("Diashow") - 1,
	"Diashow");

#if 0
    // This approach fails due freeze on first button click
    self =
	ClientAddWindow(window, xcb_get_window_attributes_unchecked(Connection,
	    window), 0, 0);

    self->State |= WM_STATE_WMDIALOG;
    ClientFocus(self);
#endif

    DiaVars->Height = height;
    DiaVars->Width = width;
    DiaVars->Window = window;

    if (DiaVars->Backdrop) {
	// lower window
	values[0] = XCB_STACK_MODE_BELOW;
	xcb_configure_window(Connection, DiaVars->Window,
	    XCB_CONFIG_WINDOW_STACK_MODE, values);
    }
    xcb_map_window(Connection, window);
    DiaDraw(0);

    DiaVars->LastTick = GetMsTicks();
}

/**
**	First dia image.
*/
static void DiaFirst(void)
{
    size_t index;
    size_t *value;

    index = 0;
    value = ArrayFirst(DiaVars->FilesInDir, &index);
    if (value) {
	DiaVars->CurrentIndex = index;
	DiaVars->FirstIndex = index;
	DiaDraw(0);
    }
}

/**
**	Previous dia image.
*/
static void DiaPrev(void)
{
    size_t index;
    size_t *value;

    index = DiaVars->CurrentIndex;
    value = ArrayPrev(DiaVars->FilesInDir, &index);
    if (value) {
	DiaVars->CurrentIndex = index;
	if (DiaVars->CurrentIndex < DiaVars->FirstIndex) {
	    int n;

	    n = DiaVars->Height / DiaVars->FilmStripHeight;
	    // Move start of index
	    index = DiaVars->FirstIndex;
	    while (n--) {
		value = ArrayPrev(DiaVars->FilesInDir, &index);
		if (!value) {
		    break;
		}
	    }
	    DiaVars->FirstIndex = index;
	}
	DiaDraw(0);
    }
}

/**
**	Next dia image.
*/
static void DiaNext(void)
{
    size_t index;
    size_t *value;

    index = DiaVars->CurrentIndex;
    value = ArrayNext(DiaVars->FilesInDir, &index);
    if (value) {
	int n;

	DiaVars->CurrentIndex = index;
	n = DiaVars->Height / DiaVars->FilmStripHeight;
	if (DiaVars->CurrentIndex >= DiaVars->FirstIndex + n) {
	    int i;

	    index = DiaVars->FirstIndex + n - 1;
	    // Look how far we can move the end
	    for (i = 0; i < n; ++i) {
		value = ArrayNext(DiaVars->FilesInDir, &index);
		if (!value) {
		    break;
		}
	    }
	    // Move start of index
	    index = DiaVars->FirstIndex;
	    while (i--) {
		value = ArrayNext(DiaVars->FilesInDir, &index);
		if (!value) {
		    break;
		}
	    }
	    DiaVars->FirstIndex = index;
	}
	DiaDraw(0);
    }
}

/**
**	Scroll film strip up
*/
static void DiaScrollUp(void)
{
    size_t index;

    // Move start of index
    index = DiaVars->FirstIndex;
    if (ArrayPrev(DiaVars->FilesInDir, &index)) {
	DiaVars->FirstIndex = index;
    }
    DiaPrev();
}

/**
**	Scroll film strip down
*/
static void DiaScrollDown(void)
{
    size_t index;

    // Move end of index
    index =
	DiaVars->FirstIndex + DiaVars->Height / DiaVars->FilmStripHeight - 1;
    if (ArrayNext(DiaVars->FilesInDir, &index)) {
	index = DiaVars->FirstIndex;
	ArrayNext(DiaVars->FilesInDir, &index);
	DiaVars->FirstIndex = index;
    }

    DiaNext();
}

/**
**	Dia scroll index page up.
*/
static void DiaIndexScrollUp(int n)
{
    int i;
    size_t *value;
    size_t index;

    index = DiaVars->FirstIndex;
    // Look how far we can move the first
    for (i = 0; i < n; ++i) {
	value = ArrayPrev(DiaVars->FilesInDir, &index);
	if (!value) {
	    break;
	}
    }
    DiaVars->FirstIndex = index;
    // Move current
    index = DiaVars->CurrentIndex;
    n = i;
    for (i = 0; i < n; ++i) {
	value = ArrayPrev(DiaVars->FilesInDir, &index);
	if (!value) {
	    break;
	}
    }
    DiaVars->CurrentIndex = index;

    DiaDraw(0);
}

/**
**	Dia scroll index page down.
*/
static void DiaIndexScrollDown(int n)
{
    int i;
    size_t *value;
    size_t index;

    i = DiaVars->Width / DiaVars->IndexWidth;
    i *= DiaVars->Height / DiaVars->IndexHeight;
    index = DiaVars->FirstIndex + i - 1;

    // Look how far we can move the end
    for (i = 0; i < n; ++i) {
	value = ArrayNext(DiaVars->FilesInDir, &index);
	if (!value) {
	    break;
	}
    }
    // Move start of index
    index = DiaVars->FirstIndex;
    n = i;
    for (i = 0; i < n; ++i) {
	value = ArrayNext(DiaVars->FilesInDir, &index);
	if (!value) {
	    break;
	}
    }
    DiaVars->FirstIndex = index;
    // Move current
    index = DiaVars->CurrentIndex;
    for (i = 0; i < n; ++i) {
	value = ArrayNext(DiaVars->FilesInDir, &index);
	if (!value) {
	    break;
	}
    }
    DiaVars->CurrentIndex = index;

    DiaDraw(0);
}

/**
**	Dia previous index page.
*/
static void DiaPrevPage(void)
{
    int n;

    n = DiaVars->Width / DiaVars->IndexWidth;
    n *= DiaVars->Height / DiaVars->IndexHeight;

    DiaIndexScrollUp(n);
}

/**
**	Dia next index page.
*/
static void DiaNextPage(void)
{
    int n;

    n = DiaVars->Width / DiaVars->IndexWidth;
    n *= DiaVars->Height / DiaVars->IndexHeight;

    DiaIndexScrollDown(n);
}

#if 0

/**
**	Dia withdrawn.
*/
static void DiaWithdrawn(void)
{
    uint32_t values[2];

    DiaVars->CornerWidth = 4;
    DiaVars->CornerHeight = 4;
    DiaVars->FilmStripWidth = 4;
    DiaVars->FilmStripHeight = 3;
    DiaVars->IndexWidth = 16;
    DiaVars->IndexHeight = 12;

    DiaVars->Width = 64;
    values[0] = 64;
    DiaVars->Height = 64;
    values[1] = 64;
    xcb_configure_window(Connection, DiaVars->Window,
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
}
#endif

/**
**	Dia pointer button press/release command.
**
**	@param mask	pointer button number pressed or released
**	@param x	pointer x-coordinate (window relative)
**	@param y	pointer y-coordinate (window relative)
*/
static void DiaClickCommand(int mask, int x, int y)
{
    // north-east corner
    if (x > DiaVars->Width - DiaVars->CornerWidth && y < DiaVars->CornerHeight) {
	DiaDestroy();
	return;
    }

    if (DiaVars->Layout == DIA_LAYOUT_SINGLE) {
	int start_x;

	if (DiaVars->SlideShow) {	// stop slide show
	    DiaVars->SlideShow = 0;
	}
	//
	//	Single image
	//
	start_x = 0;
	if (DiaVars->FilmStrip) {
	    if (x < DiaVars->FilmStripWidth) {
		size_t *value;
		size_t index;

		//
		//	button 4/5 scroll-wheel
		//
		if (mask == XCB_BUTTON_INDEX_4) {
		    DiaScrollUp();
		    return;
		}
		if (mask == XCB_BUTTON_INDEX_5) {
		    DiaScrollDown();
		    return;
		}
		index = DiaVars->FirstIndex + y / DiaVars->FilmStripHeight;
		value = ArrayLast(DiaVars->FilesInDir, &index);
		if (value) {
		    DiaVars->CurrentIndex = index;
		    DiaDraw(0);
		}
		return;
	    }
	    start_x += DiaVars->FilmStripWidth;
	}
	//
	//	button 4/5 scroll-wheel
	//
	if (mask == XCB_BUTTON_INDEX_4) {
	    DiaPrev();
	    return;
	}
	if (mask == XCB_BUTTON_INDEX_5) {
	    DiaNext();
	    return;
	}
	// north-west corner
	if (x < start_x + DiaVars->CornerWidth && y < DiaVars->CornerHeight) {
	    DiaVars->Layout = DIA_LAYOUT_INDEX;
	    DiaDraw(0);
	    return;
	}
	// south-west corner
	if (x < start_x + DiaVars->CornerWidth
	    && y > DiaVars->Height - DiaVars->CornerHeight) {
	    DiaVars->FilmStrip = !DiaVars->FilmStrip;
	    // start with film-strip showing selected image
	    if (DiaVars->FirstIndex > DiaVars->CurrentIndex) {
		DiaVars->FirstIndex = DiaVars->CurrentIndex;
	    }
	    DiaDraw(0);
	    return;
	}
	// south-east corner
	if (x > DiaVars->Width - DiaVars->CornerWidth
	    && y > DiaVars->Height - DiaVars->CornerHeight) {
	    DiaVars->SlideShow = 1;
	    DiaVars->LastTick = GetMsTicks();
	    return;
	}
	// west side
	if (x < start_x + (DiaVars->Width - start_x) / 4) {
	    DiaPrev();
	    if (DiaVars->SlideShow) {
		DiaVars->LastTick =
		    GetMsTicks() + DiaVars->SlideShowDelay * 1000;
	    }
	    return;
	}
	// east side
	if (x >= DiaVars->Width - (DiaVars->Width - start_x) / 4) {
	    DiaNext();
	    if (DiaVars->SlideShow) {
		DiaVars->LastTick =
		    GetMsTicks() + DiaVars->SlideShowDelay * 1000;
	    }
	    return;
	}

    } else {
	int i;
	int j;

	//
	//	Index page
	//

	//
	//	button 4/5 scroll-wheel
	//
	if (mask == XCB_BUTTON_INDEX_4) {
	    DiaIndexScrollUp(DiaVars->Width / DiaVars->IndexWidth);
	    return;
	}
	if (mask == XCB_BUTTON_INDEX_5) {
	    DiaIndexScrollDown(DiaVars->Width / DiaVars->IndexWidth);
	    return;
	}
	// west side
	if (x < DiaVars->CornerWidth) {
	    DiaPrevPage();
	    return;
	}
	// east side
	if (x >= DiaVars->Width - DiaVars->CornerWidth) {
	    DiaNextPage();
	    return;
	}
	// select image
	i = (DiaVars->Width % DiaVars->IndexWidth) / 2;
	j = (DiaVars->Height % DiaVars->IndexHeight) / 2;
	if (x > i && y > j) {
	    i = x - i;
	    i /= DiaVars->IndexWidth;
	    j = y - j;
	    j /= DiaVars->IndexHeight;
	    if (i < DiaVars->Width / DiaVars->IndexWidth
		&& j < DiaVars->Height / DiaVars->IndexHeight) {
		size_t *value;
		size_t index;

		index =
		    DiaVars->FirstIndex + i +
		    j * (DiaVars->Width / DiaVars->IndexWidth);
		value = ArrayLast(DiaVars->FilesInDir, &index);
		if (value) {
		    DiaVars->CurrentIndex = index;
		    DiaVars->Layout = DIA_LAYOUT_SINGLE;
		    DiaVars->FilmStrip = 0;
		    DiaDraw(0);
		    return;
		}
	    }
	}
    }
}

#define DIA_MOVE_DIVIDER	16	///< divider for move gestures

/**
**	Dia pointer button move command.
**
**	@param x	pointer x-coordinate (window relative)
**	@param y	pointer y-coordinate (window relative)
**	@param move_x	how far pointer moved left/right
**	@param move_y	how far pointer moved up/down
*/
static void DiaMoveCommand(int x, int
    __attribute__ ((unused)) y, int move_x, int move_y)
{
    int left_right;

    left_right = abs(move_x) > abs(move_y);
    if (DiaVars->Layout == DIA_LAYOUT_SINGLE) {
	if (DiaVars->SlideShow) {	///< stop slide show
	    DiaVars->SlideShow = 0;
	}
	//
	//	Single image
	//
	if (DiaVars->FilmStrip && x < DiaVars->FilmStripWidth) {
	    if (left_right) {
		DiaVars->FilmStripWidth += move_x / DIA_MOVE_DIVIDER;
		if (DiaVars->FilmStripWidth < 32) {
		    DiaVars->FilmStripWidth = 32;
		} else if (DiaVars->FilmStripWidth > RootWidth / 2) {
		    DiaVars->FilmStripWidth = RootWidth / 2;
		}
		DiaVars->FilmStripHeight =
		    (DiaVars->FilmStripWidth * DiaVars->AspectDen) /
		    DiaVars->AspectNum;
		DiaDraw(0);
		return;
	    }
	    if (move_y > 0) {
		DiaScrollUp();
	    } else {
		DiaScrollDown();
	    }
	    return;
	}
	if (left_right) {
	    if (move_x > 0) {
		DiaPrev();
	    } else {
		DiaNext();
	    }
	    return;
	}
	if (move_y > 0) {
	    DiaVars->SlideShow = 1;
	} else {
	    DiaVars->Layout = DIA_LAYOUT_INDEX;
	    DiaDraw(0);
	}
	return;
    }
    //
    //	index-page commands
    //
    if (left_right) {
	DiaVars->IndexWidth += move_x / DIA_MOVE_DIVIDER;
	if (DiaVars->IndexWidth < 32) {
	    DiaVars->IndexWidth = 32;
	} else if (DiaVars->IndexWidth > RootWidth / 2) {
	    DiaVars->IndexWidth = RootWidth / 2;
	}
	DiaVars->IndexHeight =
	    (DiaVars->IndexWidth * DiaVars->AspectDen) / DiaVars->AspectNum;
	DiaDraw(0);
	return;
    }
    if (move_y > 0) {
	DiaIndexScrollUp(DiaVars->Width / DiaVars->IndexWidth);
    } else {
	DiaIndexScrollDown(DiaVars->Width / DiaVars->IndexWidth);
    }
    return;
}

/**
**	Handle an expose event on a dia window.
**
**	@param event	X11 expose event
**
**	@returns true if the event was for the dia module, false otherwise.
*/
int DiaHandleExpose(const xcb_expose_event_t * event)
{
    if ( /* DiaVars && */ event->window == DiaVars->Window) {
	DiaDraw(1);
	return 1;
    }
    return 0;
}

/**
**	Handle a mouse button press event.
**
**	@param event	X11 button press event
**
**	@returns true if the event was for the dia module, false otherwise.
*/
int DiaHandleButtonPress(const xcb_button_press_event_t * event)
{
    if ( /* DiaVars && */ event->event == DiaVars->Window) {
	//
	//	first pointer button press reaction is delayed for
	//	gestures
	//
	if (event->detail == XCB_BUTTON_INDEX_1) {
	    DiaVars->LastTick = event->time;
	    DiaVars->LastX = event->root_x;
	    DiaVars->LastY = event->root_y;
	    DiaVars->State = 1;
	} else {
	    DiaVars->State = 0;
	    DiaClickCommand(event->detail, event->event_x, event->event_y);
	}
	return 1;
    }
    return 0;
}

/**
**	Handle a mouse button release event.
**
**	@param event	X11 button release event
**
**	@returns true if the event was for the dia module, false otherwise.
*/
int DiaHandleButtonRelease(const xcb_button_release_event_t * event)
{
    if ( /* !DiaVars || */ event->event != DiaVars->Window) {
	return 0;
    }
    //
    //	Check delayed button press
    //
    if (DiaVars->State == 1) {
	int move_x;
	int move_y;

	move_x = event->root_x - DiaVars->LastX;
	move_y = event->root_y - DiaVars->LastY;
	// moved below threshold
	if (abs(move_x) < DIA_MOVE_DIVIDER && abs(move_y) < DIA_MOVE_DIVIDER) {
	    DiaClickCommand(event->detail, event->event_x, event->event_y);
	} else {
	    DiaMoveCommand(event->event_x, event->event_y, move_x, move_y);
	}
    }
    return 1;
}

/**
**	Timeout for dia-show.  Max resolution is * 50ms.
**
**	@param tick	current tick in ms
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
*/
void DiaTimeout(uint32_t __attribute__ ((unused)) tick, int x, int y)
{
    if (!DiaVars->Window) {
	return;
    }
    if (DiaVars->SlideShow) {		// slide show running
	if (tick > DiaVars->LastTick) {
	    size_t index;
	    size_t *value;

	    DiaVars->LastTick += DiaVars->SlideShowDelay * 1000;

	    index = DiaVars->CurrentIndex;
	    value = ArrayNext(DiaVars->FilesInDir, &index);
	    if (value) {
		DiaNext();
	    } else {
		DiaFirst();
	    }
	}
    }
    if (DiaVars->NeedRedraw) {		// missing update
	DiaDraw(0);
    }
    x = x;
    y = y;
}

/**
**	Parse dia show configuration.
**
**	@param config	global config dictionary
*/
void DiaConfig(const Config * config)
{
    ssize_t ival;

    //
    //	default values
    //
    DiaVars->CornerWidth = DIA_CORNER_WIDTH;
    DiaVars->CornerHeight = DIA_CORNER_HEIGHT;

    DiaVars->SlideShowDelay = 30;

    DiaVars->FilmStripWidth = DIA_FILM_STRIP_WIDTH;
    DiaVars->FilmStripHeight = DIA_FILM_STRIP_HEIGHT;

    DiaVars->IndexWidth = DIA_INDEX_WIDTH;
    DiaVars->IndexHeight = DIA_INDEX_HEIGHT;

    DiaVars->AspectNum = 4;
    DiaVars->AspectDen = 3;

    DiaVars->SlideShow = 0;
    DiaVars->FilmStrip = 1;
    DiaVars->IndexLabel = 0;
    DiaVars->Layout = DIA_LAYOUT_SINGLE;

    // FIXME: get dia table first
    // FIXME: should write a config -> c
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "label", NULL)) {
	DiaVars->IndexLabel = ival != 0;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "film-strip", NULL)) {
	DiaVars->FilmStrip = ival != 0;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "slide-show", NULL)) {
	DiaVars->SlideShow = ival != 0;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "back-drop", NULL)) {
	DiaVars->Backdrop = ival != 0;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "delay", NULL)) {
	DiaVars->SlideShowDelay = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "aspect-num", NULL)) {
	DiaVars->AspectNum = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "aspect-den", NULL)) {
	DiaVars->AspectDen = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "film-strip-width",
	    NULL)) {
	DiaVars->FilmStripWidth = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "film-strip-height",
	    NULL)) {
	DiaVars->FilmStripHeight = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "index-page-width",
	    NULL)) {
	DiaVars->IndexWidth = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "dia", "index-page-height",
	    NULL)) {
	DiaVars->IndexHeight = ival;
    }
}

#endif // } USE_DIA

/// @}
