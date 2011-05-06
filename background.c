///
///	@file background.c	@brief desktop background control.
///
///	Copyright (c) 2009 - 2011 by Lutz Sammer.  All Rights Reserved.
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
///	@defgroup background The background module.
///
///	This module contains the desktop background control functions.
///
///	This module is only available, if compiled with #USE_BACKGROUND.
///	Image as backgrounds are only available, if compiled with #USE_ICON.
///
///	Alternative xsetroot can be used to set the background.
///
///	If backgrounds are (runtime) configured, other application can
///	set the background, but they are lost, when the user switches the
///	desktop.
///
///	@todo all background are pre-loaded consuming many many memory!
///	(about 4MB pro background), generate them on fly,
///
///	@todo support tiled and stretched or centered images
///
///	@note Is XSETROOT_ID still needed to be supported?
///	If you need it, compile with #USE_XSETROOT_ID.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_BACKGROUND			// {

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "command.h"
#include "client.h"
#include "hints.h"

#include "draw.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"
#include "background.h"

//////////////////////////////////////////////////////////////////////////////

/**
**	Enumeration of desktop background types.
*/
typedef enum
{
    BACKGROUND_SOLID,			///< solid color background
    BACKGROUND_GRADIENT,		///< gradient background
    BACKGROUND_COMMAND,			///< command to run for setting
    BACKGROUND_IMAGE,			///< image placed (top-left)
    BACKGROUND_SCALE,			///< scaled image (fit)
    BACKGROUND_ZOOM,			///< zoomed image
} BackgroundType;

/**
**	Background typedef.
*/
typedef struct _background_ Background;

/**
**	Structure to represent background for one or more desktops.
*/
struct _background_
{
    int16_t Desktop;			///< desktop

    BackgroundType Type:8;		///< type of background
    char *Value;			///< value for background type

    xcb_pixmap_t Pixmap;		///< background pixmap
} __attribute__ ((packed));

    /// table of backgrounds.
static Background *Backgrounds;

    /// number of backgrounds in table.
static int BackgroundN;

static Background *BackgroundDefault;	///< default background
static Background *BackgroundLast;	///< last background loaded

static xcb_get_property_cookie_t Cookie;	///< _XROOTPMAP_ID request cookie

// ---------------------------------------------------------------------------

/**
**	Load background for specified desktop.
**
**	@param desktop	desktop for background update
*/
void BackgroundLoad(int desktop)
{
    Background *background;

    // determine background to load
    for (background = Backgrounds; background < Backgrounds + BackgroundN;
	++background) {
	if (background->Desktop == desktop) {
	    goto found;
	}
    }
    background = BackgroundDefault;

  found:
    // if there is no background specified for this desktop, just return
    if (!background || !background->Value) {
	return;
    }
    // if background isn't changing, don't do anything
    if (BackgroundLast && background->Type == BackgroundLast->Type
	&& !strcmp(background->Value, BackgroundLast->Value)) {
	return;
    }
    BackgroundLast = background;

    // load background based on type
    switch (background->Type) {
	case BACKGROUND_COMMAND:
	    CommandRun(background->Value);
	    return;
	default:
	    break;
    }

    // set pixmap, clear window, update property
    xcb_change_window_attributes(Connection, XcbScreen->root,
	XCB_CW_BACK_PIXMAP, &background->Pixmap);
    xcb_aux_clear_window(Connection, XcbScreen->root);

    AtomSetPixmap(XcbScreen->root, &Atoms.XROOTPMAP_ID, background->Pixmap);
}

// ------------------------------------------------------------------------ //

/**
**	Load solid background.
**
**	@param background	background configuration
*/
static void BackgroundLoadSolid(Background * background)
{
    xcb_coloritem_t c;
    xcb_point_t point;

    ColorParse(background->Value, &c);

    // create 1x1 pixmap
    background->Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, XcbScreen->root_depth, background->Pixmap,
	XcbScreen->root, 1, 1);

    // draw point on it
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, &c.pixel);
    point.x = 0;
    point.y = 0;
    xcb_poly_point(Connection, XCB_COORD_MODE_ORIGIN, background->Pixmap,
	RootGC, 1, &point);
}

/**
**	Load gradient background.
**
**	@param background	background configuration
*/
static void BackgroundLoadGradient(Background * background)
{
    xcb_coloritem_t color1;
    xcb_coloritem_t color2;
    char *temp;
    char *sep;
    int len;

    sep = strchr(background->Value, '|');
    if (!sep) {
	background->Pixmap = XCB_NONE;
	Warning("background color bad syntax: \"%s\"", background->Value);
	return;
    }
    // get first color
    len = sep - background->Value;
    temp = alloca(len + 1);
    memcpy(temp, background->Value, len);
    temp[len] = 0;
    ColorParse(temp, &color1);

    // get second color
    ColorParse(sep + 1, &color2);

    // create fullscreen pixmap
    background->Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, XcbScreen->root_depth, background->Pixmap,
	XcbScreen->root, XcbScreen->width_in_pixels,
	XcbScreen->height_in_pixels);

    if (color1.pixel == color2.pixel) {
	xcb_rectangle_t rectangle;

	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, &color1.pixel);
	rectangle.x = 0;
	rectangle.y = 0;
	rectangle.width = XcbScreen->width_in_pixels;
	rectangle.height = XcbScreen->height_in_pixels;
	xcb_poly_fill_rectangle(Connection, background->Pixmap, RootGC, 1,
	    &rectangle);
    } else {
	GradientDrawHorizontal(background->Pixmap, RootGC, color1.pixel,
	    color2.pixel, 0, 0, XcbScreen->width_in_pixels,
	    XcbScreen->height_in_pixels);
    }
}

#ifdef USE_ICON

/**
**	Load an image background.
**
**	@param background	background parameters
*/
static void BackgroundLoadImage(Background * background)
{
    Icon *icon;
    unsigned width;
    unsigned height;
    int i;
    xcb_rectangle_t rectangle;
    char *name;

    // load icon
    name = ExpandPath(background->Value);
    icon = IconLoadNamed(name);
    free(name);

    if (!icon) {
	background->Pixmap = XCB_NONE;
	Warning("background image not found: \"%s\"", background->Value);
	return;
    }
    // create image pixmap
    background->Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, XcbScreen->root_depth, background->Pixmap,
	XcbScreen->root, XcbScreen->width_in_pixels,
	XcbScreen->height_in_pixels);

    // clear pixmap in case it is too small
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	&XcbScreen->black_pixel);
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = XcbScreen->width_in_pixels;
    rectangle.height = XcbScreen->height_in_pixels;
    xcb_poly_fill_rectangle(Connection, background->Pixmap, RootGC, 1,
	&rectangle);

    // determine size of background pixmap
    width = XcbScreen->width_in_pixels;
    height = XcbScreen->height_in_pixels;
    switch (background->Type) {
	case BACKGROUND_IMAGE:
	    width = icon->Image->Width;
	    height = icon->Image->Height;
	    break;
	case BACKGROUND_SCALE:
	default:
	    break;
	case BACKGROUND_ZOOM:
	    // keep icon aspect ratio
	    i = (icon->Image->Width * 65536) / icon->Image->Height;
	    width = MAX(width * 65536, height * i);
	    height = MAX(height, width / i);
	    width = (height * i) / 65536;
	    break;
    }

    // draw icon on background pixmap
    IconDraw(icon, background->Pixmap, 0, 0, width, height);

    // we don't need icon anymore
    IconDel(icon);
}
#endif // USE_ICON

/**
**	Prepare initialize background support.
*/
void BackgroundPreInit(void)
{
    Cookie =
	xcb_get_property_unchecked(Connection, 0, XcbScreen->root,
	Atoms.XROOTPMAP_ID.Atom, XCB_ATOM_PIXMAP, 0, UINT32_MAX);
}

/**
**	Initialize background support.
*/
void BackgroundInit(void)
{
#ifdef USE_XSETROOT_ID
    xcb_intern_atom_cookie_t ia_cookie;
    xcb_intern_atom_reply_t *ia_reply;
#endif
    xcb_get_property_reply_t *gp_reply;
    xcb_pixmap_t pixmap;
    Background *background;

    //
    //	free memory for pixmaps already used as backgrounds.
    //
    pixmap = XCB_NONE;

#ifdef USE_XSETROOT_ID
    // don't intern these old properties
    ia_cookie =
	xcb_intern_atom_unchecked(Connection, 1, sizeof("XSETROOT_ID") - 1,
	"XSETROOT_ID");
    if ((ia_reply = xcb_intern_atom_reply(Connection, ia_cookie, NULL))) {
	if (ia_reply->atom) {
	    xcb_get_property_cookie_t gp_cookie;

	    Debug(3, "found XSETROOT_ID atom %d\n", ia_reply->atom);
	    gp_cookie =
		xcb_get_property_unchecked(Connection, 1, XcbScreen->root,
		ia_reply->atom, XCB_ATOM_PIXMAP, 0, UINT32_MAX);

	    gp_reply = xcb_get_property_reply(Connection, gp_cookie, NULL);
	    if (gp_reply) {
		const void *data;

		Debug(3, "found XSETROOT_ID atom %d\n", ia_reply->atom);
		if (gp_reply->value_len
		    && (data = xcb_get_property_value(gp_reply))
		    && (pixmap = *(const xcb_pixmap_t *)data)) {
		    xcb_kill_client(Connection, pixmap);
		}
		free(gp_reply);
	    }
	    //xcb_delete_property(Connection, XcbScreen->root, ia_reply->atom);
	}
	free(ia_reply);
    }
#endif

    gp_reply = xcb_get_property_reply(Connection, Cookie, NULL);
    if (gp_reply) {
	const void *data;

	Debug(3, "found _XROOTPMAP_ID atom\n");
	if (gp_reply->value_len && (data = xcb_get_property_value(gp_reply))
	    && (pixmap = *(const xcb_pixmap_t *)data)) {
	    Debug(3, "_XROOTPMAP_ID pixmap %#010x\n", pixmap);
	    xcb_kill_client(Connection, pixmap);
	}
	free(gp_reply);
    }
    // load background data
    for (background = Backgrounds; background < Backgrounds + BackgroundN;
	++background) {
	switch (background->Type) {
	    case BACKGROUND_SOLID:
		BackgroundLoadSolid(background);
		break;
	    case BACKGROUND_GRADIENT:
		BackgroundLoadGradient(background);
		break;
	    case BACKGROUND_COMMAND:
		// nothing to do
		break;
	    case BACKGROUND_IMAGE:
	    case BACKGROUND_SCALE:
	    case BACKGROUND_ZOOM:
#ifdef USE_ICON
		BackgroundLoadImage(background);
		break;
#endif
	    default:
		Debug(2, "invalid background type %d\n", background->Type);
		break;
	}

	if (background->Desktop == -1) {
	    BackgroundDefault = background;
	}
    }
}

/**
**	Cleanup background support.
*/
void BackgroundExit(void)
{
    Background *background;

    for (background = Backgrounds; background < Backgrounds + BackgroundN;
	++background) {

	if (background->Pixmap) {
	    xcb_free_pixmap(Connection, background->Pixmap);
	}
	free(background->Value);
    }

    free(Backgrounds);
    Backgrounds = NULL;
    BackgroundN = 0;

    BackgroundLast = NULL;
    BackgroundDefault = NULL;
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_RC				// {

/**
**	Add background.
**
**	@param desktop	desktop number for this background
**	@param type	background type (solid, ....)
**	@param value	argument for background type (color, ....)
*/
static void BackgroundNew(int desktop, BackgroundType type, const char *value)
{
    Background *background;

    // make sure we have value
    if (!value) {
	Warning("no value specified for background\n");
	return;
    }
    Backgrounds = realloc(Backgrounds, ++BackgroundN * sizeof(*Backgrounds));

    // create background
    background = Backgrounds + BackgroundN - 1;
    background->Desktop = desktop;
    background->Type = type;
    background->Value = strdup(value);
    background->Pixmap = XCB_NONE;
}

/**
**	Parse configuration for background module.
**
**	@param config	global config dictionary
*/
void BackgroundConfig(const Config * config)
{
    const ConfigObject *array;

    //
    //	get array of background(s)
    //
    if (ConfigGetArray(ConfigDict(config), &array, "background", NULL)) {
	ssize_t ival;
	const ConfigObject *index;
	const ConfigObject *value;

	//
	//	parse array of backgrounds
	//
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const char *sval;
	    const ConfigObject *table;

	    if (ConfigCheckArray(value, &table)) {
		ConfigCheckInteger(index, &ival);
		// check for valid desktop numbers
		if (ival != -1 && DesktopN) {
		    if (ival < 0 || ival >= DesktopN) {
			Warning("desktop %zd for background not configured\n",
			    ival);
		    }
		}

		if (ConfigGetString(table, &sval, "solid", NULL)) {
		    BackgroundNew(ival, BACKGROUND_SOLID, sval);
		} else if (ConfigGetString(table, &sval, "gradient", NULL)) {
		    BackgroundNew(ival, BACKGROUND_GRADIENT, sval);
		} else if (ConfigGetString(table, &sval, "execute", NULL)) {
		    BackgroundNew(ival, BACKGROUND_COMMAND, sval);
		} else if (ConfigGetString(table, &sval, "image", NULL)) {
		    BackgroundNew(ival, BACKGROUND_IMAGE, sval);
		} else if (ConfigGetString(table, &sval, "scale", NULL)) {
		    BackgroundNew(ival, BACKGROUND_SCALE, sval);
		} else if (ConfigGetString(table, &sval, "zoom", NULL)) {
		    BackgroundNew(ival, BACKGROUND_ZOOM, sval);
		}
	    } else {
		Warning("value in background ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

#endif // } USE_RC

#endif // } !USE_BACKGROUND

/// @}
