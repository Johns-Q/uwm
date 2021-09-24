///
///	@file border.h		@brief client window border header file
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

/// @ingroup client
/// @addtogroup border
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

#define BORDER_BUTTON_WIDTH 16		///< width of border button icons
#define BORDER_BUTTON_HEIGHT 16		///< height of border button icons

/**
**	Flags to determine what action to take on border.
*/
typedef enum
{
    BORDER_ACTION_NONE,			///< do nothing
    /*
       BORDER_ACTION_LOWER,		///< lower window
       BORDER_ACTION_RAISE,		///< raise window
     */
    BORDER_ACTION_RESIZE,		///< resize window
    BORDER_ACTION_MOVE,			///< move window
    BORDER_ACTION_CLOSE,		///< close window
    BORDER_ACTION_MAXIMIZE,		///< toggle maximize window
    BORDER_ACTION_MINIMIZE,		///< minimize window
    BORDER_ACTION_STICKY,		///< toggle window sticky
    BORDER_ACTION_MENU,			///< show window menu
    BORDER_ACTION_MASK = 0x0F,		///< mask for above commands
    BORDER_ACTION_RESIZE_N = 0x10,	///< resize north flag
    BORDER_ACTION_RESIZE_S = 0x20,	///< resize south flag
    BORDER_ACTION_RESIZE_E = 0x40,	///< resize east flag
    BORDER_ACTION_RESIZE_W = 0x80	///< resize west flag
} BorderAction;

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Get size of borders for a client.
extern void BorderGetSize(const Client *, int *, int *, int *, int *);

    /// Get size of title border.
extern int BorderGetTitleSize(void) __attribute__((pure));

    /// Draw client window border.
extern void BorderDraw(const Client *, const xcb_expose_event_t *);

    /// Get cursor for specified action on frame.
extern xcb_cursor_t BorderGetCursor(BorderAction);

    /// Determine border action to take given coordinates.
extern BorderAction BorderGetAction(const Client *, int, int);

    /// Take appropriate action for click on client border.
extern void BorderHandleButtonPress(Client *,
    const xcb_button_press_event_t *);

    /// Show window menu relative to border.
extern void BorderShowMenu(Client *, int, int);

    /// Initialize border module.
extern void BorderInit(void);

    /// Cleanup border module.
extern void BorderExit(void);

    /// Parse window border configuration.
extern void BorderConfig(const Config *);

#if defined(USE_SHAPE) && defined(USE_XMU)	// {

    /// Clear shape mask of a window.
extern void ShapeRoundedRectReset(xcb_window_t);

    /// Set shape mask on window to give rounded border.
extern void ShapeRoundedRectWindow(xcb_window_t, unsigned, unsigned);

    /// Subtract shape mask on window to give rounded border.
extern void ShapeRoundedRectSubtract(xcb_window_t, unsigned, unsigned);

#else // }{ USE_SHAPE && USE_XMU

    /// Dummy for clear shape mask of a window.
#define ShapeRoundedRectReset(window)
    /// Dummy for set shape mask on window to give rounded border.
#define ShapeRoundedRectWindow(window, width, height)
    /// Dummy for subtract shape mask on window to give rounded border.
#define ShapeRoundedRectSubtract(window, width, height)

#endif // } !(USE_SHAPE && USE_XMU)

/// @}
