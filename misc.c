///
///	@file misc.c	@brief misc functions
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
///	@defgroup misc The misc module.
///
///	This module contains the functions wich didn't fit in other modules.
///
/// @{

#define _GNU_SOURCE	1		///< fix stpcpy strchrnul

#include <xcb/xcb.h>
#include "uwm.h"

#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>			// PATH_MAX

// ------------------------------------------------------------------------ //
//	Time
// ------------------------------------------------------------------------ //

/**
**	Get ticks in ms.
**
**	@returns ticks in ms,
*/
uint32_t GetMsTicks(void)
{
    struct timeval tval;

    if (gettimeofday(&tval, NULL) < 0) {
	return 0;
    }
    return (tval.tv_sec * 1000) + (tval.tv_usec / 1000);
}

// ------------------------------------------------------------------------ //
//	Tools
// ------------------------------------------------------------------------ //

/**
**	Get users home directory.
**
**	@param username	get the home directory of user with username
**
**	@returns user home directory in a static buffer.
*/
static const char *GetUserHomeDir(const char *username)
{
    struct passwd *user;

    if (!(user = getpwnam(username))) {
	Warning("Could not get password entry for user %s\n", username);
	return "/";
    }
    if (!user->pw_dir) {
	return "/";
    }
    return user->pw_dir;
}

/**
**	Get current users home directory.
**
**	@returns current user home directory in a static buffer.
*/
static const char *GetHomeDir(void)
{
    const char *home;
    struct passwd *user;

    if ((home = getenv("HOME"))) {	// environment exists: easy way
	return home;
    }
    // get home directory from password file entry
    if (!(user = getpwuid(getuid()))) {
	Warning("Could not get password entry for UID %i\n", getuid());
	return "/";
    }
    if (!user->pw_dir) {		// user has no home dir
	return "/";
    }
    return user->pw_dir;
}

/**
**	Expand path. Do shell like macro expansion.
**
**	- ~/ is expanded to the current user home directory.
**	- ~user/ is expanded to the named user home directory.
**	- $macro/ is expanded to the environment value of macro.
**	- $(macro) is expanded to the environment value of macro.
**
**	@param path	expand ~/ and $macro in path
**
**	@returns expanded path, malloced.
*/
char *ExpandPath(const char *path)
{
    char nambuf[PATH_MAX];
    char buffer[PATH_MAX];
    const char *s;
    char *d;
    int j;
    const char *tmp;

    d = buffer;
    s = path;
    if (*s == '~') {			// home directory expansion
	s++;
	if (*s == '/' || !*s) {
	    tmp = GetHomeDir();
	} else {
	    for (j = 0; *s && *s != '/'; ++s) {
		nambuf[j++] = *s;
	    }
	    nambuf[j] = '\0';
	    tmp = GetUserHomeDir(nambuf);
	}
	d = stpcpy(d, tmp);
    }

    while (*s) {
	if (*s == '$') {		// macro expansion
	    s++;
	    // expand $(HOME) or $HOME style environment variables
	    if (*s == '(') {
		s++;
		for (j = 0; *s && *s != ')'; ++s) {
		    nambuf[j++] = *s;
		}
		nambuf[j] = '\0';
		if (*s == ')') {
		    s++;
		}

		if ((tmp = getenv(nambuf))) {
		    d = stpcpy(d, tmp);
		} else {
		    // not found: reinsert macro.
		    d = stpcpy(d, "$(");
		    d = stpcpy(d, nambuf);
		    d = stpcpy(d, ")");
		}
	    } else {
		for (j = 0; *s && *s != '/'; ++s) {
		    nambuf[j++] = *s;
		}
		nambuf[j] = '\0';
		if ((tmp = getenv(nambuf))) {
		    d = stpcpy(d, tmp);
		} else {
		    // not found: reinsert macro.
		    d = stpcpy(d, "$");
		    d = stpcpy(d, nambuf);
		}
	    }
	} else {
	    *d++ = *s++;
	}
    }
    if (d > buffer + 1 && d[-1] == '/') {	// remove any trailing slash
	--d;
    }
    *d = '\0';

    Debug(3, "%s(%s) -> %s\n", __FUNCTION__, path, buffer);

    return strdup(buffer);
}

// ------------------------------------------------------------------------ //
// XMU emulation
// ------------------------------------------------------------------------ //

#ifdef USE_XMU				// {

/**
**	Draw rounded rectangle.  Where @a x, @a y, @a w, @a h are
**	dimensions of overall rectangle, and @a ew and @a eh are sizes of
**	bounding box that corners are drawn inside of.
*/
void xmu_draw_rounded_rectangle(xcb_connection_t * c, xcb_drawable_t draw,
    xcb_gcontext_t gc, int x, int y, int w, int h, int ew, int eh)
{
    xcb_arc_t arcs[8];
    int ew2;
    int eh2;

    if ((ew2 = (ew << 1)) > w) {
	ew2 = ew = 0;
    }
    if ((eh2 = (eh << 1)) > h) {
	eh2 = eh = 0;
    }

    arcs[0].x = x;
    arcs[0].y = y;
    arcs[0].width = ew2;
    arcs[0].height = eh2;
    arcs[0].angle1 = 180 * 64;
    arcs[0].angle2 = -90 * 64;

    arcs[1].x = x + ew;
    arcs[1].y = y;
    arcs[1].width = w - ew2;
    arcs[1].height = 0;
    arcs[1].angle1 = 180 * 64;
    arcs[1].angle2 = -180 * 64;

    arcs[2].x = x + w - ew2;
    arcs[2].y = y;
    arcs[2].width = ew2;
    arcs[2].height = eh2;
    arcs[2].angle1 = 90 * 64;
    arcs[2].angle2 = -90 * 64;

    arcs[3].x = x + w;
    arcs[3].y = y + eh;
    arcs[3].width = 0;
    arcs[3].height = h - eh2;
    arcs[3].angle1 = 90 * 64;
    arcs[3].angle2 = -180 * 64;

    arcs[4].x = x + w - ew2;
    arcs[4].y = y + h - eh2;
    arcs[4].width = ew2;
    arcs[4].height = eh2;
    arcs[4].angle1 = 0;
    arcs[4].angle2 = -90 * 64;

    arcs[5].x = x + ew;
    arcs[5].y = y + h;
    arcs[5].width = w - ew2;
    arcs[5].height = 0;
    arcs[5].angle1 = 0;
    arcs[5].angle2 = -180 * 64;

    arcs[6].x = x;
    arcs[6].y = y + h - eh2;
    arcs[6].width = ew2;
    arcs[6].height = eh2;
    arcs[6].angle1 = 270 * 64;
    arcs[6].angle2 = -90 * 64;

    arcs[7].x = x;
    arcs[7].y = y + eh;
    arcs[7].width = 0;
    arcs[7].height = h - eh2;
    arcs[7].angle1 = 270 * 64;
    arcs[7].angle2 = -180 * 64;

    xcb_poly_arc(c, draw, gc, 8, arcs);
}

/**
**	Fill rounded rectangle.  Where @a x, @a y, @a w, @a h are
**	dimensions of overall rectangle, and @a ew and @a eh are sizes of
**	bounding box that corners are drawn inside of.
**
**	@todo change gc and restore it to original
*/
void xmu_fill_rounded_rectangle(xcb_connection_t * c, xcb_drawable_t draw,
    xcb_gcontext_t gc, int x, int y, int w, int h, int ew, int eh)
{
    xcb_arc_t arcs[4];
    xcb_rectangle_t rects[3];
    uint32_t value[1];
    int ew2, eh2;

#if 0
    XGetGCValues(dpy, gc, GCArcMode, &vals);
    if (vals.arc_mode != ArcPieSlice)
	XSetArcMode(dpy, gc, ArcPieSlice);
#endif
    value[0] = XCB_ARC_MODE_PIE_SLICE;
    xcb_change_gc(Connection, gc, XCB_GC_ARC_MODE, value);

    if ((ew2 = (ew << 1)) > w) {
	ew2 = ew = 0;
    }
    if ((eh2 = (eh << 1)) > h) {
	eh2 = eh = 0;
    }

    arcs[0].x = x;
    arcs[0].y = y;
    arcs[0].width = ew2;
    arcs[0].height = eh2;
    arcs[0].angle1 = 180 * 64;
    arcs[0].angle2 = -90 * 64;

    arcs[1].x = x + w - ew2 - 1;
    arcs[1].y = y;
    arcs[1].width = ew2;
    arcs[1].height = eh2;
    arcs[1].angle1 = 90 * 64;
    arcs[1].angle2 = -90 * 64;

    arcs[2].x = x + w - ew2 - 1;
    arcs[2].y = y + h - eh2 - 1;
    arcs[2].width = ew2;
    arcs[2].height = eh2;
    arcs[2].angle1 = 0;
    arcs[2].angle2 = -90 * 64;

    arcs[3].x = x;
    arcs[3].y = y + h - eh2 - 1;
    arcs[3].width = ew2;
    arcs[3].height = eh2;
    arcs[3].angle1 = 270 * 64;
    arcs[3].angle2 = -90 * 64;

    xcb_poly_fill_arc(c, draw, gc, 4, arcs);

    rects[0].x = x + ew;
    rects[0].y = y;
    rects[0].width = w - ew2;
    rects[0].height = h;

    rects[1].x = x;
    rects[1].y = y + eh;
    rects[1].width = ew;
    rects[1].height = h - eh2;

    rects[2].x = x + w - ew;
    rects[2].y = y + eh;
    rects[2].width = ew;
    rects[2].height = h - eh2;

    xcb_poly_fill_rectangle(c, draw, gc, 3, rects);

#if 0
    if (vals.arc_mode != ArcPieSlice)
	XSetArcMode(dpy, gc, vals.arc_mode);
#endif
}

#endif // } USE_XMU

/// @}
