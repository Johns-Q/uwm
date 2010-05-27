///
///	@file pointer.c		@brief Pointer/Cursor functions
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
///	@defgroup pointer The pointer/cursor module.
///
///	This module handles pointers and cursors.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>

#include <X11/cursorfont.h>		// cursor font XC_

#include "pointer.h"

//////////////////////////////////////////////////////////////////////////////

static int PointerX;			///< cached current pointer x position
static int PointerY;			///< cached current pointer y position

CursorTable Cursors;			///< all cursors

/**
**	Set current pointer position.
**
**	@param x	current x-coordinate of pointer
**	@param y	current y-coordinate of pointer
*/
void PointerSetPosition(int x, int y)
{
    PointerX = x;
    PointerY = y;
}

/**
**	Get current pointer position.
**
**	@param[out] x	set to current x-coordinate of pointer
**	@param[out] y	set to current y-coordinate of pointer
*/
void PointerGetPosition(int *x, int *y)
{
    *x = PointerX;
    *y = PointerY;
}

/**
**	Get current button mask.
**
**	@todo FIXME: split into request / reply.
*/
xcb_button_mask_t PointerGetButtonMask(void)
{
    xcb_query_pointer_cookie_t query_cookie;
    xcb_query_pointer_reply_t *query_reply;
    xcb_button_mask_t mask;

    query_cookie = xcb_query_pointer_unchecked(Connection, RootWindow);
    mask = 0;
    query_reply = xcb_query_pointer_reply(Connection, query_cookie, NULL);
    if (query_reply) {
	PointerX = query_reply->root_x;
	PointerY = query_reply->root_y;
	mask = query_reply->mask;
	free(query_reply);
    }
    return mask;
}

/**
**	Grab pointer request.
**
**	@param window	specifies grab window
**	@param cursor	cursor to be displayed during grab
**
**	@returns cookie to get grab reply.
*/
xcb_grab_pointer_cookie_t PointerGrabRequest(xcb_window_t window,
    xcb_cursor_t cursor)
{
    return xcb_grab_pointer_unchecked(Connection, 0, window,
	XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_PRESS |
	XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
	XCB_GRAB_MODE_ASYNC, XCB_NONE, cursor, XCB_CURRENT_TIME);
}

/**
**	Grab pointer reply.
**
**	@param cookie	 cookie of grab pointer request
**
**	@returns true if grab successfull, false otherwise.
*/
int PointerGrabReply(xcb_grab_pointer_cookie_t cookie)
{
    xcb_grab_pointer_reply_t *reply;

    reply = xcb_grab_pointer_reply(Connection, cookie, NULL);
    if (reply) {
	int status;

	Debug(3, "  grab pointer %d\n", reply->status);
	status = reply->status == XCB_GRAB_STATUS_SUCCESS;
	free(reply);
	return status;
    }
    return 0;
}

/**
**	Request pointer grab with default cursor.
**
**	@param window	specifies grab window
**
**	@returns cookie to get grab reply.
*/
xcb_grab_pointer_cookie_t PointerGrabDefaultRequest(xcb_window_t window)
{
    return PointerGrabRequest(window, Cursors.Default);
}

/**
**	Request pointer grab for moving a window.
**
**	@returns cookie to get grab reply.
*/
xcb_grab_pointer_cookie_t PointerGrabForMoveRequest(void)
{
    return PointerGrabRequest(RootWindow, Cursors.Move);
}

/**
**	Request pointer grab for choosing a window.
**
**	@returns cookie to get grab reply.
*/
xcb_grab_pointer_cookie_t PointerGrabForChooseRequest(void)
{
    return PointerGrabRequest(RootWindow, Cursors.Choose);
}

/**
**	Send query pointer request.
**
**	@returns cookie of query pointer request.
*/
xcb_query_pointer_cookie_t PointerQueryRequest(void)
{
    return xcb_query_pointer_unchecked(Connection, RootWindow);
}

/**
**	Get query pointer reply.  Stores current pointer position in
**	#PointerX and #PointerY.
**
**	@param cookie	cookie of query pointer request
*/
void PointerQueryReply(xcb_query_pointer_cookie_t cookie)
{
    xcb_query_pointer_reply_t *reply;

    reply = xcb_query_pointer_reply(Connection, cookie, NULL);
    if (reply) {
	PointerX = reply->root_x;
	PointerY = reply->root_y;
	free(reply);
    }
}

/**
**	Query pointer.
**
**	@todo FIXME: remove use of this function.
*/
void PointerQuery(void)
{
    PointerQueryReply(PointerQueryRequest());
}

/**
**	Wrap pointer.
**
**	Updates cached #PointerX and #PointerY.
**
**	@param window	wrap pointer relative to this window
**	@param x	destination x-coordinate of pointer
**	@param y	destination y-coordinate of pointer
**
**	FIXME: split into request / reply.
*/
void PointerWrap(xcb_window_t window, int x, int y)
{
    xcb_warp_pointer(Connection, XCB_NONE, window, 0, 0, 0, 0, x, y);

    PointerQuery();
}

/**
**	Set default cursor for pointer.
**
**	@param window	specifies window to get default cursor
*/
void PointerSetDefaultCursor(xcb_window_t window)
{
    xcb_change_window_attributes(Connection, window, XCB_CW_CURSOR,
	&Cursors.Default);
}

// ---------------------------------------------------------------------------

/**
**	Create pointer
**
**	@param font	cursor font id
**	@param index	character index for cursor
**
**	@returns cursor id of new created cursor
*/
static xcb_cursor_t CursorCreate(xcb_font_t font, int index)
{
    xcb_cursor_t c;

    c = xcb_generate_id(Connection);
    xcb_create_glyph_cursor(Connection, c, font, font, index, index + 1, 0, 0,
	0, 65535, 65535, 65535);

    return c;
}

/**
**	Prepare initialize pointers.
*/
void PointerPreInit(void)
{
    // FIXME: move query here
}

/**
**	Initialize pointers.
*/
void PointerInit(void)
{
    xcb_query_pointer_cookie_t query_cookie;
    xcb_font_t font;

    query_cookie = PointerQueryRequest();

    font = xcb_generate_id(Connection);
    xcb_open_font(Connection, font, sizeof(CURSOR_FONT) - 1, CURSOR_FONT);

    Cursors.Default = CursorCreate(font, XC_left_ptr);
    Cursors.Move = CursorCreate(font, XC_fleur);
    Cursors.North = CursorCreate(font, XC_top_side);
    Cursors.South = CursorCreate(font, XC_bottom_side);
    Cursors.East = CursorCreate(font, XC_right_side);
    Cursors.West = CursorCreate(font, XC_left_side);
    //Cursors.NorthEast = CursorCreate(font, XC_ur_angle);
    //Cursors.NorthWest = CursorCreate(font, XC_ul_angle);
    //Cursors.SouthEast = CursorCreate(font, XC_lr_angle);
    //Cursors.SouthWest = CursorCreate(font, XC_ll_angle);
    Cursors.NorthEast = CursorCreate(font, XC_top_right_corner);
    Cursors.NorthWest = CursorCreate(font, XC_top_left_corner);
    Cursors.SouthEast = CursorCreate(font, XC_bottom_right_corner);
    Cursors.SouthWest = CursorCreate(font, XC_bottom_left_corner);
    Cursors.Choose = CursorCreate(font, XC_tcross);

    xcb_close_font(Connection, font);

    PointerQueryReply(query_cookie);
}

/**
**	Cleanup the pointer module.
**
**	@todo FIXME: use loop, should be shorter
*/
void PointerExit(void)
{
    xcb_free_cursor(Connection, Cursors.Default);
    xcb_free_cursor(Connection, Cursors.Move);
    xcb_free_cursor(Connection, Cursors.North);
    xcb_free_cursor(Connection, Cursors.South);
    xcb_free_cursor(Connection, Cursors.East);
    xcb_free_cursor(Connection, Cursors.West);
    xcb_free_cursor(Connection, Cursors.NorthEast);
    xcb_free_cursor(Connection, Cursors.NorthWest);
    xcb_free_cursor(Connection, Cursors.SouthEast);
    xcb_free_cursor(Connection, Cursors.SouthWest);
    xcb_free_cursor(Connection, Cursors.Choose);
}

/// @}
