///
///	@file pointer.c		@brief Pointer/Cursor functions
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
///	@defgroup pointer The pointer/cursor module.
///
///	This module handles pointers and cursors.
///
///< @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>

#include <X11/cursorfont.h>		// cursor font XC_

#include "pointer.h"

//////////////////////////////////////////////////////////////////////////////

    /// query pointer cookie for init
static xcb_query_pointer_cookie_t PointerCookie;
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
    return PointerGrabRequest(XcbScreen->root, Cursors.Move);
}

/**
**	Request pointer grab for choosing a window.
**
**	@returns cookie to get grab reply.
*/
xcb_grab_pointer_cookie_t PointerGrabForChooseRequest(void)
{
    return PointerGrabRequest(XcbScreen->root, Cursors.Choose);
}

/**
**	Send query pointer request.
**
**	@returns cookie of query pointer request.
*/
xcb_query_pointer_cookie_t PointerQueryRequest(void)
{
    return xcb_query_pointer_unchecked(Connection, XcbScreen->root);
}

/**
**	Get query pointer reply.  Stores current pointer position in
**	#PointerX and #PointerY.
**
**	@param cookie	cookie of query pointer request
**
**	@returns current pointer button mask.
*/
xcb_button_mask_t PointerQueryReply(xcb_query_pointer_cookie_t cookie)
{
    xcb_query_pointer_reply_t *reply;
    xcb_button_mask_t mask;

    mask = 0;
    reply = xcb_query_pointer_reply(Connection, cookie, NULL);
    if (reply) {
	PointerX = reply->root_x;
	PointerY = reply->root_y;
	mask = reply->mask;
	free(reply);
    }
    return mask;
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
**	@todo FIXME: split into request / reply.
*/
void PointerWrap(xcb_window_t window, int x, int y)
{
    xcb_warp_pointer(Connection, XCB_NONE, window, 0, 0, 0, 0, x, y);

    PointerQueryReply(PointerQueryRequest());
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
    xcb_cursor_t cursor;

    cursor = xcb_generate_id(Connection);
    xcb_create_glyph_cursor(Connection, cursor, font, font, index, index + 1,
	0, 0, 0, 65535, 65535, 65535);

    return cursor;
}

/**
**	Prepare initialize pointers.
*/
void PointerPreInit(void)
{
    PointerCookie = PointerQueryRequest();
}

/**
**	Initialize pointers.
*/
void PointerInit(void)
{
    xcb_font_t font;
    unsigned u;

    // check if the order in Cursors changed
    IfDebug(if ((void *)&Cursors != &Cursors.Default
	    || (&Cursors.Choose - &Cursors.Default) + 1 !=
	    sizeof(Cursors) / sizeof(Cursors.Choose)) {
	FatalError("Order of elements in CursorTable changed\n");}
    ) ;

    font = xcb_generate_id(Connection);
    xcb_open_font(Connection, font, sizeof(CURSOR_FONT) - 1, CURSOR_FONT);

    for (u = 0; u < sizeof(Cursors) / sizeof(Cursors.Default); ++u) {
	// must fit to CursorTable
	static const uint32_t table[] = { XC_left_ptr, XC_fleur, XC_top_side,
	    XC_bottom_side, XC_right_side, XC_left_side, XC_top_right_corner,
	    XC_top_left_corner, XC_bottom_right_corner, XC_bottom_left_corner,
	    XC_tcross
	};
	((xcb_cursor_t *) & Cursors)[u] = CursorCreate(font, table[u]);
    }

    xcb_close_font(Connection, font);

    PointerQueryReply(PointerCookie);
}

/**
**	Cleanup the pointer module.
*/
void PointerExit(void)
{
    xcb_cursor_t *cursor;

    for (cursor = &Cursors.Default; cursor <= &Cursors.Choose; ++cursor) {
	xcb_free_cursor(Connection, *cursor);
	*cursor = XCB_NONE;
    }
}

/// @}
