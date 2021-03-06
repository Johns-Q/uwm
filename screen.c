///
///	@file screen.c	@brief physical screen management functions
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

///
///	@defgroup screen The screen module.
///
///	This module handles physical monitors = screens. Screens are
///	determined using the X11 xinerama extension.
///
///	@todo adding / removing monitors during session is not supported.
///
///< @{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xinerama.h>

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "uwm.h"

#include "client.h"
#include "hints.h"

#include "draw.h"
#include "image.h"
#include "pointer.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"

#include "panel.h"
#include "plugin/task.h"
#include "plugin/pager.h"

#include "screen.h"

// ------------------------------------------------------------------------ //

Screen *Screens;			///< all available screens
int ScreenN;				///< number of screens

/**
**	Get screen at global screen coordinates.
**
**	@param x	x-coordinate
**	@param y	y-coordinate
**
**	@returns screen at coordinates.
*/
const Screen *ScreenGetByXY(int x, int y)
{
#ifdef USE_XINERAMA
    const Screen *screen;
    int i;

    for (i = 0; i < ScreenN; i++) {
	screen = Screens + i;
	if (x >= screen->X && x < screen->X + (signed)screen->Width
	    && y >= screen->Y && y < screen->Y + (signed)screen->Height) {
	    return screen;
	}
    }
#else
    (void)x;
    (void)y;
#endif
    // not found use 0 as default
    return &Screens[0];
}

/**
**	Get screen mouse is currently on.
**
**	@returns The screen containing mouse.
*/
const Screen *ScreenGetPointer(void)
{
#ifdef USE_XINERAMA
    int x, y;

    PointerGetPosition(&x, &y);
    return ScreenGetByXY(x, y);

#else
    return &Screens[0];
#endif
}

/**
**	Initialize screen.
**
**	@todo FIXME: desync, requests, replies
*/
void ScreenInit(void)
{
#ifdef USE_XINERAMA			// {
    xcb_xinerama_is_active_cookie_t active_cookie;
    xcb_xinerama_is_active_reply_t *active_reply;

    active_cookie = xcb_xinerama_is_active_unchecked(Connection);
    active_reply =
	xcb_xinerama_is_active_reply(Connection, active_cookie, NULL);

    if (active_reply) {
	Debug(3, "xcb_xinerama_is_active %d\n", active_reply->state);
	if (active_reply->state) {
	    xcb_xinerama_query_screens_cookie_t query_cookie;
	    xcb_xinerama_query_screens_reply_t *query_reply;

	    query_cookie = xcb_xinerama_query_screens_unchecked(Connection);
	    query_reply =
		xcb_xinerama_query_screens_reply(Connection, query_cookie,
		NULL);
	    if (query_reply) {
		xcb_xinerama_screen_info_iterator_t iter;
		int i;

		// FIXME: or ScreenN = iter.rem;
		iter =
		    xcb_xinerama_query_screens_screen_info_iterator
		    (query_reply);
		ScreenN = iter.rem;
		Screens = calloc(ScreenN, sizeof(*Screens));
		for (i = 0; iter.rem;
		    xcb_xinerama_screen_info_next(&iter), ++i) {
		    Screens[i].X = iter.data->x_org;
		    Screens[i].Y = iter.data->y_org;
		    Screens[i].Width = iter.data->width;
		    Screens[i].Height = iter.data->height;
		}
		free(query_reply);
	    }
	}
	free(active_reply);
	if (ScreenN) {			// success
	    return;
	}
    }
#endif // } USE_XINERAMA
    // no extension 1 screen default
    ScreenN = 1;
    Screens = calloc(1, sizeof(*Screens));
    Screens->Width = XcbScreen->width_in_pixels;
    Screens->Height = XcbScreen->height_in_pixels;
}

/**
**	Cleanup the screen / xinerama module.
*/
void ScreenExit(void)
{
    free(Screens);
    Screens = NULL;
}

/// @}
