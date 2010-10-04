///
///	@file keyboard.h @brief keyboard header file
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

/// @addtogroup keyboard
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Grab keyboard, send request.
extern xcb_grab_keyboard_cookie_t KeyboardGrabRequest(xcb_window_t);

    /// Grab keyboard, read reply.
extern int KeyboardGrabReply(xcb_grab_keyboard_cookie_t);

    /// Get keysym from event.
extern xcb_keysym_t KeyboardGet(xcb_keycode_t, unsigned);

    /// Grab our key bindings on client window.
extern void KeyboardGrabBindings(Client *);

    /// Key pressed or released.
extern void KeyboardHandler(int, unsigned, unsigned);

    /// Initialize the keyboard module.
extern void KeyboardInit(void);

    /// Cleanup the keyboard module.
extern void KeyboardExit(void);

    /// Parse keyboard configuration.
extern void KeyboardConfig(const Config * config);

/// @}
