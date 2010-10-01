///
///	@file client.c		@brief client functions
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

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "client.h"
#include "border.h"
#include "hints.h"

#include "draw.h"
#include "screen.h"
#include "image.h"
#include "icon.h"
#include "menu.h"
#include "panel.h"

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
/// @{

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
		strut->Rectangle.X = RootWidth - data[1];
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
		strut->Rectangle.Y = RootHeight - data[3];
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
		strut->Rectangle.Height = RootHeight;
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[1] > 0) {		// right
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = RootWidth - data[1];
		strut->Rectangle.Y = 0;
		strut->Rectangle.Width = data[1];
		strut->Rectangle.Height = RootHeight;
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[2] > 0) {		// top
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = 0;
		strut->Rectangle.Y = 0;
		strut->Rectangle.Width = RootWidth;
		strut->Rectangle.Height = data[2];
		LIST_INSERT_HEAD(&Struts, strut, Node);
	    }

	    if (data[3] > 0) {		// bottom
		strut = malloc(sizeof(Strut));
		strut->Client = client;
		strut->Rectangle.X = 0;
		strut->Rectangle.Y = RootHeight - data[3];
		strut->Rectangle.Width = RootWidth;
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
		flags & (XCB_SIZE_HINT_P_POSITION |
		    XCB_SIZE_HINT_US_POSITION)))) {
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
**	Place maximized client on screen.
**
**	@param client	client to place
**	@param horz	true if maximizing horizontally
**	@param vert	true if maximizing vertically
*/
void ClientPlaceMaximized(Client * client, int horz, int vert)
{
    int north;
    int south;
    int east;
    int west;
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

    if (client->SizeHints.flags & XCB_SIZE_HINT_P_ASPECT) {
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
    // if maximizing horizontally, update width
    if (horz) {
	client->X = rectangle.X;
	client->Width =
	    rectangle.Width - (rectangle.Width % client->SizeHints.width_inc);
	client->State |= WM_STATE_MAXIMIZED_HORZ;
    }
    // if maximizing vertically, update height
    if (vert) {
	client->Y = rectangle.Y;
	client->Height =
	    rectangle.Height -
	    (rectangle.Height % client->SizeHints.height_inc);
	client->State |= WM_STATE_MAXIMIZED_VERT;
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

    if (client->SizeHints.flags & XCB_SIZE_HINT_P_ASPECT) {
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

///
///	@defgroup client The client module.
///
///	This module contains client control functions.
///
/// @{

/// @}
