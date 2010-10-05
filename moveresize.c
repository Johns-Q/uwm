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

#include <X11/keysym.h>			// keysym XK_

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "misc.h"
#include "event.h"
#include "pointer.h"
#include "client.h"
#include "border.h"

#include "draw.h"
#include "screen.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "panel.h"

#include "moveresize.h"
#include "keyboard.h"
#include "hints.h"

#include "plugin/pager.h"

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
    if (x != Status->X || y != Status->Y) {
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
**
**	@returns the #StatusType for window location.
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
**
**	@param config	global config dictionary
*/
void StatusConfig(const Config * config)
{
    const char *sval;
    ssize_t ival;

    StatusMoveType = STATUS_WINDOW_CENTER_SCREEN;
    StatusMoveX = 0;
    StatusMoveY = 0;
    // FIXME: get array and than values
    if (ConfigGetString(ConfigDict(config), &sval, "move", "status", "type",
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
    if (ConfigGetInteger(ConfigDict(config), &ival, "move", "status", "x",
	    NULL)) {
	StatusMoveX = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "move", "status", "y",
	    NULL)) {
	StatusMoveY = ival;
    }
    StatusResizeType = STATUS_WINDOW_CENTER_SCREEN;
    StatusResizeX = 0;
    StatusResizeY = 0;
    // FIXME: get array and than values
    if (ConfigGetString(ConfigDict(config), &sval, "resize", "status", "type",
	    NULL)) {
	StatusType type;

	type = StatusParseType(sval);
	if (type != STATUS_WINDOW_INVALID) {
	    Debug(3, "resize status window type %d\n", type);
	    StatusResizeType = type;
	} else {
	    Warning("invalid resize status type: \"%s\"\n", sval);
	}
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "resize", "status", "x",
	    NULL)) {
	StatusResizeX = ival;
    }
    if (ConfigGetInteger(ConfigDict(config), &ival, "resize", "status", "y",
	    NULL)) {
	StatusResizeY = ival;
    }
}

#else // }{ USE_STATUS

#endif // } !USE_STATUS

/// @}

// ------------------------------------------------------------------------ //
// Outline
// ------------------------------------------------------------------------ //

///
///	@ingroup client
///	@defgroup outline The outline module
///
///	This module handles outline for client window move and resize.
///
///	This module is only available, if compiled with #USE_OUTLINE.
/// @{

#ifdef USE_OUTLINE			// {

static int OutlineDrawn;		///< flag outline drawn on screen
static xcb_rectangle_t OutlineLast;	///< last outline rectangle
static xcb_gcontext_t OutlineGC;	///< outline graphic context

/**
**	Draw an outline.
**
**	@param x	x-coordinate
**	@param y	y-coordinate
**	@param width	width of outline
**	@param height	height of outline
*/
static void OutlineDraw(int x, int y, unsigned width, unsigned height)
{
    if (!OutlineDrawn) {
	xcb_aux_sync(Connection);
	xcb_grab_server(Connection);

	OutlineLast.x = x;
	OutlineLast.y = y;
	OutlineLast.width = width;
	OutlineLast.height = height;
	xcb_poly_rectangle(Connection, RootWindow, OutlineGC, 1, &OutlineLast);

	OutlineDrawn = 1;
    }
}

/**
**	Clear last outline.
*/
static void OutlineClear(void)
{
    if (OutlineDrawn) {
	xcb_poly_rectangle(Connection, RootWindow, OutlineGC, 1, &OutlineLast);

	xcb_aux_sync(Connection);
	xcb_ungrab_server(Connection);

	OutlineDrawn = 0;
    }
}

/**
**	Initialize outlines.
*/
void OutlineInit(void)
{
    uint32_t values[3];

    OutlineGC = xcb_generate_id(Connection);

    values[0] = XCB_GX_INVERT;
    values[1] = 2;
    values[2] = XCB_SUBWINDOW_MODE_INCLUDE_INFERIORS;
    xcb_create_gc(Connection, OutlineGC, RootWindow,
	XCB_GC_FUNCTION | XCB_GC_LINE_WIDTH | XCB_GC_SUBWINDOW_MODE, values);

    OutlineDrawn = 0;
}

/**
**	Cleanup outlines.
*/
void OutlineExit(void)
{
    xcb_free_gc(Connection, OutlineGC);
}

#else // { !USE_OUTLINE

    /// Dummy for Clear last outline.
#define OutlineClear()

    /// Dummy for Draw an outline.
#define OutlineDraw(x, y, width, height)

#endif // } !USE_OUTLINE

/// @}

// ------------------------------------------------------------------------ //
// Client snap
// ------------------------------------------------------------------------ //

///
///	@ingroup client
///	@defgroup snap The client snap module.
///
///	This module contains the client snap functions.
///
///	This module is only available, if compiled with #USE_SNAP.
///
///	@todo FIXME: disable isn't supported yet.
/// @{

#ifdef USE_SNAP				// {

/**
**	Window snap modes enumeration.
*/
typedef enum
{
    SNAP_NONE,				///< don't snap
    SNAP_CLIENT,			///< snap to edges of windows
    SNAP_SCREEN,			///< snap to edges of screen
    SNAP_BORDER				///< snap to all borders
} SnapMode;

/**
**	Box structure for snapping.
*/
typedef struct _box_
{
    int16_t X1;				///< left side x-coordinate
    int16_t Y1;				///< top side y-coordinate
    int16_t X2;				///< right side x-coordinate
    int16_t Y2;				///< bottom side y-coordinate
} Box;

    /// client window snap mode
static SnapMode ClientSnapMode;

    /// client window snap distance
static int ClientSnapDistance;

/**
**	Get box to represent client window.
**
**	@param client	create the box for this client
**	@param[out] box	box of client top/left bottom/right coordinates
*/
static void ClientGetBox(const Client * client, Box * box)
{
    int north;
    int south;
    int east;
    int west;

    BorderGetSize(client, &north, &south, &east, &west);

    box->X1 = client->X - west;
    box->X2 = client->X + client->Width + east;
    box->Y1 = client->Y - north;
    if (client->State & WM_STATE_SHADED) {
	box->Y2 = client->Y + south;
    } else {
	box->Y2 = client->Y + client->Height + south;
    }

}

/**
**	Check if current right snap position is valid.
**
**	@param self	box arround our client
**	@param other	box arround other object
**	@param right	old right snap box, until now best snap box
**
**	@retval	true	keep the right box
**	@retval	false	other is better
*/
static int ClientCheckRight(const Box * self, const Box * other,
    const Box * right)
{
    if (right->X1 < other->X1) {
	return 1;
    }
    // if right and client go higher than other then still valid
    if (right->Y1 < other->Y1 && self->Y1 < other->Y1) {
	return 1;
    }
    // if right and client go lower than other then still valid
    if (right->Y2 > other->Y2 && self->Y2 > other->Y2) {
	return 1;
    }
    if (other->X2 <= right->X1) {
	return 1;
    }

    return 0;
}

/**
**	Check if current left snap position is valid.
**
**	@param self	box arround our client
**	@param other	box arround other object
**	@param left	old left snap box, until now best snap box
**
**	@retval	true	keep the left box
**	@retval	false	other is better
*/
static int ClientCheckLeft(const Box * self, const Box * other,
    const Box * left)
{
    if (left->X2 > other->X2) {
	return 1;
    }
    // if left and client go higher than other then still valid
    if (left->Y1 < other->Y1 && self->Y1 < other->Y1) {
	return 1;
    }
    // If left and client go lower than other then still valid
    if (left->Y2 > other->Y2 && self->Y2 > other->Y2) {
	return 1;
    }

    if (other->X1 >= left->X2) {
	return 1;
    }

    return 0;
}

/**
**	Check if current top snap position is valid.
**
**	@param self	box arround our client
**	@param other	box arround other object
**	@param top	old top snap box, until now best snap box
**
**	@retval	true	keep the top box
**	@retval	false	other is better
*/
static int ClientCheckTop(const Box * self, const Box * other, const Box * top)
{
    if (top->Y2 > other->Y2) {
	return 1;
    }
    // if top and client are to left of other then still valid
    if (top->X1 < other->X1 && self->X1 < other->X1) {
	return 1;
    }
    // If top and client are to right of other then still valid
    if (top->X2 > other->X2 && self->X2 > other->X2) {
	return 1;
    }
    if (other->Y1 >= top->Y2) {
	return 1;
    }

    return 0;
}

/**
**	Check if current bottom snap position is valid.
**
**	@param self	box arround our client
**	@param other	box arround other object
**	@param bottom	old bottom snap box, until now best snap box
**
**	@retval	true	keep the bottom box
**	@retval	false	other is better
*/
static int ClientCheckBottom(const Box * self, const Box * other,
    const Box * bottom)
{
    if (bottom->Y1 < other->Y1) {
	return 1;
    }
    // if bottom and client are to left of other then still valid
    if (bottom->X1 < other->X1 && self->X1 < other->X1) {
	return 1;
    }
    // if bottom and client are to right of other then still valid
    if (bottom->X2 > other->X2 && self->X2 > other->X2) {
	return 1;
    }
    if (other->Y2 <= bottom->Y1) {
	return 1;
    }

    return 0;
}

/**
**	Check for top/bottom overlap.
**
**	@param self	box arround our client
**	@param other	box arround other object
**
**	@returns true if the two boxes intersect, false if not.
*/
static int ClientCheckTopBottom(const Box * self, const Box * other)
{
    if (self->Y1 >= other->Y2 || self->Y2 <= other->Y1) {
	return 0;
    } else {
	return 1;
    }
}

/**
**	Check for left/right overlap.
**
**	@param self	box arround our client
**	@param other	box arround other object
**
**	@returns true if the two boxes intersect, false if not.
*/
static int ClientCheckLeftRight(const Box * self, const Box * other)
{
    if (self->X1 >= other->X2 || self->X2 <= other->X1) {
	return 0;
    } else {
	return 1;
    }
}

/**
**	Determine if we should snap to specified client.
**
**	Hidden or minimized windows are ignored.
**
**	@param client	window client to check
**
**	@returns true if we should use this client, false if not.
*/
static int ClientShouldSnap(const Client * client)
{
    if (client->State & (WM_STATE_HIDDEN | WM_STATE_MINIMIZED)) {
	return 0;
    } else {
	return 1;
    }
}

/**
**	Check if new border box is better than existing snap boxes.
**
**	@param self		box arround our client
**	@param other		box arround other object
**	@param left_valid	left snap box is valid
**	@param left		best left snap box
**	@param right_valid	right snap box is valid
**	@param right		best right snap box
**	@param top_valid	top snap box is valid
**	@param top		best top snap box
**	@param bottom_valid	bottom snap box is valid
**	@param bottom		best bottom snap box
*/
static void ClientDoSnapWork(const Box * self, Box * other, int *left_valid,
    Box * left, int *right_valid, Box * right, int *top_valid, Box * top,
    int *bottom_valid, Box * bottom)
{
    // check if this border invalidates any previous value
    if (*left_valid) {
	*left_valid = ClientCheckLeft(self, other, left);
    }
    if (*right_valid) {
	*right_valid = ClientCheckRight(self, other, right);
    }
    if (*top_valid) {
	*top_valid = ClientCheckTop(self, other, top);
    }
    if (*bottom_valid) {
	*bottom_valid = ClientCheckBottom(self, other, bottom);
    }
    // compute new snap values.
    if (ClientCheckTopBottom(self, other)) {
	if (abs(self->X1 - other->X2) <= ClientSnapDistance) {
	    *left_valid = 1;
	    *left = *other;
	}
	if (abs(self->X2 - other->X1) <= ClientSnapDistance) {
	    *right_valid = 1;
	    *right = *other;
	}
    }
    if (ClientCheckLeftRight(self, other)) {
	if (abs(self->Y1 - other->Y2) <= ClientSnapDistance) {
	    *top_valid = 1;
	    *top = *other;
	}
	if (abs(self->Y2 - other->Y1) <= ClientSnapDistance) {
	    *bottom_valid = 1;
	    *bottom = *other;
	}
    }
}

/**
**	Snap to window borders.
**
**	@param client	client wich should be snapped.
*/
static void ClientSnapToBorder(Client * client)
{
    Box self;
    Box other;
    int left_valid;
    Box left;
    int right_valid;
    Box right;
    int top_valid;
    Box top;
    int bottom_valid;
    Box bottom;
    int layer;
    int north;
    int south;
    int east;
    int west;

    ClientGetBox(client, &self);

    left_valid = 0;
    right_valid = 0;
    top_valid = 0;
    bottom_valid = 0;

#ifdef DEBUG
    // keep gcc happy
    memset(&bottom, 0, sizeof(bottom));
    memset(&top, 0, sizeof(top));
    memset(&right, 0, sizeof(right));
    memset(&left, 0, sizeof(left));
#endif

    // work from bottom of window stack to top.
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	const Panel *panel;
	const Client *temp;

	// check panel windows
	SLIST_FOREACH(panel, &Panels, Next) {
	    // ignore hidden panels
	    if (panel->Hidden) {
		continue;
	    }

	    other.X1 = panel->X;
	    other.X2 = panel->X + panel->Width;
	    other.Y1 = panel->Y;
	    other.Y2 = panel->Y + panel->Height;

	    ClientDoSnapWork(&self, &other, &left_valid, &left, &right_valid,
		&right, &top_valid, &top, &bottom_valid, &bottom);
	}

	// check client windows
	TAILQ_FOREACH(temp, &ClientLayers[layer], LayerQueue) {
	    // ignore self and invalid windows
	    if (temp == client || !ClientShouldSnap(temp)) {
		continue;
	    }

	    ClientGetBox(temp, &other);

	    ClientDoSnapWork(&self, &other, &left_valid, &left, &right_valid,
		&right, &top_valid, &top, &bottom_valid, &bottom);
	}
    }

    BorderGetSize(client, &north, &south, &east, &west);

    // any valid points found use them
    if (left_valid) {			// prefer left
	client->X = left.X2 + east;
    } else if (right_valid) {
	client->X = right.X1 - client->Width - west;
    }
    if (top_valid) {			// prefer top
	client->Y = top.Y2 + north;
    } else if (bottom_valid) {
	client->Y = bottom.Y1 - south;
	if (!(client->State & WM_STATE_SHADED)) {
	    client->Y -= client->Height;
	}
    }
}

/**
**	Snap to screen.
**
**	@param client	client moved arround.
*/
static void ClientSnapToScreen(Client * client)
{
    Box self;
    int north;
    int south;
    int east;
    int west;
    int s;

    ClientGetBox(client, &self);
    BorderGetSize(client, &north, &south, &east, &west);

    for (s = 0; s < ScreenN; s++) {
	Screen *screen;

	screen = Screens + s;

	// try four screen sides
	if (abs(self.X1 - screen->X) <= ClientSnapDistance) {
	    client->X = screen->X + east;
	    Debug(4, "snap %d\n", client->X);
	} else if (abs(self.X2 - screen->Width - screen->X) <=
	    ClientSnapDistance) {
	    client->X = screen->X + screen->Width - west - client->Width;
	    Debug(4, "snap %d\n", client->X);
	}
	if (abs(self.Y1 - screen->Y) <= ClientSnapDistance) {
	    client->Y = north + screen->Y;
	    Debug(4, "snap %d\n", client->Y);
	} else if (abs(self.Y2 - screen->Height - screen->Y) <=
	    ClientSnapDistance) {
	    client->Y = screen->Y + screen->Height - south;
	    if (!(client->State & WM_STATE_SHADED)) {
		client->Y -= client->Height;
	    }
	    Debug(4, "snap %d\n", client->Y);
	}
    }
}

/**
**	Snap to screen and/or neighboring windows.
**
**	@param client	client moved arround.
*/
void ClientSnap(Client * client)
{
    switch (ClientSnapMode) {
	case SNAP_CLIENT:
	    ClientSnapToBorder(client);
	    break;
	case SNAP_BORDER:
	    ClientSnapToBorder(client);
	    // fall through snaps larger distance, not good!
	case SNAP_SCREEN:
	    ClientSnapToScreen(client);
	    break;
	default:
	    break;
    }
}

// ------------------------------------------------------------------------ //
// Config

/**
**	Parse snap configuration.
**
**	@param config	global config dictionary
*/
void SnapConfig(const Config * config)
{
    const char *sval;
    ssize_t ival;

    // snap.mode
    if (ConfigGetString(ConfigDict(config), &sval, "snap", "mode", NULL)) {
	if (!strcasecmp(sval, "none")) {
	    ClientSnapMode = SNAP_NONE;
	} else if (!strcasecmp(sval, "client")) {
	    ClientSnapMode = SNAP_CLIENT;
	} else if (!strcasecmp(sval, "screen")) {
	    ClientSnapMode = SNAP_SCREEN;
	} else if (!strcasecmp(sval, "border")) {
	    ClientSnapMode = SNAP_BORDER;
	} else {
	    ClientSnapMode = SNAP_BORDER;
	    Warning("invalid snap mode: '%s'\n", sval);
	}
    }
    // snap.distance
    if (ConfigGetInteger(ConfigDict(config), &ival, "snap", "distance", NULL)) {
	if (SNAP_MINIMAL_DISTANCE <= ival && ival <= SNAP_MAXIMAL_DISTANCE) {
	    ClientSnapDistance = ival;
	} else {
	    ClientSnapDistance = SNAP_DEFAULT_DISTANCE;
	    Warning("snap distance %zd out of range\n", ival);
	}
    }
}

#endif // } USE_SNAP

/// @}

// ------------------------------------------------------------------------ //
// Client move
// ------------------------------------------------------------------------ //

///
///	@ingroup client
///	@defgroup move The client window move module.
///
///	This module contains the client window move functions.
///
/// @{

/**
**	Window move modes enumeration.
*/
typedef enum
{
    MOVE_OPAQUE,			///< show window contents while moving
    MOVE_OUTLINE			///< show an outline while moving
} MoveMode;

    /// client window move mode
static MoveMode ClientMoveMode = MOVE_OPAQUE;

/**
**	Stop client move.
**
**	@param client	client moved arround
**	@param do_move	client is already moved
**	@param oldx	original x-coordinate
**	@param oldy	original y-coordinate
**	@param hmax	original hmax flag
**	@param vmax	original vmax flag
*/
static void ClientStopMove(Client * client, int do_move, int oldx, int oldy,
    int hmax, int vmax)
{
    client->Controller();
    client->Controller = ClientDefaultController;

    if (do_move) {
	int north;
	int south;
	int east;
	int west;
	uint32_t values[2];

	BorderGetSize(client, &north, &south, &east, &west);

	values[0] = client->X - west;
	values[1] = client->Y - north;
	xcb_configure_window(Connection, client->Parent,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	ClientSendConfigureEvent(client);

	// restore maximized status, if only maximized in one direction.
	if ((hmax || vmax) && !(hmax && vmax)) {
	    // FIXME: this sends configure the second time, see above
	    ClientMaximize(client, hmax, vmax);
	}
    } else {
	client->X = oldx;
	client->Y = oldy;
    }
}

/**
**	Callback for stopping moves.
*/
static void ClientMoveController(void)
{
    xcb_ungrab_keyboard(Connection, XCB_CURRENT_TIME);
    xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);

    if (ClientMoveMode == MOVE_OUTLINE) {
	OutlineClear();
    }
    StatusDestroyMove();

    ClientFinishAction = 1;
}

/**
**	Interactive move client window.
**
**	@param client	client to move
**	@param button	button initiating move (0 keyboard/menu)
**	@param startx	starting mouse x-coordinate (window relative)
**	@param starty	starting mouse y-coordinate (window relative)
**
**	@returns true if client moved, false otherwise.
**
**	@todo make keyboard move distance configurable.
*/
int ClientMoveLoop(Client * client, int button, int startx, int starty)
{
    xcb_grab_pointer_cookie_t gp_cookie;
    xcb_grab_keyboard_cookie_t gk_cookie;
    int oldx;
    int oldy;
    int north;
    int south;
    int east;
    int west;
    int vmax;
    int hmax;
    int do_move;
    int height;

    // move allowed?
    if (!(client->Border & BORDER_MOVE)) {
	return 0;
    }

    gp_cookie = PointerGrabForMoveRequest();
    NO_WARNING(gk_cookie);
    if (!button) {
	gk_cookie = KeyboardGrabRequest(client->Window);
    }

    ClientFinishAction = 0;
    client->Controller = ClientMoveController;

    oldx = client->X;
    oldy = client->Y;

    BorderGetSize(client, &north, &south, &east, &west);

    if (button) {
	startx -= west;
	starty -= north;
    }

    height = north + south;		// window height for outline/keyboard
    if (!(client->State & WM_STATE_SHADED)) {
	height += client->Height;
    }

    vmax = 0;
    hmax = 0;
    do_move = 0;

    PointerGrabReply(gp_cookie);
    // FIXME: what if grab failed?
    if (button) {
	// FIXME; must convert button into mask
	if (!(PointerGetButtonMask() & (XCB_BUTTON_MASK_1 |
		    XCB_BUTTON_MASK_2))) {
	    Debug(3, "only border clicked, leave early\n");
	    ClientStopMove(client, 0, oldx, oldy, 0, 0);
	    return 0;
	}
    } else {
	// FIXME: fails if shaded
	if (!KeyboardGrabReply(gk_cookie)) {
	    ClientStopMove(client, 0, oldx, oldy, 0, 0);
	    return 0;
	}
    }

    for (;;) {
	for (;;) {
	    xcb_generic_event_t *event;

	    if (ClientFinishAction || !KeepLooping) {
		client->Controller = ClientDefaultController;
		return do_move;
	    }
	    if (!(event = PollNextEvent())) {
		break;
	    }

	    switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		    int delta_x;
		    int delta_y;

		case XCB_KEY_RELEASE:
		    break;
		case XCB_KEY_PRESS:
		    delta_x = 0;
		    delta_y = 0;
		    switch (KeyboardGet(((xcb_key_press_event_t *)
				event)->detail, ((xcb_key_press_event_t *)
				event)->state)) {
			case XK_Left:
			    delta_x = -10;
			    break;
			case XK_Right:
			    delta_x = 10;
			    break;
			case XK_Up:
			    delta_y = -10;
			    break;
			case XK_Down:
			    delta_y = 10;
			    break;
			case XK_Home:
			    delta_x = -10;
			    delta_y = -10;
			    break;
			case XK_End:
			    delta_x = -10;
			    delta_y = 10;
			    break;
			case XK_Page_Up:
			    delta_x = 10;
			    delta_y = -10;
			    break;
			case XK_Page_Down:
			    delta_x = 10;
			    delta_y = 10;
			    break;

			case XK_Shift_L:
			case XK_Shift_R:
			case XK_Control_L:
			case XK_Control_R:
			    break;
			case XK_Escape:
			case XK_Return:
			default:
			    ClientStopMove(client, do_move, oldx, oldy, hmax,
				vmax);
			    free(event);
			    return do_move;
		    }
		    if (((xcb_key_press_event_t *) event)->state &
			XCB_MOD_MASK_CONTROL) {
			delta_x /= 10;
			delta_y /= 10;
		    }
		    if (((xcb_key_press_event_t *) event)->state &
			XCB_MOD_MASK_SHIFT) {
			delta_x *= 3;
			delta_y *= 3;
		    }
		    //
		    //	Keep window on screen
		    //
		    if (client->X + delta_x + west + (signed)client->Width > 0
			&& client->X + delta_x - west <
			(signed)XcbScreen->width_in_pixels) {
			client->X += delta_x;
		    }
		    if (client->Y + delta_y - north + height > 0
			&& client->Y + delta_y - north <
			(signed)XcbScreen->height_in_pixels) {
			client->Y += delta_y;
		    }
#if 0
		    // FIXME: see ClientMoveKeyboard
		    PointerWrap(RootWindow, client->X + west,
			client->Y + north);
		    DiscardMotionEvents((xcb_motion_notify_event_t **) & event,
			client->Window);
#endif
		    goto do_move;

		case XCB_BUTTON_RELEASE:
		    if (((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_1
			|| ((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_2) {
			ClientStopMove(client, do_move, oldx, oldy, hmax,
			    vmax);
			free(event);
			return do_move;
		    }
		case XCB_BUTTON_PRESS:
		    break;
		case XCB_MOTION_NOTIFY:

		    Debug(3, "window %x\n",
			((xcb_motion_notify_event_t *) event)->event);
		    DiscardMotionEvents((xcb_motion_notify_event_t **) & event,
			client->Window);

		    // FIXME: when we don't change client->X here, we don't
		    // FIXME: need to restore X,Y in StopMove!!
		    client->X =
			((xcb_motion_notify_event_t *) event)->root_x - startx;
		    client->Y =
			((xcb_motion_notify_event_t *) event)->root_y - starty;

		  do_move:
		    ClientSnap(client);

		    // only if moved first time
		    if (!do_move) {
			if (abs(client->X - oldx) > CLIENT_MOVE_DELTA
			    || abs(client->Y - oldy) > CLIENT_MOVE_DELTA) {

			    if (client->State & WM_STATE_MAXIMIZED_HORZ) {
				hmax = 1;
			    }
			    if (client->State & WM_STATE_MAXIMIZED_VERT) {
				vmax = 1;
			    }
			    if (hmax || vmax) {
				ClientMaximize(client, 0, 0);
				startx = client->Width / 2;
				starty = -north / 2;
				PointerWrap(client->Parent, startx, starty);
			    }

			    StatusCreateMove(client);
			    do_move = 1;
			} else {
			    break;
			}
		    }

		    if (ClientMoveMode == MOVE_OUTLINE) {
			OutlineClear();
			OutlineDraw(client->X - west, client->Y - north,
			    client->Width + west + east, height);
		    } else {
			uint32_t values[2];

			values[0] = client->X - west;
			values[1] = client->Y - north;
			xcb_configure_window(Connection, client->Parent,
			    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
			ClientSendConfigureEvent(client);
		    }
		    StatusUpdateMove(client);
		    PagerUpdate();

		    break;
		default:
		    EventHandleEvent(event);
		    break;
	    }
	    free(event);
	}
	WaitForEvent();
    }
    return 0;
}

/**
**	Move client window (keyboard or menu initiated).
**	Move client window using keyboard (mouse optional).
**
**	@param client	client to move
**
**	@returns 1 if client moved, 0 otherwise.
*/
int ClientMoveKeyboard(Client * client)
{
#if 0
    //
    //	FIXME: Wrapping must keep cursor inside window
    //
    xcb_motion_notify_event_t *event;
    int north;
    int south;
    int east;
    int west;

    Debug(3, "%s: client %p\n", __FUNCTION__, client);

    // resize allowed?
    if (!(client->Border & BORDER_MOVE)) {
	return 0;
    }
    // move pointer to upper-left corner of window
    PointerWrap(RootWindow, client->X, client->Y);
    event = NULL;
    DiscardMotionEvents(&event, client->Window);

    BorderGetSize(client, &north, &south, &east, &west);

    return ClientMoveLoop(client, 0, client->Width + east,
	client->Height + north);
#else
    int pointer_x;
    int pointer_y;

    PointerGetPosition(&pointer_x, &pointer_y);
    return ClientMoveLoop(client, 0, pointer_x - client->X,
	pointer_y - client->Y);
#endif
}

/// @}

// ------------------------------------------------------------------------ //
// Client resize
// ------------------------------------------------------------------------ //

///
///	@ingroup client
///	@defgroup resize The client window resize module.
///
///	This module contains the client window resize functions.
///
/// @{

    /// typedef of enumeration of possible resize modes
typedef enum
{
    RESIZE_OPAQUE,			///< show window contents while resizing
    RESIZE_OUTLINE			///< show an outline while resizing
} ResizeMode;

    /// client window resize mode
static ResizeMode ClientResizeMode = RESIZE_OPAQUE;

/**
**	Stop resize action.
**
**	@param client	client resized
*/
static void ClientStopResize(Client * client)
{
    int north;
    int south;
    int east;
    int west;
    uint32_t values[4];

    client->Controller();
    client->Controller = ClientDefaultController;

    ClientUpdateShape(client);

    BorderGetSize(client, &north, &south, &east, &west);

    values[0] = client->X - west;
    values[1] = client->Y - north;
    values[2] = client->Width + east + west;
    if (client->State & WM_STATE_SHADED) {
	values[3] = north + south;
    } else {
	values[3] = client->Height + north + south;
    }
    xcb_configure_window(Connection, client->Parent,
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
	XCB_CONFIG_WINDOW_HEIGHT, values);

    values[0] = west;
    values[1] = north;
    values[2] = client->Width;
    values[3] = client->Height;
    xcb_configure_window(Connection, client->Window,
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
	XCB_CONFIG_WINDOW_HEIGHT, values);
    ClientSendConfigureEvent(client);
}

/**
**	Callback for stopping resizes.
*/
static void ClientResizeController(void)
{
    xcb_ungrab_keyboard(Connection, XCB_CURRENT_TIME);
    xcb_ungrab_pointer(Connection, XCB_CURRENT_TIME);

    if (ClientResizeMode == RESIZE_OUTLINE) {
	OutlineClear();
    }
    StatusDestroyResize();

    ClientFinishAction = 1;
}

/**
**	Fix width to match aspect ratio.
**
**	@param client	client to be fixed
*/
static void ClientFixWidth(Client * client)
{
    int_fast32_t ratio;
    int_fast32_t goal;

    if ((client->SizeHints.flags & XCB_SIZE_HINT_P_ASPECT)
	&& client->Height > 0) {

	ratio = (65536 * client->Width) / client->Height;

	goal =
	    (65536 * client->SizeHints.min_aspect_num) /
	    client->SizeHints.min_aspect_den;
	if (ratio < goal) {
	    client->Width = (client->Height * goal) / 65536;
	}

	goal =
	    (65536 * client->SizeHints.max_aspect_num) /
	    client->SizeHints.max_aspect_den;
	if (ratio > goal) {
	    client->Width = (client->Height * goal) / 65536;
	}
    }
}

/**
**	Fix height to match aspect ratio.
**
**	@param client	client to be fixed
*/
static void ClientFixHeight(Client * client)
{
    int_fast32_t ratio;
    int_fast32_t goal;

    if ((client->SizeHints.flags & XCB_SIZE_HINT_P_ASPECT)
	&& client->Height > 0) {

	ratio = (65536 * client->Width) / client->Height;

	goal =
	    (65536 * client->SizeHints.min_aspect_num) /
	    client->SizeHints.min_aspect_den;
	if (ratio < goal) {
	    client->Height = (65536 * client->Width) / goal;
	}

	goal =
	    (65536 * client->SizeHints.max_aspect_num) /
	    client->SizeHints.max_aspect_den;
	if (ratio > goal) {
	    client->Height = (65536 * client->Width) / goal;
	}
    }
}

/**
**	Interactive resize client window.
**
**	@param client	client to resize
**	@param button	button initiating resize (0 keyboard/menu)
**	@param action	what border(s) to resize
**	@param startx	starting mouse x-coordinate (window relative)
**	@param starty	starting mouse y-coordinate (window relative)
*/
void ClientResizeLoop(Client * client, int button, int action, int startx,
    int starty)
{
    xcb_grab_pointer_cookie_t gp_cookie;
    xcb_grab_keyboard_cookie_t gk_cookie;
    int oldx;
    int oldy;
    int oldw;
    int oldh;
    int north;
    int south;
    int east;
    int west;
    int width;
    int height;
    int lastwidth;
    int lastheight;
    int delta_x;
    int delta_y;

    // resize allowed?
    if (!(client->Border & BORDER_RESIZE)) {
	return;
    }

    gp_cookie = PointerGrabForResizeRequest(action);
    NO_WARNING(gk_cookie);
    if (!button) {
	gk_cookie = KeyboardGrabRequest(client->Window);
    }

    if (client->State & WM_STATE_SHADED) {
	action &= ~(BORDER_ACTION_RESIZE_N | BORDER_ACTION_RESIZE_S);
    }

    ClientFinishAction = 0;
    client->Controller = ClientResizeController;

    oldx = client->X;
    oldy = client->Y;
    oldw = client->Width;
    oldh = client->Height;

    BorderGetSize(client, &north, &south, &east, &west);

    startx += client->X - west;
    starty += client->Y - north;

    width = (client->Width - client->SizeHints.base_width)
	/ client->SizeHints.width_inc;
    height = (client->Height - client->SizeHints.base_height)
	/ client->SizeHints.height_inc;

    StatusCreateResize(client);
    StatusUpdateResize(client, width, height);

    PointerGrabReply(gp_cookie);
    // FIXME: what if grab failed?

    if (button) {
	// FIXME; must convert button into mask
	if (!(PointerGetButtonMask() & (XCB_BUTTON_MASK_1 |
		    XCB_BUTTON_MASK_3))) {
	    Debug(3, "only border clicked, leave early\n");
	    ClientStopResize(client);
	    return;
	}
    } else {
	if (!KeyboardGrabReply(gk_cookie)) {
	    ClientStopResize(client);
	    return;
	}
    }

    for (;;) {
	for (;;) {
	    xcb_generic_event_t *event;

	    if (ClientFinishAction || !KeepLooping) {
		client->Controller = ClientDefaultController;
		// PointerSetDefaultCursor(client->Parent);
		return;
	    }
	    if (!(event = PollNextEvent())) {
		break;
	    }

	    switch (XCB_EVENT_RESPONSE_TYPE(event)) {

		case XCB_KEY_RELEASE:
		    break;
		case XCB_KEY_PRESS:
		    delta_x = 0;
		    delta_y = 0;
		    switch (KeyboardGet(((xcb_key_press_event_t *)
				event)->detail, ((xcb_key_press_event_t *)
				event)->state)) {
			case XK_Left:
			    delta_x = MIN(-client->SizeHints.width_inc, -10);
			    break;
			case XK_Right:
			    delta_x = MAX(client->SizeHints.width_inc, 10);
			    break;
			case XK_Up:
			    delta_y = MIN(-client->SizeHints.height_inc, -10);
			    break;
			case XK_Down:
			    delta_y = MAX(client->SizeHints.height_inc, 10);
			    break;
			case XK_Home:
			    delta_x = MIN(-client->SizeHints.width_inc, -10);
			    delta_y = MIN(-client->SizeHints.height_inc, -10);
			    break;
			case XK_End:
			    delta_x = MIN(-client->SizeHints.width_inc, -10);
			    delta_y = MAX(client->SizeHints.height_inc, 10);
			    break;
			case XK_Page_Up:
			    delta_x = MAX(client->SizeHints.width_inc, 10);
			    delta_y = MIN(-client->SizeHints.height_inc, -10);
			    break;
			case XK_Page_Down:
			    delta_x = MAX(client->SizeHints.width_inc, 10);
			    delta_y = MAX(client->SizeHints.height_inc, 10);
			    break;

			case XK_Shift_L:
			case XK_Shift_R:
			case XK_Control_L:
			case XK_Control_R:
			    break;
			case XK_Escape:
			case XK_Return:
			default:
			    ClientStopResize(client);
			    free(event);
			    return;
		    }
		    if (((xcb_key_press_event_t *) event)->state &
			XCB_MOD_MASK_CONTROL) {
			delta_x /= 10;
			delta_y /= 10;
		    }
		    if (((xcb_key_press_event_t *) event)->state &
			XCB_MOD_MASK_SHIFT) {
			delta_x *= 2;
			delta_y *= 2;
		    }
		    Debug(3, "move %dx%d\n", delta_x, delta_y);
		    delta_x += client->Width - oldw;
		    delta_y += client->Height - oldh;
		    Debug(3, "move %dx%d\n", delta_x, delta_y);
		    goto do_resize;
		case XCB_BUTTON_RELEASE:
		    if (((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_1
			|| ((xcb_button_press_event_t *) event)->detail ==
			XCB_BUTTON_INDEX_3) {
			ClientStopResize(client);
			free(event);
			return;
		    }
		case XCB_BUTTON_PRESS:
		    break;
		case XCB_MOTION_NOTIFY:
		    DiscardMotionEvents((xcb_motion_notify_event_t **) & event,
			client->Window);

		    delta_x =
			((xcb_motion_notify_event_t *) event)->root_x - startx;
		    delta_y =
			((xcb_motion_notify_event_t *) event)->root_y - starty;

		  do_resize:
		    delta_y /= client->SizeHints.height_inc;
		    delta_y *= client->SizeHints.height_inc;
		    if (action & BORDER_ACTION_RESIZE_N) {
			// limits
			if (oldh - delta_y >= client->SizeHints.min_height
			    && (oldh - delta_y <= client->SizeHints.max_height
				|| delta_y > 0)) {
			    client->Height = oldh - delta_y;
			    client->Y = oldy + delta_y;
			}
			if (!(action & (BORDER_ACTION_RESIZE_E |
				    BORDER_ACTION_RESIZE_W))) {
			    ClientFixWidth(client);
			}
		    } else if ((action & BORDER_ACTION_RESIZE_S)) {
			delta_y += oldh;
			delta_y = MAX(delta_y, client->SizeHints.min_height);
			delta_y = MIN(delta_y, client->SizeHints.max_height);
			client->Height = delta_y;
			if (!(action & (BORDER_ACTION_RESIZE_E |
				    BORDER_ACTION_RESIZE_W))) {
			    ClientFixWidth(client);
			}
		    }

		    delta_x /= client->SizeHints.width_inc;
		    delta_x *= client->SizeHints.width_inc;
		    if ((action & BORDER_ACTION_RESIZE_E)) {
			delta_x += oldw;
			delta_x = MAX(delta_x, client->SizeHints.min_width);
			delta_x = MIN(delta_x, client->SizeHints.max_width);
			client->Width = delta_x;
			if (!(action & (BORDER_ACTION_RESIZE_N |
				    BORDER_ACTION_RESIZE_S))) {
			    ClientFixHeight(client);
			}
		    } else if (action & BORDER_ACTION_RESIZE_W) {
			if (oldw - delta_x >= client->SizeHints.min_width
			    && (oldw - delta_x <= client->SizeHints.max_width
				|| delta_x > 0)) {
			    client->Width = oldw - delta_x;
			    client->X = oldx + delta_x;
			}
			if (!(action & (BORDER_ACTION_RESIZE_N |
				    BORDER_ACTION_RESIZE_S))) {
			    ClientFixHeight(client);
			}
		    }

		    if ((client->SizeHints.flags & XCB_SIZE_HINT_P_ASPECT)
			&& (action & (BORDER_ACTION_RESIZE_N |
				BORDER_ACTION_RESIZE_S))
			&& (action & (BORDER_ACTION_RESIZE_E |
				BORDER_ACTION_RESIZE_W))) {
			int_fast32_t ratio;
			int_fast32_t goal;

			ratio = (65536 * client->Width) / client->Height;

			goal =
			    (65536 * client->SizeHints.min_aspect_num) /
			    client->SizeHints.min_aspect_den;
			if (ratio < goal) {
			    delta_x = client->Width;
			    client->Width = (client->Height * goal) / 65536;
			    if (action & BORDER_ACTION_RESIZE_W) {
				client->X -= client->Width - delta_x;
			    }
			}

			goal =
			    (65536 * client->SizeHints.max_aspect_num) /
			    client->SizeHints.max_aspect_den;
			if (ratio > goal) {
			    delta_y = client->Height;
			    client->Height = (65536 * client->Width) / goal;
			    if (action & BORDER_ACTION_RESIZE_N) {
				client->Y -= client->Height - delta_y;
			    }
			}
		    }

		    lastwidth = width;
		    lastheight = height;

		    width =
			(client->Width -
			client->SizeHints.base_width) /
			client->SizeHints.width_inc;
		    height =
			(client->Height -
			client->SizeHints.base_height) /
			client->SizeHints.height_inc;

		    if (lastheight != height || lastwidth != width) {
			if (client->State & (WM_STATE_MAXIMIZED_HORZ |
				WM_STATE_MAXIMIZED_VERT)) {
			    client->State &=
				~(WM_STATE_MAXIMIZED_HORZ |
				WM_STATE_MAXIMIZED_VERT);
			    // update hints to respect the state change
			    HintSetAllStates(client);
			    ClientSendConfigureEvent(client);
			}

			StatusUpdateResize(client, width, height);

			if (ClientResizeMode == RESIZE_OUTLINE) {
			    OutlineClear();
			    if (client->State & WM_STATE_SHADED) {
				OutlineDraw(client->X - west,
				    client->Y - north,
				    client->Width + west + east,
				    north + south);
			    } else {
				OutlineDraw(client->X - west,
				    client->Y - north,
				    client->Width + west + east,
				    client->Height + north + south);
			    }
			} else {
			    uint32_t values[4];

			    ClientUpdateShape(client);

			    values[0] = client->X - west;
			    values[1] = client->Y - north;
			    values[2] = client->Width + east + west;
			    if (client->State & WM_STATE_SHADED) {
				values[3] = north + south;
			    } else {
				values[3] = client->Height + north + south;
			    }
			    xcb_configure_window(Connection, client->Parent,
				XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
				XCB_CONFIG_WINDOW_WIDTH |
				XCB_CONFIG_WINDOW_HEIGHT, values);

			    values[0] = west;
			    values[1] = north;
			    values[2] = client->Width;
			    values[3] = client->Height;
			    xcb_configure_window(Connection, client->Window,
				XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
				XCB_CONFIG_WINDOW_WIDTH |
				XCB_CONFIG_WINDOW_HEIGHT, values);
			    ClientSendConfigureEvent(client);
			}

			PagerUpdate();
		    }
		    break;
		default:
		    EventHandleEvent(event);
		    break;
	    }
	    free(event);
	}
	WaitForEvent();
    }
}

/**
**	Resize client window (keyboard or menu initiated).
**	Resize client window using keyboard (mouse optional).
**
**	@param client	client to resize
*/
void ClientResizeKeyboard(Client * client)
{
    xcb_motion_notify_event_t *event;
    int north;
    int south;
    int east;
    int west;

    // resize allowed?
    if (!(client->Border & BORDER_RESIZE)) {
	return;
    }
    // move pointer to lower-right corner of window
    PointerWrap(RootWindow, client->X + client->Width,
	client->Y + client->Height);
    event = NULL;
    DiscardMotionEvents(&event, client->Window);

    BorderGetSize(client, &north, &south, &east, &west);

    ClientResizeLoop(client, 0,
	BORDER_ACTION_RESIZE | BORDER_ACTION_RESIZE_E | BORDER_ACTION_RESIZE_S,
	client->Width + east, client->Height + north);
}

/// @}
