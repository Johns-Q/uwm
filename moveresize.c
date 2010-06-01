///
///	@file moveresize.c	@brief move/resize window functions.
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

#include <xcb/xcb.h>
#include "uwm.h"

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_aux.h>

#include "array.h"
#include "config.h"

#include "client.h"
#include "border.h"

#include "draw.h"
#include "screen.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "panel.h"

extern Config *UwmConfig;		///< µwm config

//////////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------ //
// Status
// ------------------------------------------------------------------------ //

///
///	@ingroup client
///	@defgroup status The status window module
///
///	This module handles the display of the status for window move and
///	resize.
///
///	This module is only available, if compiled with #USE_STATUS.
///
/// @{

#ifdef USE_STATUS			// {

/**
**	Status window type enumeration.
*/
typedef enum
{
    STATUS_WINDOW_INVALID,		///< invalid type of status
    STATUS_WINDOW_OFF,			///< status window turned off
    STATUS_WINDOW_CENTER_SCREEN,	///< center of screen
    STATUS_WINDOW_CENTER_WINDOW,	///< center of window
    STATUS_WINDOW_CORNER_SCREEN,	///< corner of screen
    STATUS_WINDOW_CENTER_PANEL		///< center of panel
} StatusType;

/**
**	Status window structure typedef.
*/
typedef struct _status_window_
{
    xcb_window_t Window;		///< status window self
    int16_t X;				///< window x-coordinate
    int16_t Y;				///< window y-coordinate
    uint32_t Height;			///< height of window
    uint32_t Width;			///< width of window
} StatusWindow;

static StatusWindow *Status;		///< status window local variables

static StatusType StatusMoveType;	///< type for move status
static StatusType StatusResizeType;	///< type for resize status
static int16_t StatusMoveX;		///< x-coordinate relative to type
static int16_t StatusMoveY;		///< y-coordinate relative to type
static int16_t StatusResizeX;		///< x-coordinate relative to type
static int16_t StatusResizeY;		///< y-coordinate relative to type

/**
**	Get location to place status window.
**
**	@param client		status window for client.
**	@param type		type of status window
**	@param[in,out] x	x-coordinate relative to type
**	@param[in,out] y	y-coordinate relative to type
*/
static void StatusGetCoordinates(const Client * client, StatusType type,
    int *x, int *y)
{
    const Screen *screen;

    // center of window
    if (type == STATUS_WINDOW_CENTER_WINDOW) {
	*x += client->X + client->Width / 2 - Status->Width / 2;
	*y += client->Y + client->Height / 2 - Status->Height / 2;
	return;
    }
    // center of panel
    if (type == STATUS_WINDOW_CENTER_PANEL) {
	const Panel *panel;

	panel = SLIST_FIRST(&Panels);
	*x += panel->X + panel->Width / 2 - Status->Width / 2;
	*y += panel->Y + panel->Height / 2 - Status->Height / 2;
	return;
    }

    screen = ScreenGetByXY(client->X, client->Y);

    // corner of screen
    if (type == STATUS_WINDOW_CORNER_SCREEN) {
	if (*x >= 0) {
	    *x += screen->X;
	} else {
	    *x += screen->Width - Status->Width;

	}
	if (*y >= 0) {
	    *y += screen->Y;
	} else {
	    *y += screen->Height - Status->Height;
	}
	return;
    }
    // center of screen
    *x += screen->X + screen->Width / 2 - Status->Width / 2;
    *y += screen->Y + screen->Height / 2 - Status->Height / 2;
}

/**
**	Create status window.
**
**	@param client	status window for client.
**	@param type	type of status window.
**	@param x	x-coordinate relative to type
**	@param y	y-coordinate relative to type
*/
static void StatusCreateWindow(const Client * client, StatusType type, int x,
    int y)
{
    xcb_query_text_extents_cookie_t cookie;
    unsigned width;
    uint32_t values[3];

    if (type == STATUS_WINDOW_OFF) {
	return;
    }
    // FIXME: cache font width
    cookie =
	FontQueryExtentsRequest(&Fonts.Menu, sizeof(" 00000 x 00000 ") - 1,
	" 00000 x 00000 ");

    Status = malloc(sizeof(*Status));

    Status->Height = Fonts.Menu.Height + 8;
    StatusGetCoordinates(client, type, &x, &y);
    Status->X = x;
    Status->Y = y;
    Status->Window = xcb_generate_id(Connection);

    values[0] = Colors.MenuBG.Pixel;
    values[1] = 1;
    values[2] = 1;

    width = FontTextWidthReply(cookie);
    Status->Width = width;

    xcb_create_window(Connection,	// connection
	XCB_COPY_FROM_PARENT,		// depth (same as root)
	Status->Window,			// window Id
	RootWindow,			// parent window
	x, y,				// x, y
	width, Status->Height,		// width, height
	0,				// border_width
	XCB_WINDOW_CLASS_INPUT_OUTPUT,	// class
	XCB_COPY_FROM_PARENT,		// visual (same as root)
	XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_SAVE_UNDER, values);	// mask, values

    // raise window
    values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(Connection, Status->Window,
	XCB_CONFIG_WINDOW_STACK_MODE, values);
    // shape window corners.
    ShapeRoundedRectWindow(Status->Window, Status->Width, Status->Height);
    // map window on screen
    xcb_map_window(Connection, Status->Window);
}

/**
**	Destroy move/resize status window.
*/
static void StatusDestroyWindow(void)
{
    if (Status) {
	xcb_destroy_window(Connection, Status->Window);
	free(Status);
	Status = NULL;
    }
}

/**
**	Draw status window.
**
**	@param client	status window for this client
**	@param type	type of status window
**	@param x	x-coordinate relative to type
**	@param y	y-coordinate relative to type
*/
void StatusUpdateWindow(const Client * client, StatusType type, int x, int y)
{
    StatusGetCoordinates(client, type, &x, &y);
    // update position, if needed
    if (x != Status->X || y != Status->X) {
	uint32_t values[2];

	Status->X = x;
	Status->Y = y;
	values[0] = x;
	values[1] = y;
	xcb_configure_window(Connection, Status->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
    }
    // clear background.
    xcb_aux_clear_window(Connection, Status->Window);

    // draw border.
    xcb_change_gc(Connection, RootGC, XCB_GC_FOREGROUND, &Colors.MenuFG.Pixel);

#ifdef USE_XMU
    xmu_draw_rounded_rectangle(Connection, Status->Window, RootGC, 0, 0,
	Status->Width - 1, Status->Height - 1, CORNER_RADIUS, CORNER_RADIUS);
#else
    if (1) {
	xcb_rectangle_t rectangle;

	rectangle.x = 0;
	rectangle.y = 0;
	rectangle.width = Status->Width - 1;
	rectangle.height = Status->Height - 1;
	xcb_poly_rectangle(Connection, Status->Window, RootGC, 1, &rectangle);
    }
#endif
}

/**
**	Create move status window.
**
**	@param client	client being moved
*/
void StatusCreateMove(const Client * client)
{
    StatusCreateWindow(client, StatusMoveType, StatusMoveX, StatusMoveY);
}

/**
**	Destroy move status window.
*/
void StatusDestroyMove(void)
{
    StatusDestroyWindow();
}

/**
**	Update move status window.
**
**	@param client	client being moved
*/
void StatusUpdateMove(const Client * client)
{
    char buf[80];
    xcb_query_text_extents_cookie_t cookie;
    unsigned width;
    int i;

    if (StatusMoveType == STATUS_WINDOW_OFF) {
	return;
    }

    i = snprintf(buf, sizeof(buf), "(%d, %d)", client->X, client->Y);
    cookie = FontQueryExtentsRequest(&Fonts.Menu, i, buf);

    StatusUpdateWindow(client, StatusMoveType, StatusMoveX, StatusMoveY);

    width = FontTextWidthReply(cookie);
    FontDrawString(Status->Window, &Fonts.Menu, Colors.MenuFG.Pixel,
	Status->Width / 2 - width / 2, 4, width, NULL, buf);
}

/**
**	Create resize status window.
**
**	@param client	client being resized
*/
void StatusCreateResize(const Client * client)
{
    StatusCreateWindow(client, StatusResizeType, StatusResizeX, StatusResizeY);
}

/**
**	Destroy resize status window.
*/
#define StatusDestroyResize()	StatusDestroyMove()

/**
**	Update resize status window.
**
**	@param client	client being resized
**	@param width	width to display
**	@param height	height to display
*/
void StatusUpdateResize(const Client * client, unsigned width, unsigned height)
{
    char buf[80];
    xcb_query_text_extents_cookie_t cookie;
    int i;

    if (StatusResizeType == STATUS_WINDOW_OFF) {
	return;
    }

    i = snprintf(buf, sizeof(buf), "%d x %d", width, height);
    cookie = FontQueryExtentsRequest(&Fonts.Menu, i, buf);

    StatusUpdateWindow(client, StatusResizeType, StatusResizeX, StatusResizeY);

    width = FontTextWidthReply(cookie);
    FontDrawString(Status->Window, &Fonts.Menu, Colors.MenuFG.Pixel,
	Status->Width / 2 - width / 2, 4, width, NULL, buf);
}

// ------------------------------------------------------------------------ //
// Config

/**
**	Set location of resize status windows.
**
**	@param str	location (off, screen, window, or corner)
*/
static StatusType StatusParseType(const char *str)
{
    if (!str) {
	return STATUS_WINDOW_CENTER_SCREEN;
    } else if (!strcasecmp(str, "off")) {
	return STATUS_WINDOW_OFF;
    } else if (!strcasecmp(str, "screen")) {
	return STATUS_WINDOW_CENTER_SCREEN;
    } else if (!strcasecmp(str, "window")) {
	return STATUS_WINDOW_CENTER_WINDOW;
    } else if (!strcasecmp(str, "corner")) {
	return STATUS_WINDOW_CORNER_SCREEN;
    } else if (!strcasecmp(str, "panel")) {
	return STATUS_WINDOW_CENTER_PANEL;
    } else {
	return STATUS_WINDOW_INVALID;
    }
}

/**
**	Parse status window configuration.
*/
void StatusConfig(void)
{
    const char *sval;
    ssize_t ival;

    StatusMoveType = STATUS_WINDOW_CENTER_SCREEN;
    StatusMoveX = 0;
    StatusMoveY = 0;
    // FIXME: get array and than values
    if (ConfigGetString(ConfigDict(UwmConfig), &sval, "move", "status", "type",
	    NULL)) {
	StatusType type;

	type = StatusParseType(sval);
	if (type != STATUS_WINDOW_INVALID) {
	    Debug(3, "move status window type %d\n", type);
	    StatusMoveType = type;
	} else {
	    Warning("invalid move status type: \"%s\"\n", sval);
	}
    }
    if (ConfigGetInteger(ConfigDict(UwmConfig), &ival, "move", "status", "x",
	    NULL)) {
	StatusMoveX = ival;
    }
    if (ConfigGetInteger(ConfigDict(UwmConfig), &ival, "move", "status", "y",
	    NULL)) {
	StatusMoveY = ival;
    }
    StatusResizeType = STATUS_WINDOW_CENTER_SCREEN;
    StatusResizeX = 0;
    StatusResizeY = 0;
    // FIXME: get array and than values
    if (ConfigGetString(ConfigDict(UwmConfig), &sval, "resize", "status",
	    "type", NULL)) {
	StatusType type;

	type = StatusParseType(sval);
	if (type != STATUS_WINDOW_INVALID) {
	    Debug(3, "resize status window type %d\n", type);
	    StatusResizeType = type;
	} else {
	    Warning("invalid resize status type: \"%s\"\n", sval);
	}
    }
    if (ConfigGetInteger(ConfigDict(UwmConfig), &ival, "resize", "status", "x",
	    NULL)) {
	StatusResizeX = ival;
    }
    if (ConfigGetInteger(ConfigDict(UwmConfig), &ival, "resize", "status", "y",
	    NULL)) {
	StatusResizeY = ival;
    }
}

#else // }{ USE_STATUS

#endif // } !USE_STATUS
/// @}
