///
///	@file background.c	@brief desktop background control.
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
///	@defgroup background The background module.
///
///	This module contains background control functions.
///
///	This module is only available, if compiled with #USE_BACKGROUND.
///	Image as backgrounds are only available, if compiled with #USE_ICON.
///
///	Alternative xsetroot can be used to set the background.
///
///	@todo all background are pre-loaded consuming many many memory!
///	(about 4MB pro background), generate them on fly,
///
///	@todo support tile / zoom / centered images
///
///	@todo remove list use table
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_BACKGROUND			// {

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
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
    BACKGROUND_SCALE,			///< stretched image
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
    SLIST_ENTRY(_background_) Next;	///< singly-linked list

    int Desktop;			///< desktop

    BackgroundType Type;		///< type of background
    char *Value;			///< value for background type

    xcb_pixmap_t Pixmap;		///< background pixmap
};

    /// Background list head typedef
typedef struct _background_head_ BackgroundHead;

    /// Background list head structure
SLIST_HEAD(_background_head_, _background_);

    /// Linked list of backgrounds.
static BackgroundHead Backgrounds = SLIST_HEAD_INITIALIZER(Backgrounds);

static Background *BackgroundDefault;	///< default background
static Background *BackgroundLast;	///< last background loaded

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
    SLIST_FOREACH(background, &Backgrounds, Next) {
	if (background->Desktop == desktop) {
	    break;
	}
    }
    if (!background) {
	background = BackgroundDefault;
    }
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
    xcb_change_window_attributes(Connection, RootWindow, XCB_CW_BACK_PIXMAP,
	&background->Pixmap);
    xcb_aux_clear_window(Connection, RootWindow);

    AtomSetPixmap(RootWindow, &Atoms.XROOTPMAP_ID, background->Pixmap);
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Set background to use for specified desktops.
**
**	@param desktop	desktop number for this background
**	@param type	background type (solid, ....)
**	@param value	argument for background type (color, ....)
**
**	@todo background for desktop can be set twice, wasting memory
*/
void BackgroundSet(int desktop, const char *type, const char *value)
{
    BackgroundType type_id;
    Background *background;

    // make sure we have value
    if (!value) {
	Warning("no value specified for background\n");
	return;
    }
    // parse background type
    if (!type || !strcasecmp(type, "solid")) {
	type_id = BACKGROUND_SOLID;
    } else if (!strcasecmp(type, "gradient")) {
	type_id = BACKGROUND_GRADIENT;
    } else if (!strcasecmp(type, "execute")) {
	type_id = BACKGROUND_COMMAND;
    } else if (!strcasecmp(type, "scale")) {
	type_id = BACKGROUND_SCALE;
    } else if (!strcasecmp(type, "image")) {
	type_id = BACKGROUND_IMAGE;
    } else {
	Warning("invalid background type: \"%s\"\n", type);
	return;
    }

    // create background node
    background = calloc(1, sizeof(*background));
    background->Desktop = desktop;
    background->Type = type_id;
    background->Value = strdup(value);

    // insert node into list
    SLIST_INSERT_HEAD(&Backgrounds, background, Next);
}

#else

/**
**	Add background.
**
**	@param desktop	desktop number for this background
**	@param type	background type (solid, ....)
**	@param value	argument for background type (color, ....)
*/
static void BackgroundSet(int desktop, BackgroundType type, const char *value)
{
    Background *background;

    // make sure we have value
    if (!value) {
	Warning("no value specified for background\n");
	return;
    }
    // create background node
    background = calloc(1, sizeof(*background));
    background->Desktop = desktop;
    background->Type = type;
    background->Value = strdup(value);

    // insert node into list
    SLIST_INSERT_HEAD(&Backgrounds, background, Next);
}

/**
**	Parse configuration for background module.
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
		Debug(3, "background %zd\n", ival);
		// check for valid desktop numbers
		if (ival != -1 && DesktopN) {
		    if (ival < 0 || ival >= DesktopN) {
			Warning("desktop %zd for background not configured\n",
			    ival);
		    }
		}

		if (ConfigGetString(table, &sval, "solid", NULL)) {
		    Debug(3, "\tSolid %s\n", sval);
		    BackgroundSet(ival, BACKGROUND_SOLID, sval);
		} else if (ConfigGetString(table, &sval, "gradient", NULL)) {
		    Debug(3, "\tGradient %s\n", sval);
		    BackgroundSet(ival, BACKGROUND_GRADIENT, sval);
		} else if (ConfigGetString(table, &sval, "execute", NULL)) {
		    Debug(3, "\tExecute %s\n", sval);
		    BackgroundSet(ival, BACKGROUND_COMMAND, sval);
		} else if (ConfigGetString(table, &sval, "image", NULL)) {
		    Debug(3, "\tImage %s\n", sval);
		    BackgroundSet(ival, BACKGROUND_IMAGE, sval);
		} else if (ConfigGetString(table, &sval, "scale", NULL)) {
		    Debug(3, "\tScale %s\n", sval);
		    BackgroundSet(ival, BACKGROUND_SCALE, sval);
		}
	    } else {
		Warning("value in background ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

#endif

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
    xcb_create_pixmap(Connection, RootDepth, background->Pixmap, RootWindow, 1,
	1);

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
    xcb_create_pixmap(Connection, RootDepth, background->Pixmap, RootWindow,
	RootWidth, RootHeight);

    if (color1.pixel == color2.pixel) {
	xcb_rectangle_t rectangle;

	xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, &color1.pixel);
	rectangle.x = 0;
	rectangle.y = 0;
	rectangle.width = RootWidth;
	rectangle.height = RootHeight;
	xcb_poly_fill_rectangle(Connection, background->Pixmap, RootGC, 1,
	    &rectangle);
    } else {
	GradientDrawHorizontal(background->Pixmap, RootGC, color1.pixel,
	    color2.pixel, 0, 0, RootWidth, RootHeight);
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
    // we can't use render on these
    icon->UseRender = 0;

    // determine size of background pixmap
    if (background->Type == BACKGROUND_IMAGE) {
	width = icon->Image->Width;
	height = icon->Image->Height;
    } else {
	width = RootWidth;
	height = RootHeight;
    }

    // create image pixmap
    background->Pixmap = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, RootDepth, background->Pixmap, RootWindow,
	RootWidth, RootHeight);

    // clear pixmap in case it is too small
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND,
	&XcbScreen->black_pixel);
    rectangle.x = 0;
    rectangle.y = 0;
    rectangle.width = RootWidth;
    rectangle.height = RootHeight;
    xcb_poly_fill_rectangle(Connection, background->Pixmap, RootGC, 1,
	&rectangle);

    // draw icon on background pixmap
    IconDraw(icon, background->Pixmap, 0, 0, width, height);

    // we don't need icon anymore
    IconDel(icon);
}
#endif // USE_ICON

/**
**	Initialize background support.
**
**	@todo is XSETROOT_ID still needed to be supported?
*/
void BackgroundInit(void)
{
    xcb_intern_atom_cookie_t ia_cookie;
    xcb_intern_atom_reply_t *ia_reply;
    xcb_get_property_cookie_t gp_cookie;

    // xcb_get_property_reply_t *gp_reply;
    Background *background;

    //
    //	free memory for pixmaps already used as backgrounds.
    //

    // don't intern these old properties
    ia_cookie =
	xcb_intern_atom_unchecked(Connection, 1, sizeof("XSETROOT_ID") - 1,
	"XSETROOT_ID");
    if ((ia_reply = xcb_intern_atom_reply(Connection, ia_cookie, NULL))) {
	if (ia_reply->atom) {
	    Debug(3, "found XSETROOT_ID atom %d\n", ia_reply->atom);
	    gp_cookie =
		xcb_get_property_unchecked(Connection, 1, RootWindow,
		ia_reply->atom, PIXMAP, 0, UINT32_MAX);
	}
	free(ia_reply);
    }

    Debug(2, "FIXME: handle old pixmaps\n");
#if 0

    if (XGetWindowProperty(dpy2, root2, XA_XSETROOT_ID, 0L, 1L, True,
	    XA_PIXMAP, &type, &format, &length, &after, &data) == Success
	&& type == XA_PIXMAP && format == 32 && length == 1 && after == 0
	&& (Pixmap) (*(long *)data) != XCB_NONE) {
	xcb_kill_client(Connection, *((Pixmap *) data));
    }
    if (XGetWindowProperty(dpy2, root2, XA_ESETROOT_PMAP_ID, 0L, 1L, True,
	    XA_PIXMAP, &type, &format, &length, &after, &data) == Success
	&& type == XA_PIXMAP && format == 32 && length == 1 && after == 0
	&& (Pixmap) (*(Pixmap *) data) != XCB_NONE) {
	e_deleted = True;
	xcb_kill_client(Connection, *((Pixmap *) data));
    }
    if (e_deleted) {
	XDeleteProperty(Connection, root2, XA_XROOTPMAP_ID);
    }
#endif

    // load background data
    SLIST_FOREACH(background, &Backgrounds, Next) {
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
	    case BACKGROUND_SCALE:
	    case BACKGROUND_IMAGE:
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
    while (!SLIST_EMPTY(&Backgrounds)) {	// list deletion
	Background *background;

	background = SLIST_FIRST(&Backgrounds);
	if (background->Pixmap) {
	    xcb_free_pixmap(Connection, background->Pixmap);
	}
	free(background->Value);

	SLIST_REMOVE_HEAD(&Backgrounds, Next);
	free(background);
    }

    BackgroundLast = NULL;
}

#endif // } !USE_BACKGROUND

/// @}
