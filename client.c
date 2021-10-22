///
///	@file client.c		@brief client functions
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

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_aux.h>
#ifdef USE_SHAPE
#include <xcb/shape.h>
#endif

#include "queue.h"
#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "pointer.h"
#include "client.h"
#include "border.h"
#include "keyboard.h"
#include "hints.h"
#include "draw.h"
#include "screen.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "rule.h"
#include "panel.h"
#include "plugin/pager.h"
#include "plugin/swallow.h"
#include "plugin/task.h"
#include "desktop.h"

// ------------------------------------------------------------------------ //
// Placement
// ------------------------------------------------------------------------ //

///
///	@ingroup client
///	@defgroup placement The placement module.
///
///	This module contains the client placement functions.
///
///	@note if we use pixman region code for render, we can use it also
///	for struts.
///
///	Support for EWMH _NET_WM_STRUT_PARTIAL and _NET_WM_STRUT.
///
///< @{

/**
**	typedef for rectangle.
*/
typedef struct _rectangle_ Rectangle;

/**
**	structure for rectangle.
**
**	Simple screen rectangle.
**
**	@note Width/Height can become negative.
*/
struct _rectangle_
{
    int16_t X;				///< x-coordinate of rectangle
    int16_t Y;				///< y-coordinate of rectangle
    int32_t Width;			///< width of rectangle
    int32_t Height;			///< height of rectangle
};

/**
**	typedef for strut.
*/
typedef struct _strut_ Strut;

/**
**	Structure for strut.
**
**	@note there can be max four strut / client.
*/
struct _strut_
{
    LIST_ENTRY(_strut_) Node;		///< in double linked list of strut

    const Client *Client;		///< client owning this strut
    Rectangle Rectangle;		///< bounding rectangle
};

    /// structure for strut head
LIST_HEAD(_strut_head_, _strut_);

    /// struts of all clients
static struct _strut_head_ Struts = LIST_HEAD_INITIALIZER(&Struts);

    /**
    **	Auto window placement offsets
    **
    **	#DesktopN x #ScreenN
    **
    **	@note we assume x and y are 0 based for all screens here.
    */
static int *CascadeOffsets;

/**
**	Determine which way to move client for border.
**
**	@param client	client
**	@param[out] x	pointer to store x delta
**	@param[out] y	pointer to store y delta
*/
static void ClientGetGravityDelta(const Client * client, int *x, int *y)
{
    int north;
    int south;
    int east;
    int west;

    BorderGetSize(client, &north, &south, &east, &west);

    switch (client->SizeHints.win_gravity) {
	case XCB_GRAVITY_NORTH:
	    *x = 0;
	    *y = -north;
	    break;
	case XCB_GRAVITY_NORTH_WEST:
	    *x = -west;
	    *y = -north;
	    break;
	case XCB_GRAVITY_NORTH_EAST:
	    *x = west;
	    *y = -north;
	    break;
	case XCB_GRAVITY_WEST:
	    *x = -west;
	    *y = 0;
	    break;
	case XCB_GRAVITY_EAST:
	    *x = west;
	    *y = 0;
	    break;
	case XCB_GRAVITY_CENTER:
	    *x = (east + west) / 2;
	    *y = (north + south) / 2;
	    break;
	case XCB_GRAVITY_SOUTH:
	    *x = 0;
	    *y = south;
	    break;
	case XCB_GRAVITY_SOUTH_WEST:
	    *x = -west;
	    *y = south;
	    break;
	case XCB_GRAVITY_SOUTH_EAST:
	    *x = west;
	    *y = south;
	    break;
	default:			// static
	    *x = 0;
	    *y = 0;
	    break;
    }
}

/**
**	Move window in specified direction for reparenting.
**
**	@param client	client to be moved
**	@param negate	false to gravitate for border,
**			true to gravitate for no border
*/
void ClientGravitate(Client * client, int negate)
{
    int delta_x;
    int delta_y;

    ClientGetGravityDelta(client, &delta_x, &delta_y);

    if (negate) {
	client->X += delta_x;
	client->Y += delta_y;
    } else {
	client->X -= delta_x;
	client->Y -= delta_y;
    }
}

/**
**	Get screen bounds.
**
**	@param screen		screen whose bounds to get
**	@param rectangle	bounding rectangle for screen
*/
static void GetScreenBounds(const Screen * screen, Rectangle * rectangle)
{
    rectangle->X = screen->X;
    rectangle->Y = screen->Y;
    rectangle->Width = screen->Width;
    rectangle->Height = screen->Height;
}

/**
**	Subtract rectangle from bounding rectangle.
**
**	@param sub		substract rectangle
**	@param[in,out] dst	base rectangle
*/
static void ClientSubtractBounds(const Rectangle * sub, Rectangle * dst)
{
    Rectangle rectangles[4];

    if ((sub->X + sub->Width <= dst->X)
	|| (sub->Y + sub->Height <= dst->Y)
	|| (dst->X + dst->Width <= sub->X)
	|| (dst->Y + dst->Height <= sub->Y)) {
	return;				// no intersection
    }
    //	There are four ways to do this:
    //	0. Increase x-coordinate and decrease width of dst
    //	1. Increase y-coordinate and decrease height of dst
    //	2. Decrease width of dst
    //	3. Decrease height of dst
    //
    //	We will chose option, which leaves greatest area
    //	Note that negative areas are possible

    // 0
    rectangles[0] = *dst;
    rectangles[0].X = sub->X + sub->Width;
    rectangles[0].Width = dst->X + dst->Width - rectangles[0].X;

    // 1
    rectangles[1] = *dst;
    rectangles[1].Y = sub->Y + sub->Height;
    rectangles[1].Height = dst->Y + dst->Height - rectangles[1].Y;

    // 2
    rectangles[2] = *dst;
    rectangles[2].Width = sub->X - dst->X;

    // 3
    rectangles[3] = *dst;
    rectangles[3].Height = sub->Y - dst->Y;

    // 0 and 1, winner in 0
    if (rectangles[0].Width * rectangles[0].Height <
	rectangles[1].Width * rectangles[1].Height) {
	rectangles[0] = rectangles[1];
    }
    // 2 and 3, winner in 2
    if (rectangles[2].Width * rectangles[2].Height <
	rectangles[3].Width * rectangles[3].Height) {
	rectangles[2] = rectangles[3];
    }
    // 0 and 2, winner in dst
    if (rectangles[0].Width * rectangles[0].Height <
	rectangles[2].Width * rectangles[2].Height) {
	*dst = rectangles[2];
    } else {
	*dst = rectangles[0];
    }
}

/**
**	Subtract panel area from bounding rectangle.
**
**	@param rectangle	bounding rectangle
**	@param layer		maximum layer of panel bounds
*/
static void ClientSubtractPanelBounds(Rectangle * rectangle, int layer)
{
    const Panel *panel;

    SLIST_FOREACH(panel, &Panels, Next) {
	if (panel->OnLayer > layer && !panel->AutoHide && !panel->MaximizeOver) {
	    Rectangle sub;
	    Rectangle last;

	    sub.X = panel->X;
	    sub.Y = panel->Y;
	    sub.Width = panel->Width;
	    sub.Height = panel->Height;

	    last = *rectangle;
	    ClientSubtractBounds(&sub, rectangle);
	    if (rectangle->Width * rectangle->Height <= 0) {
		*rectangle = last;
		break;			// don't allow empty rectangle
	    }
	}
    }
}

/**
**	Remove struts from bounding rectangle.
**
**	@param[in,out] rectangle	read/write rectangle of bounds
*/
void ClientSubtractStrutBounds(Rectangle * rectangle)
{
    Strut *strut;
    Rectangle last;

    LIST_FOREACH(strut, &Struts, Node) {
	if (strut->Client->Desktop == DesktopCurrent
	    || strut->Client->State & WM_STATE_STICKY) {
	    last = *rectangle;
	    ClientSubtractBounds(&strut->Rectangle, rectangle);
	    if (rectangle->Width * rectangle->Height <= 0) {
		*rectangle = last;
		break;			// don't allow empty rectangle
	    }
	}
    }
}

/**
**	Remove clients from bounding rectangle.
**
**	@param new_client	new client to be placed
**	@param[in,out] rectangle	read/write rectangle of bounds
*/
void ClientSubtractClientBounds(const Client * new_client,
    Rectangle * rectangle)
{
    const Client *client;
    int layer;

    // FIXME: should i remove clients: only above or on same layer or all?
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    Rectangle sub;
	    Rectangle last;
	    int north;
	    int south;
	    int east;
	    int west;

	    // ignore self
	    if (client == new_client) {
		continue;
	    }
	    // ignore clients on other desktops
	    if (client->Desktop != DesktopCurrent
		&& !(client->State & WM_STATE_STICKY)) {
		continue;
	    }
	    // ignore hidden clients
	    if (!(client->State & WM_STATE_MAPPED)) {
		continue;
	    }

	    BorderGetSize(client, &north, &south, &east, &west);

	    sub.X = client->X - west;
	    sub.Y = client->Y - north;
	    sub.Width = client->Width + west + east;
	    sub.Height = client->Height + north + south;

	    last = *rectangle;
	    ClientSubtractBounds(&sub, rectangle);

	    if (rectangle->Width * rectangle->Height <= 0) {
		*rectangle = last;
		break;			// don't allow empty rectangle
	    }
	}
    }
}

/**
**	Remove struts associated with client.
**
**	@param client	client
*/
void ClientDelStrut(const Client * client)
{
    Strut *strut;
    Strut *temp;

    LIST_FOREACH_SAFE(strut, &Struts, Node, temp) {
	if (strut->Client == client) {
	    LIST_REMOVE(strut, Node);
	    free(strut);
	}
    }
}

/**
**	Add client specified struts to our list.
**
**	@param client	client
**
**	Use _NET_WM_STRUT_PARTIAL and _NET_WM_STRUT
**
**	@todo can split query/request.
*/
void ClientGetStrut(const Client * client)
{
    xcb_get_property_cookie_t cookie;
    xcb_get_property_reply_t *reply;
    uint32_t *data;
    Strut *strut;

    ClientDelStrut(client);		// remove any old strut

    cookie =
	xcb_get_property_unchecked(Connection, 0, client->Window,
	Atoms.NET_WM_STRUT_PARTIAL.Atom, XCB_ATOM_CARDINAL, 0, 12);

    // _NET_WM_STRUT_PARTIAL,
    //	left, right, top, bottom, left_start_y, left_end_y,
    //	right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x,
    //	bottom_end_x,CARDINAL[12]/32
    // Struts MUST be specified in root window coordinates.
    reply = xcb_get_property_reply(Connection, cookie, NULL);
    if (reply) {
	if (reply->value_len && (data = xcb_get_property_value(reply))) {
	    if (data[0] > 0) {		// left
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = 0;
		strut->Rectangle.Y = data[4];
		strut->Rectangle.Width = data[0];
		strut->Rectangle.Height = data[5] - data[4];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[1] > 0) {		// right
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = XcbScreen->width_in_pixels - data[1];
		strut->Rectangle.Y = data[4];
		strut->Rectangle.Width = data[1];
		strut->Rectangle.Height = data[5] - data[4];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[2] > 0) {		// top
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = data[8];
		strut->Rectangle.Y = 0;
		strut->Rectangle.Width = data[9] - data[8];
		strut->Rectangle.Height = data[2];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[3] > 0) {		// bottom
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = data[8];
		strut->Rectangle.Y = XcbScreen->height_in_pixels - data[3];
		strut->Rectangle.Width = data[9] - data[8];
		strut->Rectangle.Height = data[3];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }
	}
	free(reply);
	return;
    }

    cookie =
	xcb_get_property_unchecked(Connection, 0, client->Window,
	Atoms.NET_WM_STRUT.Atom, XCB_ATOM_CARDINAL, 0, 4);

    // _NET_WM_STRUT, left, right, top, bottom, CARDINAL[4]/32

    reply = xcb_get_property_reply(Connection, cookie, NULL);
    if (reply) {
	if (reply->value_len && (data = xcb_get_property_value(reply))) {
	    if (data[0] > 0) {		// left
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = 0;
		strut->Rectangle.Y = 0;
		strut->Rectangle.Width = data[0];
		strut->Rectangle.Height = XcbScreen->height_in_pixels;
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[1] > 0) {		// right
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = XcbScreen->width_in_pixels - data[1];
		strut->Rectangle.Y = 0;
		strut->Rectangle.Width = data[1];
		strut->Rectangle.Height = XcbScreen->height_in_pixels;
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[2] > 0) {		// top
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = 0;
		strut->Rectangle.Y = 0;
		strut->Rectangle.Width = XcbScreen->width_in_pixels;
		strut->Rectangle.Height = data[2];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[3] > 0) {		// bottom
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = 0;
		strut->Rectangle.Y = XcbScreen->height_in_pixels - data[3];
		strut->Rectangle.Width = XcbScreen->width_in_pixels;
		strut->Rectangle.Height = data[3];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }
	}
	free(reply);
    }
}

/**
**	Place client on screen.
**
**	@param client		client to place
**	@param already_mapped	true if already mapped, false if unmapped
**
**	@todo automatic placement in free area isn't the best.
*/
void ClientPlace(Client * client, int already_mapped)
{
    int north;
    int south;
    int east;
    int west;
    int overflow;
    const Screen *screen;
    uint32_t values[2];
    int i;

    screen = ScreenGetPointer();
    // client bigger than screen
    if (client->X + client->Width > screen->Width
	|| client->Y + client->Height > screen->Height) {
	overflow = 1;
    } else {
	overflow = 0;
    }

    BorderGetSize(client, &north, &south, &east, &west);

    //
    //	client already placed and not outside of screen
    //	or client has specified position
    //
    if ((!overflow && already_mapped)
	|| (!(client->State & WM_STATE_PIGNORE)
	    && (client->SizeHints.
		flags & (XCB_ICCCM_SIZE_HINT_P_POSITION |
		    XCB_ICCCM_SIZE_HINT_US_POSITION)))) {
	ClientGravitate(client, 0);
    } else {
	Rectangle rectangle;
	Rectangle area;

	GetScreenBounds(screen, &rectangle);
	ClientSubtractPanelBounds(&rectangle, client->OnLayer);
	ClientSubtractStrutBounds(&rectangle);

	area = rectangle;
	ClientSubtractClientBounds(client, &area);

	//
	//	try to place windows in free area
	//	FIXME: make window placement configurable
	//
	i = (screen - Screens) * DesktopN + DesktopCurrent;
	if (client->Width + west + east < area.Width
	    && client->Height + north + south < area.Height) {
	    client->X = area.X + west;
	    client->Y = area.Y + north;
	} else {

	    // set cascaded location
	    client->X = rectangle.X + west + CascadeOffsets[i];
	    client->Y = rectangle.Y + north + CascadeOffsets[i];
	    CascadeOffsets[i] += BorderGetTitleSize();
	}

	// check for cascade overflow
	overflow = 0;
	if (client->X + client->Width - rectangle.X > rectangle.Width) {
	    overflow = 1;
	} else if (client->Y + client->Height - rectangle.Y > rectangle.Height) {
	    overflow = 1;
	}

	if (overflow) {
	    CascadeOffsets[i] = BorderGetTitleSize();
	    client->X = rectangle.X + west + CascadeOffsets[i];
	    client->Y = rectangle.Y + north + CascadeOffsets[i];

	    // check for client overflow
	    overflow = 0;
	    if (client->X + client->Width - rectangle.X > rectangle.Width) {
		overflow = 1;
	    } else if (client->Y + client->Height - rectangle.Y >
		rectangle.Height) {
		overflow = 1;
	    }
	    // update cascade position or position client
	    if (overflow) {
		client->X = rectangle.X + west;
		client->Y = rectangle.Y + north;
	    } else {
		CascadeOffsets[i] += BorderGetTitleSize();
	    }

	}
    }

    // move parent to correct place
    if (client->State & WM_STATE_FULLSCREEN) {
	values[0] = screen->X;
	values[1] = screen->Y;
    } else {
	values[0] = client->X - west;
	values[1] = client->Y - north;
    }
    Debug(3, "%s: final %+d%+d\n", __FUNCTION__, values[0], values[1]);

    // FIXME: fullscreen have no parent window mapped! check if this is needed
    xcb_configure_window(Connection, client->Parent,
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
}

/**
**	Place tiled client on screen.
**
**	@param client	client to place
**	@param type	type of tile
*/
void ClientPlaceTiled(Client * client, int type)
{
    int north;
    int south;
    int east;
    int west;
    int t;
    const Screen *screen;
    Rectangle rectangle;

    // save old position and size for restore
    client->OldX = client->X;
    client->OldY = client->Y;
    client->OldWidth = client->Width;
    client->OldHeight = client->Height;

    BorderGetSize(client, &north, &south, &east, &west);

    screen =
	ScreenGetByXY(client->X + (east + west + client->Width) / 2,
	client->Y + (north + south + client->Height) / 2);
    GetScreenBounds(screen, &rectangle);

    ClientSubtractPanelBounds(&rectangle, client->OnLayer);
    ClientSubtractStrutBounds(&rectangle);

    rectangle.X += west;
    rectangle.Y += north;
    rectangle.Width -= east + west;
    rectangle.Height -= north + south;

    // check for size-hints FIXME: combine with below
    if (rectangle.Width > client->SizeHints.max_width) {
	rectangle.Width = client->SizeHints.max_width;
    }
    if (rectangle.Height > client->SizeHints.max_height) {
	rectangle.Height = client->SizeHints.max_height;
    }

    if (client->SizeHints.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) {
	double ratio;
	double minr;
	double maxr;

	// FIXME: remove double
	ratio = (double)rectangle.Width / rectangle.Height;

	minr =
	    (double)client->SizeHints.min_aspect_num /
	    client->SizeHints.min_aspect_den;
	if (ratio < minr) {
	    rectangle.Height = (double)rectangle.Width / minr;
	}

	maxr =
	    (double)client->SizeHints.max_aspect_num /
	    client->SizeHints.max_aspect_den;
	if (ratio > maxr) {
	    rectangle.Width = (double)rectangle.Height * maxr;
	}

    }
    // YYHHXXWW
    // if maximizing horizontally, update width
    // 00: -- 01: 100% 10: 50% 11: 33%
    if (type & 3) {
	t = rectangle.Width / (type & 3);
	client->Width = t - (t % client->SizeHints.width_inc);
	client->State |= WM_STATE_MAXIMIZED_HORZ;
    }
    type >>= 2;

    // 00: -- 01: right 10: left: 11: center
    switch (type & 3) {
	case 0:
	    break;
	case 1:
	    client->X = rectangle.X;
	    break;
	case 2:
	    client->X = rectangle.X + rectangle.Width - client->Width;
	    break;
	case 3:
	    client->X = rectangle.X + rectangle.Width / 2 - client->Width / 2;
	    break;
    }
    type >>= 2;

    // if maximizing vertically, update height
    // 00: -- 01: 100% 10: 50% 11: 33%
    if (type & 3) {
	t = rectangle.Height / (type & 3);
	client->Height = t - (t % client->SizeHints.height_inc);
	client->State |= WM_STATE_MAXIMIZED_VERT;
    }
    type >>= 2;

    switch (type & 3) {
	case 0:
	    break;
	case 1:
	    client->Y = rectangle.Y;
	    break;
	case 2:
	    client->Y = rectangle.Y + rectangle.Height - client->Height;
	    break;
	case 3:
	    client->Y =
		rectangle.Y + rectangle.Height / 2 - client->Height / 2;
	    break;
    }
}

/**
**	Constrain size of client to available screen space.
**
**	@param client	client
*/
void ClientConstrainSize(Client * client)
{
    int north;
    int south;
    int east;
    int west;
    const Screen *screen;
    Rectangle rectangle;

    // determine if size needs to be constrained
    screen = ScreenGetByXY(client->X, client->Y);
    if (client->Width < screen->Width && client->Height < screen->Height) {
	return;
    }
    // constrain size
    BorderGetSize(client, &north, &south, &east, &west);

    GetScreenBounds(screen, &rectangle);
    ClientSubtractPanelBounds(&rectangle, client->OnLayer);
    ClientSubtractStrutBounds(&rectangle);

    rectangle.X += west;
    rectangle.Y += north;
    rectangle.Width -= east + west;
    rectangle.Height -= north + south;

    // check for size-hints FIXME: combine with above
    if (rectangle.Width > client->SizeHints.max_width) {
	rectangle.Width = client->SizeHints.max_width;
    }
    if (rectangle.Height > client->SizeHints.max_height) {
	rectangle.Height = client->SizeHints.max_height;
    }

    if (client->SizeHints.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) {
	double ratio;
	double minr;
	double maxr;

	// FIXME: remove double
	ratio = (double)rectangle.Width / rectangle.Height;

	minr =
	    (double)client->SizeHints.min_aspect_num /
	    client->SizeHints.min_aspect_den;
	if (ratio < minr) {
	    rectangle.Height = (double)rectangle.Width / minr;
	}

	maxr =
	    (double)client->SizeHints.max_aspect_num /
	    client->SizeHints.max_aspect_den;
	if (ratio > maxr) {
	    rectangle.Width = (double)rectangle.Height * maxr;
	}

    }

    Debug(2, "constrain %dx%d%+d%+d -> %dx%d%+d%+d\n", client->Width,
	client->Height, client->X, client->Y, rectangle.Width,
	rectangle.Height, rectangle.X, rectangle.Y);

    client->X = rectangle.X;
    client->Y = rectangle.Y;
    client->Width =
	rectangle.Width - (rectangle.Width % client->SizeHints.width_inc);
    client->Height =
	rectangle.Height - (rectangle.Height % client->SizeHints.height_inc);
}

/**
**	Initialize placement module.
*/
void PlacementInit(void)
{
    CascadeOffsets = calloc(DesktopN * ScreenN, sizeof(*CascadeOffsets));
}

/**
**	Exit placement module.
*/
void PlacementExit(void)
{
    Strut *strut;

    strut = LIST_FIRST(&Struts);
    while (strut) {			// faster list deletion
	Strut *temp;

	temp = LIST_NEXT(strut, Node);
	free(strut);
	strut = temp;
    }
    LIST_INIT(&Struts);

    free(CascadeOffsets);
    CascadeOffsets = NULL;
}

/// @}

// ------------------------------------------------------------------------ //
// Client
// ------------------------------------------------------------------------ //

///
///	@defgroup client The client module.
///
///	This module contains client control functions.
///
///< @{

FocusModel FocusModus;			///< current focus model

// *INDENT-OFF*	queue macros break indent
    /// singly-linked List of all clients for _NET_CLIENT_LIST and task list
ClientNetListHead ClientNetList;
    /// double linked list to find client by child = window
static LIST_HEAD(_client_child_, _client_) ClientByChild;
    /// double linked list to find client by frame = parent
static LIST_HEAD(_client_frame_, _client_) ClientByFrame;
// *INDENT-ON*

int ClientN;				///< number of clients managed

    /// table of double linked tail queues of all clients in layer
ClientLayerHead ClientLayers[LAYER_MAX];

void (*ClientController)(void);		///< callback to stop move/resize
Client *ClientControlled;		///< current controlled client

    /// pre-init -> init cookie
static xcb_query_tree_cookie_t QueryTreeCookie;

static uint32_t ClientTopmostOpacity;	///< of topmost client
static uint32_t ClientMaxStackingOpacity;	///< max of stacking client
static uint32_t ClientMinStackingOpacity;	///< min of stacking client
static uint32_t ClientStackingStepOpacity;	///< step between stacking

static Client *ClientActive;		///< current active client window

/**
**	Determine if client is allowed focus.
**
**	@param client	client
**
**	@returns 1 if focus is allowed, 0 otherwise.
*/
int ClientShouldFocus(const Client * client)
{
    if ((client->Desktop != DesktopCurrent
	    && !(client->State & WM_STATE_STICKY))
	|| (client->State & WM_STATE_NOLIST) || (client->Owner != XCB_NONE)) {
	return 0;
    }

    return 1;
}

/**
**	Send client message to window.
**
**	@param window	client window
**	@param type	type of message to send
**	@param message	message to send
*/
void ClientSendMessage(xcb_window_t window, xcb_atom_t type,
    xcb_atom_t message)
{
    xcb_client_message_event_t event;

    memset(&event, 0, sizeof(event));
    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.window = window;
    event.type = type;
    event.data.data32[0] = message;
    event.data.data32[1] = XCB_CURRENT_TIME;

    xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW, window,
	XCB_EVENT_MASK_NO_EVENT, (void *)&event);
}

/**
**	Send WM_DELETE_WINDOW message to window.
**
**	@param window	client window
*/
void ClientSendDeleteWindow(xcb_window_t window)
{
    ClientSendMessage(window, Atoms.WM_PROTOCOLS.Atom,
	Atoms.WM_DELETE_WINDOW.Atom);
}

/**
**	Send configure event to client window.
**	This will send updated location and size information to client.
**
**	@param client	client to get event
*/
void ClientSendConfigureEvent(const Client * client)
{
    xcb_configure_notify_event_t event;

    event.response_type = XCB_CONFIGURE_NOTIFY;
    event.event = client->Window;
    event.window = client->Window;

    if (client->State & WM_STATE_FULLSCREEN) {
	const Screen *screen;

	// fullscreen isn't stored in client
	screen = ScreenGetByXY(client->X, client->Y);
	event.x = screen->X;
	event.y = screen->Y;
	event.width = screen->Width;
	event.height = screen->Height;
    } else {
	event.x = client->X;
	event.y = client->Y;
	event.width = client->Width;
	event.height = client->Height;
    }

    event.border_width = 0;
    event.above_sibling = XCB_NONE;
    event.override_redirect = 0;

    xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW,
	client->Window, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (void *)&event);
}

#ifdef USE_COLORMAPS

/**
**	Update window's colormap.
**
**	Call to this function indicates that colormap(s) for given
**	client changed. This will change active colormap(s) if given
**	client is active.
**
**	@param client	client with changed colormaps
*/
static void ClientUpdateColormap(Client * client)
{
    Debug(2, "%s: FIXME: for %p\n", __FUNCTION__, client);
}
#else

///	Dummy: Update window's colormap.
#define ClientUpdateColormap(client)	/* client */

#endif

/**
**	Restack clients.
**
**	This is used when client is mapped so that stacking order
**	remains consistent.
*/
void ClientRestack(void)
{
    uint32_t values[2];
    int layer;
    Client *client;
    uint32_t opacity;
    uint32_t otmp;
    int is_topmost;
    Panel *panel;
    xcb_window_t window;

    // FIXME: restack with a single request?

    is_topmost = 1;
    opacity = ClientMaxStackingOpacity;
    values[0] = XCB_NONE;
    values[1] = XCB_STACK_MODE_BELOW;

    for (layer = LAYER_TOP; layer >= LAYER_BOTTOM; --layer) {
	// add all visible clients on this layer to window stack
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    if ((client->State & (WM_STATE_MAPPED | WM_STATE_SHADED))
		&& !(client->State & WM_STATE_HIDDEN)) {
		if (is_topmost) {
		    if (!(client->State & WM_STATE_OPACITY)
			&& client->Opacity != ClientTopmostOpacity) {
			client->Opacity = ClientTopmostOpacity;
			HintSetAllStates(client);
		    }
		    is_topmost = 0;
		} else if (!(client->State & WM_STATE_OPACITY)) {
		    if (client->Opacity != opacity) {
			client->Opacity = opacity;
			HintSetAllStates(client);
		    }
		    otmp = opacity - ClientStackingStepOpacity;
		    if (otmp < ClientMinStackingOpacity || otmp > opacity) {
			opacity = ClientMinStackingOpacity;
		    } else {
			opacity = otmp;
		    }
		}
		// fix fullscreen windows, which have no parent
		if (client->State & WM_STATE_FULLSCREEN) {
		    window = client->Window;
		} else {
		    window = client->Parent;
		}
		// XCB_NONE generates error
		if (values[0] != XCB_NONE) {
		    xcb_configure_window(Connection, window,
			XCB_CONFIG_WINDOW_SIBLING |
			XCB_CONFIG_WINDOW_STACK_MODE, values);
		}
		values[0] = window;
	    }
	}
	// add all panels on this layer to window stack
	SLIST_FOREACH(panel, &Panels, Next) {
	    if (layer == panel->OnLayer) {
		// XCB_NONE generates error
		if (values[0] != XCB_NONE) {
		    xcb_configure_window(Connection, panel->Window,
			XCB_CONFIG_WINDOW_SIBLING |
			XCB_CONFIG_WINDOW_STACK_MODE, values);
		}
		values[0] = panel->Window;
	    }
	}
    }

    // FIXME: only need to update _NET_CLIENT_LIST_STACKING
    HintSetNetClientList();
    PagerUpdate();
}

/**
**	Set keyboard focus to client.
**
**	@param client	client to focus
*/
void ClientFocus(Client * client)
{
    // hidden client can't get focus
    if (client->State & WM_STATE_HIDDEN) {
	return;
    }

    if (ClientActive != client) {
	if (ClientActive) {		// redraw old active client
	    ClientActive->State &= ~WM_STATE_ACTIVE;
	    BorderDraw(ClientActive, NULL);
	}
	client->State |= WM_STATE_ACTIVE;
	ClientActive = client;

	if (!(client->State & WM_STATE_SHADED)) {
	    ClientUpdateColormap(client);

	    AtomSetWindow(XcbScreen->root, &Atoms.NET_ACTIVE_WINDOW,
		client->Window);
	}

	BorderDraw(client, NULL);
	TaskUpdate();
	PagerUpdate();
    }

    if (client->State & WM_STATE_MAPPED) {
	xcb_set_input_focus(Connection, XCB_INPUT_FOCUS_POINTER_ROOT,
	    client->Window, XCB_CURRENT_TIME);
    } else {
	xcb_set_input_focus(Connection, XCB_INPUT_FOCUS_POINTER_ROOT,
	    XcbScreen->root, XCB_CURRENT_TIME);
    }
}

/**
**	Set keyboard focus back to active client.
*/
void ClientRefocus(void)
{
    if (ClientActive) {
	ClientFocus(ClientActive);
    }

}

/**
**	Focus next client in stacking order.
**
**	@param client	client before client to focus
*/
void ClientFocusNextStacked(const Client * client)
{
    int layer;
    Client *temp;

    // look on current layer
    for (temp = TAILQ_NEXT(client, LayerQueue); temp;
	temp = TAILQ_NEXT(temp, LayerQueue)) {
	if ((temp->State & (WM_STATE_MAPPED | WM_STATE_SHADED))
	    && !(temp->State & WM_STATE_HIDDEN)) {
	    ClientFocus(temp);
	    return;
	}
    }
    // look on layers below
    for (layer = client->OnLayer - 1; layer >= LAYER_BOTTOM; --layer) {
	TAILQ_FOREACH(temp, &ClientLayers[layer], LayerQueue) {
	    if ((temp->State & (WM_STATE_MAPPED | WM_STATE_SHADED))
		&& !(temp->State & WM_STATE_HIDDEN)) {
		ClientFocus(temp);
		return;
	    }
	}
    }
}

#ifdef USE_SHAPE			// {

/**
**	Update shape of client using shape extension.
**
**	@param client	client to update
*/
void ClientUpdateShape(const Client * client)
{
    int north;
    int south;
    int east;
    int west;
    xcb_rectangle_t rectangles[4];

    BorderGetSize(client, &north, &south, &east, &west);

    // shaded windows are special case
    if (client->State & WM_STATE_SHADED) {
	ShapeRoundedRectWindow(client->Parent, client->Width + west + east,
	    north);
	return;
    }

    if (client->State & WM_STATE_SHAPE) {
	// add shape of window
	xcb_shape_combine(Connection, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING,
	    XCB_SHAPE_SK_BOUNDING, client->Parent, west, north,
	    client->Window);
	// add shape of border
	if (north > 0) {
	    // top
	    rectangles[0].x = 0;
	    rectangles[0].y = 0;
	    rectangles[0].width = client->Width + east + west;
	    rectangles[0].height = north;

	    // left
	    rectangles[1].x = 0;
	    rectangles[1].y = 0;
	    rectangles[1].width = west;
	    rectangles[1].height = client->Height + north + south;

	    // right
	    rectangles[2].x = client->Width + east;
	    rectangles[2].y = 0;
	    rectangles[2].width = west;
	    rectangles[2].height = client->Height + north + south;

	    // bottom
	    rectangles[3].x = 0;
	    rectangles[3].y = client->Height + north;
	    rectangles[3].width = client->Width + east + west;
	    rectangles[3].height = south;

	    xcb_shape_rectangles(Connection, XCB_SHAPE_SO_UNION,
		XCB_SHAPE_SK_BOUNDING, XCB_CLIP_ORDERING_UNSORTED,
		client->Parent, 0, 0, 4, rectangles);
	}
	ShapeRoundedRectSubtract(client->Parent, client->Width + west + east,
	    client->Height + south + north);
    } else {
	ShapeRoundedRectWindow(client->Parent, client->Width + west + east,
	    client->Height + south + north);
    }
}

/**
**	Determine if window uses shape extension prefetch.
**
**	@param client	client to check for shape
**
**	@returns xcb request cookie
*/
static xcb_shape_query_extents_cookie_t ClientCheckShapeRequest(const Client *
    client)
{
    return xcb_shape_query_extents_unchecked(Connection, client->Window);
}

/**
**	Determine if window uses shape extension.
**
**	@param cookie	xcb cookie from @see ClientCheckShapeRequest
**	@param client	client to check for shape
*/
static void ClientCheckShape(xcb_shape_query_extents_cookie_t cookie,
    Client * client)
{
    xcb_shape_query_extents_reply_t *reply;

    if ((reply = xcb_shape_query_extents_reply(Connection, cookie, NULL))) {
	if (reply->bounding_shaped) {
	    client->State |= WM_STATE_SHAPE;
	}
	free(reply);
    }
    ClientUpdateShape(client);
}
#endif // } USE_SHAPE

/**
**	Shade client.
**
**	@param client	client to shade
*/
void ClientShade(Client * client)
{
    int north;
    int south;
    int east;
    int west;
    uint32_t values[2];

    if (!(client->Border & BORDER_TITLE)) {	// without title can't shade
	return;
    }
    if (client->State & WM_STATE_SHADED) {	// already shaded
	return;
    }

    BorderGetSize(client, &north, &south, &east, &west);

    if (client->State & WM_STATE_MAPPED) {
	xcb_unmap_window(Connection, client->Window);
    }
    client->State |= WM_STATE_SHADED;
    client->State &=
	~(WM_STATE_MINIMIZED | WM_STATE_SHOW_DESKTOP | WM_STATE_MAPPED);

    ShapeRoundedRectWindow(client->Parent, client->Width + west + east, north);

    values[0] = client->Width + east + west;
    values[1] = north;
    xcb_configure_window(Connection, client->Parent,
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

    HintSetAllStates(client);
}

/**
**	Unshade client.
**
**	@param client client to unshade
**
**	@todo FIXME: unshade of shaped clients flickers
*/
void ClientUnshade(Client * client)
{
    int north;
    int south;
    int east;
    int west;
    uint32_t values[2];

    if (!(client->Border & BORDER_TITLE)) {	// without title can't shade
	return;
    }
    if (!(client->State & WM_STATE_SHADED)) {	// already unshaded
	return;
    }
    xcb_map_window(Connection, client->Window);
    client->State |= WM_STATE_MAPPED;
    client->State &= ~WM_STATE_SHADED;

    ClientUpdateShape(client);

    BorderGetSize(client, &north, &south, &east, &west);
    values[0] = client->Width + east + west;
    values[1] = client->Height + north + south;
    xcb_configure_window(Connection, client->Parent,
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

    HintSetAllStates(client);

    ClientRefocus();
    ClientRestack();
}

/**
**	Set full screen status of client.
**
**	@param client		client
**	@param fullscreen	true to make full screen, false not
*/
void ClientSetFullscreen(Client * client, int fullscreen)
{
    uint32_t values[4];

    // make sure there's something to do
    if (!fullscreen == !(client->State & WM_STATE_FULLSCREEN)) {
	return;
    }
    // unshaded the window, if needed
    if (client->State & WM_STATE_SHADED) {
	ClientUnshade(client);
    }

    if (fullscreen) {			// enter fullscreen
	const Screen *screen;

	client->State |= WM_STATE_FULLSCREEN;
	// remove any shapes
	ShapeRoundedRectReset(client->Parent);

	// resize window to screen size
	screen = ScreenGetByXY(client->X, client->Y);
	xcb_reparent_window(Connection, client->Window, XcbScreen->root, 0, 0);
	values[0] = 0;
	values[1] = 0;
	values[2] = screen->Width;
	values[3] = screen->Height;
	xcb_configure_window(Connection, client->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);
	// unmap border window
	xcb_unmap_window(Connection, client->Parent);
	ClientSetLayer(client, LAYER_FULLSCREEN);
    } else {				// leave fullscreen
	int north;
	int south;
	int east;
	int west;

	/// disabled look below: xcb_map_request_event_t event;

	client->State &= ~WM_STATE_FULLSCREEN;

	// reparent window
	BorderGetSize(client, &north, &south, &east, &west);
	xcb_reparent_window(Connection, client->Window, client->Parent, west,
	    north);
	values[0] = west;
	values[1] = north;
	values[2] = client->Width;
	values[3] = client->Height;
	xcb_configure_window(Connection, client->Window,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);

	// restore any shapes
	ClientUpdateShape(client);

	// restore parent position
	Debug(3, "fullscreen old %dx%d%+d%+d\n", client->Width, client->Height,
	    client->X, client->Y);
	values[0] = client->X - west;
	values[1] = client->Y - north;
	values[2] = client->Width + east + west;
	values[3] = client->Height + north + south;
	xcb_configure_window(Connection, client->Parent,
	    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, values);

#if 0
	// FIXME: i think this isn't needed
	// send map request to us
	event.response_type = XCB_MAP_REQUEST;
	event.parent = client->Parent;
	event.window = client->Window;
	xcb_send_event(Connection, XCB_SEND_EVENT_DEST_POINTER_WINDOW,
	    XcbScreen->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
	    (void *)&event);

	client->State |= WM_STATE_MAPPED;
#endif

	// FIXME: shouldn't be back to old layer?
	ClientSetLayer(client, LAYER_NORMAL);

	// map border window again
	xcb_map_window(Connection, client->Parent);
    }

    HintSetAllStates(client);
    ClientSendConfigureEvent(client);
}

/**
**	Raise client to top of its layer.  This will affect transients.
**
**	@param client	client to raise
*/
void ClientRaise(Client * client)
{
    int layer;

    // already on top of its layer
    if (TAILQ_FIRST(&ClientLayers[client->OnLayer]) == client) {
	return;
    }
    // raise window
    TAILQ_REMOVE(&ClientLayers[client->OnLayer], client, LayerQueue);
    TAILQ_INSERT_HEAD(&ClientLayers[client->OnLayer], client, LayerQueue);

    // place any transient windows on top of owner
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	Client *transient;
	Client *temp;

	TAILQ_FOREACH_SAFE(transient, &ClientLayers[layer], LayerQueue, temp) {
	    if (transient->Owner == client->Window) {
		TAILQ_REMOVE(&ClientLayers[layer], transient, LayerQueue);
		TAILQ_INSERT_HEAD(&ClientLayers[client->OnLayer], transient,
		    LayerQueue);
	    }
	}
    }
    ClientRestack();
}

/**
**	Lower client to bottom of its layer.  This will not affect transients.
**
**	@param client	client to lower
*/
void ClientLower(Client * client)
{
    // already on bottom of its layer
    if (TAILQ_LAST(&ClientLayers[client->OnLayer], _client_layer_) == client) {
	return;
    }
    // lower window
    TAILQ_REMOVE(&ClientLayers[client->OnLayer], client, LayerQueue);
    TAILQ_INSERT_TAIL(&ClientLayers[client->OnLayer], client, LayerQueue);

    ClientRestack();
}

/**
**	Set client's state to withdrawn.
**
**	Withdrawn client is client that is not visible in any way to
**	user. This may be window that an application keeps around so that
**	it can be reused at later time.
**
**	@param client	client whose status to change
*/
void ClientSetWithdrawn(Client * client)
{
    if (ClientActive == client) {	// withdrawn can't be active
	ClientActive = NULL;
	client->State &= ~WM_STATE_ACTIVE;
	ClientFocusNextStacked(client);
    }

    if (client->State & WM_STATE_MAPPED) {	// unmap if mapped
	xcb_unmap_window(Connection, client->Window);
	xcb_unmap_window(Connection, client->Parent);
    } else if (client->State & WM_STATE_SHADED) {
	xcb_unmap_window(Connection, client->Parent);
    }

    client->State &=
	~(WM_STATE_SHADED | WM_STATE_MAPPED | WM_STATE_MINIMIZED |
	WM_STATE_SHOW_DESKTOP);

    // FIXME: need only to update WM_STATE_MAPPED!
    // FIXME: HintSetWMState
    HintSetAllStates(client);

    TaskUpdate();
    PagerUpdate();
}

/**
**	Minimize all transients as well as specified client.
**
**	@param owner	client to minimize
*/
static void ClientMinimizeTransients(Client * owner)
{
    Client *client;
    int layer;

    // minimized client can't be active
    if (ClientActive == owner) {
	ClientActive = NULL;
	owner->State &= ~WM_STATE_ACTIVE;
    }
    // unmap window and update its state
    if (owner->State & (WM_STATE_MAPPED | WM_STATE_SHADED)) {
	xcb_unmap_window(Connection, owner->Window);
	xcb_unmap_window(Connection, owner->Parent);
    }
    owner->State |= WM_STATE_MINIMIZED;
    owner->State &= ~WM_STATE_MAPPED;
    HintSetAllStates(owner);

    // minimize transient windows
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(client, &ClientLayers[layer], LayerQueue) {
	    if (client->Owner == owner->Window
		&& (client->State & (WM_STATE_MAPPED | WM_STATE_SHADED))
		&& !(client->State & WM_STATE_MINIMIZED)) {
		ClientMinimizeTransients(client);
	    }
	}
    }
}

/**
**	Minimize client window and all of its transients.
**
**	@param client	client to minimize
*/
void ClientMinimize(Client * client)
{
    if (FocusModus == FOCUS_CLICK && client == ClientActive) {
	ClientFocusNextStacked(client);
    }

    ClientMinimizeTransients(client);

    TaskUpdate();
    PagerUpdate();
}

/**
**	Restore window with its transients (helper method).
**
**	@param owner	client window and its transients to restore
**	@param raise	true to raise client,
**			false to leave stacking unchanged.
*/
static void ClientRestoreTransients(Client * owner, int raise)
{
    Client *transient;
    int layer;

    // restore this window
    if (!(owner->State & WM_STATE_MAPPED)) {
	if (owner->State & WM_STATE_SHADED) {
	    xcb_map_window(Connection, owner->Parent);
	} else {
	    xcb_map_window(Connection, owner->Window);
	    xcb_map_window(Connection, owner->Parent);
	    owner->State |= WM_STATE_MAPPED;
	}
    }
    owner->State &= ~WM_STATE_MINIMIZED;
    owner->State &= ~WM_STATE_SHOW_DESKTOP;

    HintSetAllStates(owner);

    // restore transient windows
    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	TAILQ_FOREACH(transient, &ClientLayers[layer], LayerQueue) {
	    if (transient->Owner == owner->Window
		&& !(transient->State & (WM_STATE_MAPPED | WM_STATE_SHADED))
		&& (transient->State & WM_STATE_MINIMIZED)) {
		ClientRestoreTransients(transient, raise);
	    }
	}
    }

    if (raise) {
	ClientRaise(owner);
    }
}

/**
**	Restore client window and its transients.
**
**	@param client	client to restore
**	@param raise	true to raise client,
**			false to leave stacking unchanged.
*/
void ClientRestore(Client * client, int raise)
{
    ClientRestoreTransients(client, raise);

    ClientRestack();
    TaskUpdate();
    PagerUpdate();
}

/**
**	Maximize client window.
**
**	@param client	client to maximize
**	@param horz	true to maximize client horizontally
**	@param vert	true to maximize client vertically
*/
void ClientMaximize(Client * client, int horz, int vert)
{
    int north;
    int south;
    int east;
    int west;
    uint32_t values[4];

    // we don't want to mess with full screen clients
    if (client->State & WM_STATE_FULLSCREEN) {
	return;
    }
    if (client->State & WM_STATE_SHADED) {
	ClientUnshade(client);
    }

    if (client->State & (WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT)) {
	client->X = client->OldX;
	client->Y = client->OldY;
	client->Width = client->OldWidth;
	client->Height = client->OldHeight;
	client->State &= ~(WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT);
    } else {
	ClientPlaceTiled(client, 0 | (horz ? 0x05 : 0) | (vert ? 0x50 : 0));
    }

    ClientUpdateShape(client);

    BorderGetSize(client, &north, &south, &east, &west);
    values[0] = client->X - west;
    values[1] = client->Y - north;
    values[2] = client->Width + east + west;
    values[3] = client->Height + north + south;
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

    HintSetAllStates(client);
    ClientSendConfigureEvent(client);
}

/**
**	Tile client window.
**
**	@param client	client to maximize
**	@param type	type of tile
**
**	Type is a bit field: YyHhXxWw
**
**	- Yy encodes y 00=unchanged 01=top 10=bottom 11=center
**	- Hh encodes hight 00=unchanged 01=100% 10=50% 11=33%
**	- Xx encodes x 00=unchanged 01=right 10=left 11=center
**	- Ww encodes width 00=unchanged 01=100% 10=50% 11=33%
*/
void ClientTile(Client * client, int type)
{
    int north;
    int south;
    int east;
    int west;
    uint32_t values[4];

    // we don't want to mess with full screen clients
    if (client->State & WM_STATE_FULLSCREEN) {
	return;
    }
    if (client->State & WM_STATE_SHADED) {
	ClientUnshade(client);
    }

    if (client->State & (WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT)) {
	client->X = client->OldX;
	client->Y = client->OldY;
	client->Width = client->OldWidth;
	client->Height = client->OldHeight;
    }
    ClientPlaceTiled(client, type);

    ClientUpdateShape(client);

    BorderGetSize(client, &north, &south, &east, &west);
    values[0] = client->X - west;
    values[1] = client->Y - north;
    values[2] = client->Width + east + west;
    values[3] = client->Height + north + south;
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

    HintSetAllStates(client);
    ClientSendConfigureEvent(client);
}

/**
**	Maximize client using its default maximize settings.
**
**	@param client	client to maximize
*/
void ClientMaximizeDefault(Client * client)
{
    ClientMaximize(client, (client->Border & BORDER_MAXIMIZE_HORZ) ? 1 : 0,
	(client->Border & BORDER_MAXIMIZE_VERT) ? 1 : 0);
}

/**
**	Show hidden client.  This will not update transients.
**	This is used for changing desktops.
**
**	@param client	client to show
*/
void ClientShow(Client * client)
{
    Debug(3, "CMD: %s(%x)\n", __FUNCTION__, client->Window);

    if (client->State & WM_STATE_HIDDEN) {
	client->State &= ~WM_STATE_HIDDEN;
	if (client->State & (WM_STATE_MAPPED | WM_STATE_SHADED)) {
	    xcb_map_window(Connection, client->Parent);
	    if (client->State & WM_STATE_ACTIVE) {
		ClientFocus(client);
	    }
	}
    }
}

/**
**	Hide client without unmapping.	This will not update transients.
**	This is used for changing desktops.
**
**	@param client	client to hide
*/
void ClientHide(Client * client)
{
    Debug(3, "CMD: %s(%x)\n", __FUNCTION__, client->Window);

    if (ClientActive == client) {
	ClientActive = NULL;
    }
    client->State |= WM_STATE_HIDDEN;
    if (client->State & (WM_STATE_MAPPED | WM_STATE_SHADED)) {
	xcb_unmap_window(Connection, client->Parent);
    }
}

// ---------------------------------------------------------------------------

/**
**	Set client layer. This will affect transients.
**
**	@param client	client whose layer to set
**	@param layer	layer to assign to client
*/
void ClientSetLayer(Client * client, int layer)
{
    if (layer > LAYER_TOP) {
	Warning("client %s requested an invalid layer: %d\n", client->Name,
	    layer);
	return;
    }

    if (client->OnLayer != layer) {
	int l;

	// loop through all clients so we get transients
	for (l = LAYER_BOTTOM; l < LAYER_MAX; l++) {
	    Client *temp;
	    Client *next;

	    if (l == layer) {		// skip destination layer
		continue;
	    }
	    TAILQ_FOREACH_SAFE(temp, &ClientLayers[l], LayerQueue, next) {
		if (temp == client || temp->Owner == client->Window) {
		    // remove from old node list
		    TAILQ_REMOVE(&ClientLayers[client->OnLayer], temp,
			LayerQueue);
		    // insert into new node list
		    TAILQ_INSERT_HEAD(&ClientLayers[layer], temp, LayerQueue);

		    // set new layer
		    temp->OnLayer = layer;
		}
	    }
	}
	ClientRestack();
    }
}

/**
**	Set client's desktop. This will update transients.
**
**	@param client	client
**	@param desktop	desktop to be assigned to client
*/
void ClientSetDesktop(Client * client, int desktop)
{
    if (desktop >= DesktopN) {
	return;
    }

    if (!(client->State & WM_STATE_STICKY)) {
	int layer;

	for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	    Client *temp;

	    TAILQ_FOREACH(temp, &ClientLayers[layer], LayerQueue) {
		if (temp == client || temp->Owner == client->Window) {

		    temp->Desktop = desktop;
		    if (desktop == DesktopCurrent) {
			ClientShow(temp);
		    } else {
			ClientHide(temp);
		    }

		    HintSetNetWMDesktop(temp);
		}
	    }
	}
	TaskUpdate();
	PagerUpdate();
    }
}

/**
**	Set client's sticky status.  This will update transients.
**
**	A sticky client will appear on all desktops.
**
**	@param client	client
**	@param sticky	true to make client sticky, false to make it not sticky
*/
void ClientSetSticky(Client * client, int sticky)
{
    if (sticky && !(client->State & WM_STATE_STICKY)) {
	int layer;

	// change from non-sticky to sticky
	for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	    Client *temp;

	    TAILQ_FOREACH(temp, &ClientLayers[layer], LayerQueue) {
		if (temp == client || temp->Owner == client->Window) {
		    temp->State |= WM_STATE_STICKY;
		    // FIXME: not all states need to be updated
		    HintSetAllStates(temp);
		    BorderDraw(temp, NULL);
		}
	    }
	}

    } else if (!sticky && (client->State & WM_STATE_STICKY)) {
	int layer;

	// change from sticky to non-sticky
	for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	    Client *temp;

	    TAILQ_FOREACH(temp, &ClientLayers[layer], LayerQueue) {
		if (temp == client || temp->Owner == client->Window) {
		    temp->State &= ~WM_STATE_STICKY;
		    // FIXME: not all states need to be updated
		    HintSetAllStates(temp);
		    BorderDraw(temp, NULL);
		}
	    }
	}

	// since this client is no longer sticky, we need to assign
	// desktop. Here we use current desktop.
	// @note: that ClientSetDesktop updates transients (which is good).
	ClientSetDesktop(client, DesktopCurrent);
    }
}

// ---------------------------------------------------------------------------

/**
**	Reparent client window.
**
**	@param client		client to be reparent
**	@param not_owner	true, if we doesn't own this window
*/
static void ClientReparent(Client * client, int not_owner)
{
    uint32_t values[5];
    int x;
    int y;
    unsigned width;
    unsigned height;
    int north;
    int south;
    int east;
    int west;

#ifdef USE_SHAPE
    xcb_shape_query_extents_cookie_t cookie;

    if (HaveShape) {
	cookie = ClientCheckShapeRequest(client);
    }
#endif
    if (not_owner) {
	xcb_change_save_set(Connection, XCB_SET_MODE_INSERT, client->Window);

	values[0] =
	    XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE |
	    XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_COLOR_MAP_CHANGE;
	values[1] = XCB_EVENT_MASK_NO_EVENT;
	xcb_change_window_attributes(Connection, client->Window,
	    XCB_CW_EVENT_MASK | XCB_CW_DONT_PROPAGATE, values);
    }
    //
    //	grab what is used
    //
    xcb_grab_button(Connection, 1, client->Window, XCB_EVENT_MASK_BUTTON_PRESS,
	XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE,
	XCB_GRAB_ANY, XCB_BUTTON_MASK_ANY);
    KeyboardGrabBindings(client);

    //
    //	create frame window
    //
    x = client->X;
    y = client->Y;
    width = client->Width;
    height = client->Height;
    BorderGetSize(client, &north, &south, &east, &west);
    x -= west;
    y -= north;
    width += east + west;
    height += north + south;

    client->Parent = xcb_generate_id(Connection);

    values[0] = XCB_BACK_PIXMAP_PARENT_RELATIVE;
    values[1] = Colors.TitleBG2.Pixel;
    values[2] = 1;
    values[3] =
	XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
	XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
	XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_EXPOSURE |
	XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
    values[4] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;

    xcb_create_window(Connection, XCB_COPY_FROM_PARENT, client->Parent,
	XcbScreen->root, x, y, width, height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
	XCB_COPY_FROM_PARENT,
	XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT |
	XCB_CW_EVENT_MASK | XCB_CW_DONT_PROPAGATE, values);

    // update client window to get only events we want
    values[0] =
	XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
	XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
	XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION;
    xcb_change_window_attributes(Connection, client->Window,
	XCB_CW_DONT_PROPAGATE, values);

    values[0] = 0;
    xcb_configure_window(Connection, client->Window,
	XCB_CONFIG_WINDOW_BORDER_WIDTH, values);

    // reparent client window
    xcb_reparent_window(Connection, client->Window, client->Parent, west,
	north);

#ifdef USE_SHAPE
    if (HaveShape) {
	// XCB_SHAPE_NOTIFY
	xcb_shape_select_input(Connection, client->Window, 1);
	ClientCheckShape(cookie, client);
    }
#endif
    // FIXME: should i use frame window for frameless windows?
}

/**
**	Get current active client.
**
**	@returns client (NULL if none active).
*/
Client *ClientGetActive(void)
{
    return ClientActive;
}

/**
**	Find client by its frame = parent window.
**
**	@param window	parent window
**
**	@returns client (NULL if not found).
**
**	@todo use table lookup (hash vs judy vs burst trie)
**	@todo FrameList is not required without hash
*/
Client *ClientFindByFrame(xcb_window_t window)
{
    Client *client;

    LIST_FOREACH(client, &ClientByFrame, FrameList) {
	if (client->Parent == window) {
	    return client;
	}
    }
    return NULL;
}

/**
**	Find client by its own client window.
**
**	@param window	window id of client
**
**	@returns client (NULL if not found).
**
**	@todo use table lookup (hash vs judy vs burst trie)
*/
Client *ClientFindByChild(xcb_window_t window)
{
    Client *client;

    LIST_FOREACH(client, &ClientByChild, ChildList) {
	if (client->Window == window) {
	    return client;
	}
    }
    return NULL;
}

/**
**	Find client by window or parent window.
**
**	@param window	window id of client or frame
**
**	@returns client (NULL if not found).
**
**	@todo use hash lookup or array
*/
Client *ClientFindByAny(xcb_window_t window)
{
    Client *client;

    if ((client = ClientFindByChild(window))) {
	return client;
    }
    return ClientFindByFrame(window);
}

/**
**	Add window to management.
**
**	@param window		client window
**	@param cookie		request cookie for get window attributes
**	@param already_mapped	true if window is mapped, false if not
**	@param not_owner	true if we doesn't own this window
**
**	@returns our client window data.
*/
Client *ClientAddWindow(xcb_window_t window,
    xcb_get_window_attributes_cookie_t cookie, int already_mapped,
    int not_owner)
{
    xcb_get_window_attributes_reply_t *attr_reply;
    xcb_get_geometry_cookie_t geom_cookie;
    xcb_get_geometry_reply_t *geom_reply;
    Client *client;

    attr_reply = xcb_get_window_attributes_reply(Connection, cookie, NULL);
    if (!attr_reply) {
	return NULL;			// error can't get reply
    }
    // don't manage clients with override redirect flag
    // check if window is mapped and don't care about input only windows
    if (attr_reply->override_redirect || (already_mapped
	    && attr_reply->map_state != XCB_MAP_STATE_VIEWABLE)
	|| attr_reply->_class == XCB_WINDOW_CLASS_INPUT_ONLY) {
	free(attr_reply);
	return NULL;
    }

    geom_cookie = xcb_get_geometry_unchecked(Connection, window);

    // prepare client structure for this window
    client = calloc(1, sizeof(*client));
    client->RefCount = 1;
    client->Window = window;
    ++ClientN;

#ifdef USE_COLORMAP
    client->Colormap = attr_reply->colormap;
#endif
    free(attr_reply);

    // FIXME: pre fetch all properties

    geom_reply = xcb_get_geometry_reply(Connection, geom_cookie, NULL);
    client->X = geom_reply->x;
    client->Y = geom_reply->y;
    client->Width = geom_reply->width;
    client->Height = geom_reply->height;
    free(geom_reply);

    HintGetClientProtocols(client);	// set defaults State,Border,...

    // we didn't own this window
    if (!not_owner) {
	// add some border
	client->Border = BORDER_OUTLINE | BORDER_TITLE | BORDER_MOVE;
	client->State |= WM_STATE_WMDIALOG | WM_STATE_STICKY;
    }
#ifdef USE_ICON
    IconLoadClient(client);		// icon could be overwritten by rules
#endif
    RulesApplyNewClient(client, already_mapped);

    // insert client into correct layer
    TAILQ_INSERT_HEAD(&ClientLayers[client->OnLayer], client, LayerQueue);

    Debug(3, "%s: client %s state = %#x\n", __FUNCTION__, client->Name,
	client->State);

    PointerSetDefaultCursor(client->Window);
    ClientReparent(client, not_owner);

    ClientPlace(client, already_mapped);

    // insert client into net-client list
    SLIST_INSERT_HEAD(&ClientNetList, client, NetClient);

    // insert client into search queue's
    LIST_INSERT_HEAD(&ClientByChild, client, ChildList);
    LIST_INSERT_HEAD(&ClientByFrame, client, FrameList);

    if (client->State & WM_STATE_MAPPED) {
	// FIXME: below we unmap client window for shape or minimized!
	// FIXME: now mapped and below unmapped for fullscreen
	xcb_map_window(Connection, client->Window);
	xcb_map_window(Connection, client->Parent);
    }
    BorderDraw(client, NULL);

    // FIXME: not needed during startup
    TaskUpdate();
    HintSetNetClientList();

    if (!already_mapped) {
	ClientRaise(client);
    }
    // minimize client if requested
    if (client->State & WM_STATE_MINIMIZED) {
	Debug(3, "%s: minimize client %s\n", __FUNCTION__, client->Name);
	client->State &= ~WM_STATE_MINIMIZED;
	// FIXME: above we map and now we unmap....
	ClientMinimize(client);
    }
    // fullscreen client if requested
    if (client->State & WM_STATE_FULLSCREEN) {
	// remove the state, to get the action working
	client->State &= ~WM_STATE_FULLSCREEN;
	ClientSetFullscreen(client, 1);
    }
    // shade client if requested
    if (client->State & WM_STATE_SHADED) {
	client->State &= ~WM_STATE_SHADED;
	// FIXME: above we map and now we unmap....
	ClientShade(client);
    }
    // maximize client if requested
    if (client->State & (WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT)) {
	client->State &= ~(WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT);
	ClientMaximizeDefault(client);
    }
    // make sure we're still in sync
    HintSetAllStates(client);
    ClientSendConfigureEvent(client);

    // hide client if we're not on right desktop
    if (client->Desktop != DesktopCurrent
	&& !(client->State & WM_STATE_STICKY)) {
	Debug(3, "%s: hide client %s\n", __FUNCTION__, client->Name);
	ClientHide(client);
    }

    ClientGetStrut(client);

    // focus transients, if their parent has focus
    if (client->Owner && ClientActive && client->Owner == ClientActive->Window) {
	ClientFocus(client);
    }

    return client;
}

/**
**	Remove client window from management.
**
**	@param client	client to remove
*/
void ClientDelWindow(Client * client)
{
    // update rules matches
    RulesApplyDelClient(client);

    // remove this client from client list
    TAILQ_REMOVE(&ClientLayers[client->OnLayer], client, LayerQueue);
    --ClientN;

    // remove client from search queue's
    LIST_REMOVE(client, ChildList);
    LIST_REMOVE(client, FrameList);

    // remove client from net client list
    SLIST_REMOVE(&ClientNetList, client, _client_, NetClient);

    // make sure this client isn't active
    if (ClientActive == client && KeepLooping) {
	ClientFocusNextStacked(client);
    }
    if (ClientActive == client) {
	AtomSetWindow(XcbScreen->root, &Atoms.NET_ACTIVE_WINDOW, XCB_NONE);
	ClientActive = NULL;
    }
    // grab server to avoid race conditions
    xcb_grab_server(Connection);

    //
    //	If window manager is exiting (ie, not client), then reparent etc.
    //
    if (!KeepLooping && !(client->State & WM_STATE_WMDIALOG)) {
	if (client->State & (WM_STATE_MAXIMIZED_VERT |
		WM_STATE_MAXIMIZED_HORZ)) {
	    uint32_t values[4];

	    client->X = client->OldX;
	    client->Y = client->OldY;
	    client->Width = client->OldWidth;
	    client->Height = client->OldHeight;

	    values[0] = client->X;
	    values[1] = client->Y;
	    values[2] = client->Width;
	    values[3] = client->Height;
	    xcb_configure_window(Connection, client->Window,
		XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
		XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
	}
	ClientGravitate(client, 1);
	if (!(client->State & WM_STATE_MAPPED)
	    && (client->State & (WM_STATE_MINIMIZED | WM_STATE_SHADED))) {
	    xcb_map_window(Connection, client->Window);
	}
	xcb_ungrab_button(Connection, XCB_BUTTON_INDEX_ANY, client->Window,
	    XCB_BUTTON_MASK_ANY);
	xcb_reparent_window(Connection, client->Window, XcbScreen->root,
	    client->X, client->Y);
	xcb_change_save_set(Connection, XCB_SET_MODE_DELETE, client->Window);
    }
    // destroy parent
    if (client->Parent) {
	xcb_destroy_window(Connection, client->Parent);
    }
    // free resources
    free(client->Name);
    free(client->InstanceName);
    free(client->ClassName);

    TaskUpdate();
    HintSetNetClientList();
    ClientDelStrut(client);
    PagerUpdate();

#ifdef USE_COLORMAP
#if 0
    // FIXME:
    while (client->Colormaps) {
	Colormap *cm;

	cm = client->Colormaps->Next;
	free(client->Colormaps);
	client->Colormaps = cm;
    }
#endif
#endif

#ifdef USE_ICON
    // reference counts for icons, if same icon i used for menu
    IconDel(client->Icon);
#endif

    client->Deleted = 1;
    if (--client->RefCount <= 0) {
	free(client);
    }

    xcb_flush(Connection);
    xcb_ungrab_server(Connection);

    ClientRestack();
}

/**
**	Callback to kill client after confirm dialog.
**
**	@param client	client to kill
*/
static void ClientKillHandler(Client * client)
{
    --client->RefCount;
    if (client->Deleted) {		// already deleted
	if (client->RefCount <= 0) {
	    free(client);
	}
	return;
    }
    if (client == ClientActive) {
	ClientFocusNextStacked(client);
    }

    xcb_grab_server(Connection);
    xcb_aux_sync(Connection);

    xcb_kill_client(Connection, client->Window);

    xcb_aux_sync(Connection);
    xcb_ungrab_server(Connection);

    ClientDelWindow(client);
}

/**
**	Kill client window.
**
**	@param client	client to kill
*/
void ClientKill(Client * client)
{
    if (ShowKillConfirmation) {
	++client->RefCount;
	DialogShowConfirm(client, ClientKillHandler, "Kill this window?",
	    "This may cause data to be lost!", NULL);
    } else {
	ClientKillHandler(client);
    }
}

/**
**	Send delete message to client.
**
**	@param client	client to delete
*/
void ClientDelete(Client * client)
{
    xcb_get_property_cookie_t cookie;
    xcb_icccm_get_wm_protocols_reply_t protocols;

    cookie =
	xcb_icccm_get_wm_protocols_unchecked(Connection, client->Window,
	Atoms.WM_PROTOCOLS.Atom);

    // FIXME: move into functions...
    // check if client supports WM_DELETE_WINDOW
    if (xcb_icccm_get_wm_protocols_reply(Connection, cookie, &protocols, NULL)) {
	unsigned u;

	for (u = 0; u < protocols.atoms_len; ++u) {
	    if (protocols.atoms[u] == Atoms.WM_DELETE_WINDOW.Atom) {
		ClientSendDeleteWindow(client->Window);
		xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
		return;
	    }
	}
	xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
    }
    // do hard way:
    ClientKill(client);
}

// ---------------------------------------------------------------------------

/**
**	Set focus to window currently under mouse pointer.
**
**	@warning Can't use old request, clients could be restacked.
**
**	@todo but I can request after all clients are mapped und decored
*/
static void ClientUpdateFocus(void)
{
    xcb_query_pointer_cookie_t cookie;
    xcb_query_pointer_reply_t *reply;
    Client *client;

    cookie = xcb_query_pointer_unchecked(Connection, XcbScreen->root);
    reply = xcb_query_pointer_reply(Connection, cookie, NULL);
    if (reply) {
	client = ClientFindByAny(reply->child);
	if (client) {
	    ClientFocus(client);
	}
	free(reply);
    }
}

/**
**	Prepare initialize clients.
*/
void ClientPreInit(void)
{
    QueryTreeCookie = xcb_query_tree_unchecked(Connection, XcbScreen->root);
}

/**
**	Initialize client module.
*/
void ClientInit(void)
{
    unsigned u;
    xcb_query_tree_reply_t *reply;
    xcb_window_t *children;
    xcb_get_window_attributes_cookie_t *cookies;
    int i;
    int len;

    // clear out client lists
    SLIST_INIT(&ClientNetList);
    LIST_INIT(&ClientByFrame);
    LIST_INIT(&ClientByChild);
    for (u = LAYER_BOTTOM; u < LAYER_MAX; u++) {
	TAILQ_INIT(&ClientLayers[u]);
    }

    //
    //	Setup opacity
    //
    ClientTopmostOpacity = UINT32_MAX;
    ClientMaxStackingOpacity = 0.9 * UINT32_MAX;
    ClientMinStackingOpacity = 0.4 * UINT32_MAX;
    ClientStackingStepOpacity = 0.1 * UINT32_MAX;

    //
    // load windows that are already mapped
    //
    // query client windows pre-fetched from pre-init
    if (!(reply = xcb_query_tree_reply(Connection, QueryTreeCookie, NULL))) {
	Debug(2, "xcb_query_tree_reply failed\n");
	return;
    }

    len = xcb_query_tree_children_length(reply);
    cookies = alloca(len * sizeof(*cookies));

    // request window attributes for every window
    children = xcb_query_tree_children(reply);
    for (i = 0; i < len; ++i) {
	// FIXME: check if swallow needed

	if (SwallowTryWindow(1, children[i])) {
	    cookies[i].sequence = 0;
	} else {
	    cookies[i] =
		xcb_get_window_attributes_unchecked(Connection, children[i]);
	}
    }

    // add each client
    for (i = 0; i < len; ++i) {
	if (cookies[i].sequence) {
	    ClientAddWindow(children[i], cookies[i], 1, 1);
	}
    }
    free(reply);

    ClientUpdateFocus();
    TaskUpdate();
    PagerUpdate();
}

/**
**	Cleanup client module.
*/
void ClientExit(void)
{
    int layer;

    for (layer = LAYER_BOTTOM; layer < LAYER_MAX; layer++) {
	Client *client;

	// reverse to keep stacking order
	client = TAILQ_LAST(&ClientLayers[layer], _client_layer_);
	while (client) {		// faster TailQ Deletion
	    Client *temp;

	    temp = TAILQ_PREV(client, _client_layer_, LayerQueue);
	    ClientDelWindow(client);
	    client = temp;
	}
	TAILQ_INIT(&ClientLayers[layer]);
    }

}

/// @}
