///
///	@file icon.c		@brief icon handling functions.
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
///	@defgroup icon The icon module.
///
///	This module contains icon functions.
///
///	These functions and all dependencies are only available if compiled
///	widh #USE_ICON.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_ICON				// {

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/queue.h>

#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_image.h>
#ifdef USE_RENDER
#include <xcb/xcb_pixel.h>
#include <xcb/xcb_renderutil.h>
#include <xcb/render.h>
#endif

#include "array.h"
#include "config.h"

#include "draw.h"
#include "client.h"
#include "hints.h"

#include "image.h"
#include "icon.h"

// ------------------------------------------------------------------------ //

/**
**	Scaled icon structure.
*/
struct _scaled_icon_
{
    SLIST_ENTRY(_scaled_icon_) Next;	///< singly-linked list

    uint16_t Width;			///< width of scaled icon
    uint16_t Height;			///< height of scaled icon

    union
    {
	xcb_pixmap_t Pixmap;		///< image pixmap
#ifdef USE_RENDER
	xcb_render_picture_t Picture;	///< render picture of image
#endif
    } Image;				///< the image drawing data
    union
    {
	xcb_pixmap_t Pixmap;		///< mask pixmap
#ifdef USE_RENDER
	xcb_render_picture_t Picture;	///< render picture of mask
#endif
    } Mask;				///< the mask data
};

static xcb_gcontext_t IconGC;		///< icon graphic context

    /// icon head structure
LIST_HEAD(_icon_head_, _icon_);

    /// hash table for icons
static struct _icon_head_ *IconHashTable;

    /// list of icon paths, empty string terminated
static char *IconPath;

#if 0

/**
**	Set preferred icon sizes on root window.
**
**	replaced by _NET_WM_ICON_GEOMETRY
*/
void SetIconSize(void)
{
    XIconSize size;

    if (!IconSize) {
	// FIXME: compute values based on sizes we can actually use
	IconSize = 32;

	size.min_width = IconSize;
	size.min_height = IconSize;
	size.max_width = IconSize;
	size.max_height = IconSize;
	size.width_inc = IconSize;
	size.height_inc = IconSize;

	XSetIconSizes(Connection, RootWindow, &size, 1);

    }
}
#endif

#ifdef USE_RENDER			// {

/**
**	Create a scaled icon with XCB render extension.
**
**	@param icon	icon
**	@param width	width of icon to create
**	@param height	height of icon to create
**
**	@returns scaled icon.
**/
static ScaledIcon *IconCreateRenderScaled(Icon * icon, int width, int height)
{
    ScaledIcon *scaled;
    double scale_y;
    double scale_x;
    double src_x;
    double src_y;
    xcb_gcontext_t gc;
    xcb_image_t *image_data;
    xcb_image_t *image_mask;
    xcb_pixmap_t pixmap_data;
    xcb_pixmap_t pixmap_mask;
    uint8_t *argb;
    int y;
    xcb_render_pictvisual_t *pictvisual;
    xcb_render_pictforminfo_t template;
    xcb_render_pictforminfo_t *pictforminfo;
    xcb_render_pictformat_t pictformat;

    // FIXME: disable HaveRender with RootDepth != 24
    if (!icon->UseRender || !HaveRender || RootDepth < 24) {
	icon->UseRender = 0;
	return NULL;
    }
    scaled = malloc(sizeof(*scaled));
    SLIST_INSERT_HEAD(&icon->Scaled, scaled, Next);
    scaled->Width = width;
    scaled->Height = height;

    // create alpha data pixmap
    pixmap_mask = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, 8, pixmap_mask, RootWindow, width, height);
    gc = xcb_generate_id(Connection);
    xcb_create_gc(Connection, gc, pixmap_mask, 0, NULL);

    // create color data pixmap
    pixmap_data = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, RootDepth, pixmap_data, RootWindow, width,
	height);

    // create a temporary xcb_image for scaling
    image_data =
	xcb_image_create_native(Connection, width, height,
	XCB_IMAGE_FORMAT_Z_PIXMAP, RootDepth, NULL, 0L, NULL);
    image_mask =
	xcb_image_create_native(Connection, width, height,
	XCB_IMAGE_FORMAT_Z_PIXMAP, 8, NULL, 0L, NULL);

    scale_x = (double)icon->Image->Width / width;
    scale_y = (double)icon->Image->Height / height;
    argb = icon->Image->Data;

    src_y = 0.0;
    for (y = 0; y < height; y++) {
	int x;
	int n;

	src_x = 0.0;
	n = (int)src_y *icon->Image->Width;

	for (x = 0; x < width; x++) {
	    xcb_coloritem_t color;
	    int i;

	    i = 4 * (n + (int)src_x);

	    // FIXME: image_mask has no effect
	    xcb_image_put_pixel_Z8(image_mask, x, y, 255 /*argb[i + 0] */ );

	    color.red = (65535 * (argb[i + 1]) / 255);
	    color.green = (65535 * (argb[i + 2]) / 255);
	    color.blue = (65535 * (argb[i + 3]) / 255);
	    ColorGetPixel(&color);

	    // FIXME: can use fast pixel here?
	    xcb_image_put_pixel(image_data, x, y, color.pixel);

	    src_x += scale_x;
	}

	src_y += scale_y;
    }

    // render image data to color data pixmap
    xcb_image_put(Connection, pixmap_data, RootGC, image_data, 0, 0, 0);
    xcb_image_destroy(image_data);
    // render alpha data to mask pixmap
    xcb_image_put(Connection, pixmap_mask, gc, image_mask, 0, 0, 0);
    xcb_image_destroy(image_mask);
    xcb_free_gc(Connection, gc);

    // create render picture
    template.type = XCB_RENDER_PICT_TYPE_DIRECT;
    template.depth = 8;
    template.direct.alpha_mask = 0xFF;
    pictforminfo =
	xcb_render_util_find_format(xcb_render_util_query_formats(Connection),
	XCB_PICT_FORMAT_TYPE | XCB_PICT_FORMAT_DEPTH |
	XCB_PICT_FORMAT_ALPHA_MASK, &template, 0);
    pictformat = pictforminfo->id;
    // FIXME: must free pictforminfo?
    scaled->Mask.Picture = xcb_generate_id(Connection);
    xcb_render_create_picture(Connection, scaled->Mask.Picture, pixmap_mask,
	pictformat, 0, NULL);

    pictvisual =
	xcb_render_util_find_visual_format(xcb_render_util_query_formats
	(Connection), XcbScreen->root_visual);
    pictformat = pictvisual->format;
    // FIXME: must free pictvisual?
    scaled->Image.Picture = xcb_generate_id(Connection);
    xcb_render_create_picture(Connection, scaled->Image.Picture, pixmap_data,
	pictformat, XCB_RENDER_CP_ALPHA_MAP, &scaled->Mask.Picture);

    // free temporary pixmaps
    xcb_free_pixmap(Connection, pixmap_mask);
    xcb_free_pixmap(Connection, pixmap_data);

    return scaled;
}

/**
**	Draw a scaled icon with XCB render extension
**
**	@param icon	icon
**	@param scaled	scaled icon data
**	@param drawable	drawable on which to render icon
**	@param x	x-coordinate to place icon
**	@param y	y-coordinate to place icon
**
**	@returns true if icon was successfully rendered, false otherwise.
*/
static int IconDrawRenderScaled(const Icon * icon, const ScaledIcon * scaled,
    xcb_drawable_t drawable, int x, int y)
{
    xcb_render_picture_t src;

    if (!icon->UseRender || !HaveRender || RootDepth < 24) {
	return 0;
    }

    if ((src = scaled->Image.Picture)) {
	xcb_render_pictvisual_t *pictvisual;
	xcb_render_pictformat_t pictformat;
	xcb_render_picture_t dst;
	unsigned width;
	unsigned height;

	// FIXME: cache this, no need to lookup each draw
	pictvisual =
	    xcb_render_util_find_visual_format(xcb_render_util_query_formats
	    (Connection), XcbScreen->root_visual);
	pictformat = pictvisual->format;
	// FIXME: must free pictvisual ?

	dst = xcb_generate_id(Connection);
	xcb_render_create_picture(Connection, dst, drawable, pictformat, 0,
	    NULL);

	width = scaled->Width;
	height = scaled->Height;

	xcb_render_composite(Connection, XCB_RENDER_PICT_OP_OVER, src,
	    XCB_NONE, dst, 0, 0, 0, 0, x, y, width, height);

	xcb_render_free_picture(Connection, dst);
    }

    return 1;
}

#else // }{ USE_RENDER

    /// Dummy for create a scaled icon with XCB render extension.
#define IconCreateRenderScaled(icon, width, height)	NULL

    /// Dummy for draw a scaled icon with XCB render extension
#define IconDrawRenderScaled(icon, scaled, drawable, x, y)	0

#endif // } !USE_RENDER

/**
**	Get a scaled icon.
**
**	@param icon	base icon, to be scaled
**	@param width	requested icon width
**	@param height	requested icon height
**
**	@returns scaled icon, ready to draw.
*/
static ScaledIcon *IconGetScaled(Icon * icon, int width, int height)
{
    ScaledIcon *scaled;
    xcb_image_t *xcb_image;
    int x;
    int y;
    int i;
    int n;
    double scale_x;
    double scale_y;
    double src_x;
    double src_y;
    uint8_t *argb;
    uint8_t *mask;
    int mask_width;

    // calculated scaled icon size
    if (!width) {
	width = icon->Image->Width;
    }
    if (!height) {
	height = icon->Image->Height;
    }
    Debug(4, "icon %dx%d -> %dx%d\n", icon->Image->Width, icon->Image->Height,
	width, height);
    // keep icon aspect ratio
    i = (icon->Image->Width * 65536) / icon->Image->Height;
    width = MIN(width * 65536, height * i);
    height = MIN(height, width / i);
    width = (height * i) / 65536;
    if (width < 1) {
	width = 1;
    }
    if (height < 1) {
	height = 1;
    }
    // check if this size already exists
    SLIST_FOREACH(scaled, &icon->Scaled, Next) {
	if (scaled->Width == width && scaled->Height == height) {
	    return scaled;
	}
    }

    // see if we can use XCB render to create icon
    if ((scaled = IconCreateRenderScaled(icon, width, height))) {
	return scaled;
    }

    Debug(3, "new scaled icon from %dx%d\n", width, height);
    // create a new ScaledIcon old-fashioned way
    scaled = malloc(sizeof(*scaled));
    SLIST_INSERT_HEAD(&icon->Scaled, scaled, Next);
    scaled->Width = width;
    scaled->Height = height;

    // determine, if we need a mask, alpha > 128!
    mask = NULL;
    mask_width = (width + 7) / 8;	// moved out of if, to make gcc happy
    n = 4 * icon->Image->Height * icon->Image->Width;
    for (i = 0; i < n; i += 4) {
	if (icon->Image->Data[i] < 128) {
	    //
	    //	allocate empty mask (if mask is needed)
	    //
	    i = mask_width * height;
	    if ((mask = malloc(i))) {	// malloc success
		memset(mask, 255, i);
	    }
	    break;
	}
    }

    // create a temporary xcb_image for scaling
    xcb_image =
	xcb_image_create_native(Connection, width, height,
	(RootDepth ==
	    1) ? XCB_IMAGE_FORMAT_XY_BITMAP : XCB_IMAGE_FORMAT_Z_PIXMAP,
	RootDepth, NULL, 0L, NULL);

    // determine scale factor
    // FIXME: remove doubles
    scale_x = (double)icon->Image->Width / width;
    scale_y = (double)icon->Image->Height / height;

    argb = icon->Image->Data;
    src_y = 0.0;
    for (y = 0; y < height; y++) {
	src_x = 0.0;
	n = (int)src_y *icon->Image->Width;

	for (x = 0; x < width; x++) {
	    xcb_coloritem_t color;

	    i = 4 * (n + (int)src_x);

	    if (argb[i] < 128) {
		mask[(y * mask_width) + (x >> 3)] &= (~(1 << (x & 7)));
	    }

	    color.red = (65535 * (argb[i + 1]) / 255);
	    color.green = (65535 * (argb[i + 2]) / 255);
	    color.blue = (65535 * (argb[i + 3]) / 255);
	    ColorGetPixel(&color);

	    xcb_image_put_pixel(xcb_image, x, y, color.pixel);

	    src_x += scale_x;
	}

	src_y += scale_y;
    }

    // create color data pixmap
    scaled->Image.Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, RootDepth, scaled->Image.Pixmap, RootWindow,
	width, height);
    // render xcb_image to color data pixmap
    xcb_image_put(Connection, scaled->Image.Pixmap, RootGC, xcb_image, 0, 0,
	0);
    // release xcb_image
    xcb_image_destroy(xcb_image);

    scaled->Mask.Pixmap = XCB_NONE;
    if (mask) {
	scaled->Mask.Pixmap =
	    xcb_create_pixmap_from_bitmap_data(Connection, RootWindow, mask,
	    width, height, 1, 0, 0, NULL);
	free(mask);
    }

    return scaled;
}

/**
**	Draw an icon.
**
**	This will scale an icon if necessary to fit requested size.
**	Aspect ratio of icon is preserved.
**
**	@param icon	icon to render
**	@param drawable drawable on which to place icon
**	@param x	x-offset on drawable to render icon
**	@param y	y-offset on drawable to render icon
**	@param width	width of icon to display
**	@param height	height of icon to display
*/
void IconDraw(Icon * icon, xcb_drawable_t drawable, int x, int y,
    unsigned width, unsigned height)
{
    ScaledIcon *scaled;

    // Debug(3, "draw icon %d,%d %dx%d\n", x, y, width, height);
    // scale icon
    if ((scaled = IconGetScaled(icon, width, height))) {

	// draw icon centered
	x += width / 2 - scaled->Width / 2;
	y += height / 2 - scaled->Height / 2;

	// if we support xrender, use it
	if (IconDrawRenderScaled(icon, scaled, drawable, x, y)) {
	    return;
	}
	// draw icon old way
	if (scaled->Image.Pixmap) {
	    // set clip mask
	    if (scaled->Mask.Pixmap) {
		uint32_t values[3];

		values[0] = x;
		values[1] = y;
		values[2] = scaled->Mask.Pixmap;
		xcb_change_gc(Connection, IconGC,
		    XCB_GC_CLIP_ORIGIN_X | XCB_GC_CLIP_ORIGIN_Y |
		    XCB_GC_CLIP_MASK, values);
	    }
	    // draw icon
	    xcb_copy_area(Connection, scaled->Image.Pixmap, drawable, IconGC,
		0, 0, x, y, scaled->Width, scaled->Height);

	    // reset clip mask
	    if (scaled->Mask.Pixmap) {
		uint32_t values[1];

		values[0] = XCB_NONE;
		xcb_change_gc(Connection, IconGC, XCB_GC_CLIP_MASK, values);
	    }
	}
    }
}

/**
**	Compute hash value for given string.
**
**	@param str	0 terminated string to hash
*/
static int IconHash(const char *str)
{
    uint32_t hval;

    hval = 0xFFFFFFFFUL;
    while (*str) {
	uint32_t g;

	hval <<= 4;
	hval += (uint8_t) * str++;
	g = hval & ((uint32_t) 0xFUL << (32 - 4));
	hval ^= g >> (32 - 8);
	hval ^= g;
    }
    return (hval ^ 0xFFFFFFFFUL) % ICON_HASH_SIZE;
}

/**
**	Find an icon in icon hash table.
**
**	@param name	file name of icon to find
*/
static Icon *IconLookup(const char *name)
{
    Icon *icon;

    LIST_FOREACH(icon, &IconHashTable[IconHash(name)], Node) {
	if (!strcmp(icon->Name, name)) {
	    return icon;
	}
    }
    return NULL;
}

/**
**	Insert an icon into icon hash table.
**
**	@param icon	icon inserted at head in hashtable
*/
static void IconInsert(Icon * icon)
{
    LIST_INSERT_HEAD(&IconHashTable[IconHash(icon->Name)], icon, Node);
}

/**
**	Remove an icon from icon hash table.
**
**	@param icon	icon deleted from hashtable
*/
static void IconRemove(Icon * icon)
{
    LIST_REMOVE(icon, Node);
}

/**
**	Create and initialize a new icon structure.
**
**	@returns new icon structure.
*/
static Icon *IconNew(void)
{
    Icon *icon;

    icon = calloc(1, sizeof(*icon));
    icon->UseRender = 1;

    return icon;
}

/**
**	Delete icon data and all scaled icons.
**
**	@param icon	icon to delete
*/
static void IconDelete(Icon * icon)
{
    ScaledIcon *scaled;
    ScaledIcon *temp;

    scaled = SLIST_FIRST(&icon->Scaled);
    while (scaled) {			// fast tail deletion
	temp = SLIST_NEXT(scaled, Next);

#ifdef USE_RENDER
	if (icon->UseRender) {
	    if (scaled->Image.Picture) {
		xcb_render_free_picture(Connection, scaled->Image.Picture);
	    }
	    if (scaled->Mask.Picture) {
		xcb_render_free_picture(Connection, scaled->Mask.Picture);
	    }
	} else
#endif
	{
	    if (scaled->Image.Pixmap) {
		xcb_free_pixmap(Connection, scaled->Image.Pixmap);
	    }
	    if (scaled->Mask.Pixmap) {
		xcb_free_pixmap(Connection, scaled->Mask.Pixmap);
	    }
	}

	free(scaled);
	scaled = temp;
    }

    free(icon->Name);
    ImageDel(icon->Image);
    free(icon);
}

/**
**	Destroy an icon.
**
**	@param icon	icon to destroy
*/
void IconDel(Icon * icon)
{
    if (!icon) {
	return;
    }
    if (icon->Name) {
	if (icon->RefCnt--) {
	    Debug(3, "%s: reference counter %d\n", __FUNCTION__, icon->RefCnt);
	    return;
	}
	IconRemove(icon);
    }
    IconDelete(icon);
}

/**
**	Create an icon from specified file.
**
**	@param name	file name of icon to load
**
**	@returns icon structure, NULL if not found.
*/
static Icon *IconNewFromFile(const char *name)
{
    Image *image;
    Icon *icon;

    // check if this icon has already been loaded
    if ((icon = IconLookup(name))) {
	++icon->RefCnt;
	return icon;
    }

    icon = NULL;
    if ((image = ImageLoadFile(name))) {
	icon = IconNew();
	icon->Name = strdup(name);
	icon->Image = image;

	IconInsert(icon);
    }
    return icon;
}

#ifdef USE_XPM

/**
**	Create an icon from XPM image data,
**
**	@param name	name of icon to load
**	@param data	XPM image data
**
**	@returns icon structure, NULL if not found.
*/
static Icon *IconNewFromData(const char *name, const char *const *data)
{
    Image *image;
    Icon *icon;

    // check if this icon has already been loaded
    if ((icon = IconLookup(name))) {
	++icon->RefCnt;
	return icon;
    }

    icon = NULL;
    if ((image = ImageFromData(data))) {
	icon = IconNew();
	icon->Name = strdup(name);
	icon->Image = image;

	IconInsert(icon);
    }
    return icon;
}

#include "u.xpm"

/**
**	Create default icon.
*/
static Icon *IconGetDefault(void)
{
    return IconNewFromData("default", (const char *const *)u_xpm);
}

#else

    /// Dummy for create default icon.
#define IconGetDefault() NULL

#endif

/**
**	Load an icon given a path, name, and suffix.
**
**	@param path	path to file name
**	@param name	file name of icon to load
**	@param suffix	suffix of file name
*/
Icon *IconLoadSuffixed(const char *path, const char *name, const char *suffix)
{
    char *buf;

    buf = alloca(strlen(path) + strlen(name) + strlen(suffix) + 2);
    stpcpy(stpcpy(stpcpy(stpcpy(buf, path), "/"), name), suffix);

    Debug(4, "try '%s'\n", buf);

    return IconNewFromFile(buf);
}

/**
**	Load an icon from a file.
**
**	@param name	file name of icon to load
**
**	@returns icon structure, NULL if not found.
*/
Icon *IconLoadNamed(const char *name)
{
    Icon *icon;
    const char *p;

    if (!name) {
	return NULL;
    }
    // absolute or relative path
    if (name[0] == '/' || name[0] == '.') {
	// not quite correct, but for me enough (./ ../)
	return IconNewFromFile(name);
    }

    if ((p = IconPath)) {
	while (*p) {			// end of list
	    if ((icon = IconLoadSuffixed(p, name, ""))) {
		return icon;
	    }
	    p = strchr(p, '\0') + 1;
	}
    }
    return NULL;
}

/**
**	Create an icon from binary data (as specified via window properties).
**
**	@param input	width, height, argb-data * width * height
**	@param length	length of input data
*/
static Icon *IconNewFromEWMH(const uint32_t * input, unsigned length)
{
    unsigned height;
    unsigned width;
    Icon *icon;

    if (!input || length < 2) {
	return NULL;
    }

    width = input[0];
    height = input[1];

    if (width * height + 2 > length) {
	Debug(0, "invalid image size: %dx%d + 2 >%d\n", width, height, length);
	return NULL;
    } else if (!width || !height) {
	Debug(0, "invalid image size: %dx%d\n", width, height);
	return NULL;
    }

    icon = IconNew();
    icon->Image = ImageFromEWMH(input);

    // don't insert this icon since it is transient
    return icon;
}

/**
**	Read icon property from a client.
**
**	_NET_WM_ICON
**
**	_NET_WM_ICON CARDINAL[][2+n]/32
**
**	This is an array of possible icons for client. This specification
**	does not stipulate what size icons should be, but individual
**	desktop environments or toolkits may do so. Window Manager MAY
**	scale any of these icons to an appropriate size.
**
**	This is an array of 32bit packed CARDINAL ARGB with high byte being A,
**	low byte being B. First two cardinals are width, height. Data is
**	in rows, left to right and top to bottom.
**
**	@param client	read icon from our client
**
**	@todo move into hints?
**	@todo rewrite to support multiple icon sizes
*/
static void IconReadNetWMIcon(Client * client)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    int count;
    uint32_t *data;

    cookie = AtomCardinalRequest(client->Window, &Atoms.NET_WM_ICON);

    reply = xcb_get_property_reply(Connection, cookie, NULL);
    if (reply) {
	count = xcb_get_property_value_length(reply) / sizeof(uint32_t);
	data = xcb_get_property_value(reply);
	// validate icon data
	if (reply->type == CARDINAL && reply->format == 32 && count > 2
	    && data) {
	    client->Icon = IconNewFromEWMH(data, count);
	} else {
	    // this application has wrong _NET_WM_ICON
	    //Warning("_NET_WM_ICON of client %#x unsupported\n", client->Window);
	    //Debug(0, "%d\n", reply->value_len);
	}
	// FIXME: if I understand specs correct, there could be multiple
	// sized icons, choose best size.
	free(reply);
    }
}

/**
**	Load an icon for a client.
**
**	@param client client
*/
void IconLoadClient(Client * client)
{
    Debug(0, "FIXME: %s(%p)\n", __FUNCTION__, client);
    Debug(0, "FIXME: SetIconSize(); \n");

    // if client already has an icon, destroy it first
    IconDel(client->Icon);
    client->Icon = NULL;

    // attempt to read _NET_WM_ICON for an icon
    IconReadNetWMIcon(client);
    if (client->Icon) {
	return;
    }
    // attempt to find an icon for this program in icon directory
    if (client->InstanceName) {
	const char *path;

	if ((path = IconPath)) {
	    while (*path) {
#ifdef USE_JPEG
		if ((client->Icon =
			IconLoadSuffixed(path, client->InstanceName,
			    ".jpg"))) {
		    return;
		}
#endif
#ifdef USE_PNG
		if ((client->Icon =
			IconLoadSuffixed(path, client->InstanceName,
			    ".png"))) {
		    return;
		}
#endif
#ifdef USE_XPM
		if ((client->Icon =
			IconLoadSuffixed(path, client->InstanceName,
			    ".xpm"))) {
		    return;
		}
#endif
		path = strchr(path, '\0') + 1;
	    }

	}
    }
    // load default icon
    client->Icon = IconGetDefault();
}

// ---------------------------------------------------------------------------

/**
**	Initialize icon module.
*/
void IconInit(void)
{
    int i;
    uint32_t value[1];

    IconHashTable = calloc(ICON_HASH_SIZE, sizeof(*IconHashTable));
    for (i = 0; i < ICON_HASH_SIZE; ++i) {
	LIST_INIT(&IconHashTable[i]);
    }

    // FIXME: can't use rootGC?
    IconGC = xcb_generate_id(Connection);
    value[0] = 0;
    xcb_create_gc(Connection, IconGC, RootWindow, XCB_GC_GRAPHICS_EXPOSURES,
	value);
}

/**
**	Cleanup icon module.
*/
void IconExit(void)
{
    int i;

    for (i = 0; i < ICON_HASH_SIZE; i++) {
	Icon *icon;
	Icon *temp;

	icon = LIST_FIRST(&IconHashTable[i]);
	while (icon) {
	    temp = LIST_NEXT(icon, Node);
	    IconDelete(icon);
	    icon = temp;
	}
    }
    free(IconHashTable);
    IconHashTable = NULL;

    free(IconPath);
    IconPath = NULL;

    xcb_free_gc(Connection, IconGC);
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Add an icon search path.
**
**	This adds a path to list of icon search paths.
**
**	@param path	icon path to add
*/
void IconAddPath(const char *path)
{
#if 0
    size_t len;
    char *str;

    if (!path || !(len = strlen(path))) {	// empty path
	return;
    }

    if (path[len - 1] == '/') {		// remove trailing '/'
	--len;
    }

    str = malloc(len + 1);
    memcpy(str, path, len);
    str[len] = '\0';

    ExpandPath(&str);
#endif
    if (!path || !*path) {		// empty path
	return;
    }

    ++IconPathN;
    IconPaths = realloc(IconPaths, IconPathN * sizeof(*IconPaths));
    IconPaths[IconPathN - 1] = ExpandPath(path);
}

#else

/**
**	Parse icon config
**
**	@param config	global config dictionary
*/
void IconConfig(const Config * config)
{
    const ConfigObject *array;

    if (ConfigGetArray(ConfigDict(config), &array, "icon-path", NULL)) {
	const ConfigObject *index;
	const ConfigObject *value;
	char *path;
	int len;

	//
	//	join all paths to a string
	//
	len = 0;
	path = NULL;
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const char *s;
	    char *p;
	    int n;

	    if (ConfigCheckString(value, &s) && *s) {
		p = ExpandPath(s);
		n = strlen(p);
		path = realloc(path, len + n + 2);
		strcpy(path + len, p);
		free(p);
		len += n + 1;
	    } else {
		Warning("wrong value in icon-path config ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
	if (path) {
	    path[len] = '\0';
	    IconPath = path;
	}
    }
}

#endif

#endif // } !USE_ICON

/// @}
