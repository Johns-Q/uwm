///
///	@file pointer.h		@brief Pointer/Cursor header file
///
///	Copyright (c) 2009 by Lutz Sammer.  All Rights Reserved.
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

/// @addtogroup pointer
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Cursor table typedef.
*/
typedef struct _cursor_table_ CursorTable;

/**
**	Cursor table structure.
*/
struct _cursor_table_
{
    xcb_cursor_t Default;		///< default cursor
    xcb_cursor_t Move;			///< move cursor
    xcb_cursor_t North;			///< north cursor
    xcb_cursor_t South;			///< south cursor
    xcb_cursor_t East;			///< east cursor
    xcb_cursor_t West;			///< west cursor
    xcb_cursor_t NorthEast;		///< north-east cursor
    xcb_cursor_t NorthWest;		///< north-west cursor
    xcb_cursor_t SouthEast;		///< south-east cursor
    xcb_cursor_t SouthWest;		///< south-west cursor
    xcb_cursor_t Choose;		///< choose cursor
};

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern CursorTable Cursors;		///< all cursors

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Set current pointer position.
extern void PointerSetPosition(int, int);

    /// Get current pointer position.
extern void PointerGetPosition(int *, int *);

    /// Get current button mask.
extern xcb_button_mask_t PointerGetButtonMask(void);

    /// Grab pointer request.
extern xcb_grab_pointer_cookie_t PointerGrabRequest(xcb_window_t,
    xcb_cursor_t);
    /// Grab pointer reply.
extern int PointerGrabReply(xcb_grab_pointer_cookie_t);

    /// Request pointer grab with default cursor.
extern xcb_grab_pointer_cookie_t PointerGrabDefaultRequest(xcb_window_t);

    /// Request pointer grab for moving a window.
extern xcb_grab_pointer_cookie_t PointerGrabForMoveRequest(void);

    /// Request pointer grab for choosing a window.
extern xcb_grab_pointer_cookie_t PointerGrabForChooseRequest(void);

    /// Request pointer grab for resizing window.
extern xcb_grab_pointer_cookie_t PointerGrabForResizeRequest(int);

    /// Get query pointer reply.
extern void PointerQueryReply(xcb_query_pointer_cookie_t);

    /// Query pointer.
extern void PointerQuery(void);

    /// Wrap pointer
extern void PointerWrap(xcb_window_t, int, int);

    /// Set default cursor for pointer.
extern void PointerSetDefaultCursor(xcb_window_t);

    /// Prepare initialize pointers.
extern void PointerPreInit(void);

    /// Initialize pointers.
extern void PointerInit(void);

    /// Cleanup the pointer module.
extern void PointerExit(void);

/// @}
