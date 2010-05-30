///
///	@file event.h		@brief x11 event handler header file
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

/// @addtogroup event
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern int DoubleClickDelta;		///< maximal movement for double click
extern int DoubleClickSpeed;		///< maximal time to detect double click

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Wait for event.
extern void WaitForEvent(void);

    /// Poll for next event.
extern xcb_generic_event_t *PollNextEvent(void);

    /// Look if there is a event.
extern int IsNextEventAvail(void);

    /// Peek window event.
extern xcb_generic_event_t *PeekWindowEvent(xcb_window_t, int);

    /// Discard further motion events on same window.
extern void DiscardMotionEvents(xcb_motion_notify_event_t **, xcb_window_t);

    /// Main event loop.
extern void EventLoop(void);

    /// Initialize the event module.
extern void EventInit(void);

    /// Cleanup the event module.
extern void EventExit(void);

/// @}
