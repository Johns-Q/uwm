///
///	@file systray.h		@brief systray panel plugin header file.
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

///	@ingroup panel
///	@addtogroup systray_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Handle a resize request event.
extern int SystrayHandleResizeRequest(const xcb_resize_request_event_t *);

    /// Handle a configure request.
extern int SystrayHandleConfigureRequest(const xcb_configure_request_event_t
    *);

    /// Handle a reparent notify event.
extern int SystrayHandleReparentNotify(const xcb_reparent_notify_event_t *);

    /// Handle a destroy event.
extern int SystrayHandleDestroyNotify(xcb_window_t);

    /// Handle a selection clear event.
extern int SystrayHandleSelectionClear(const xcb_selection_clear_event_t *);

    /// Handle a client message sent to systray window.
extern void SystrayHandleClientMessageEvent(const xcb_client_message_event_t
    *);
    /// Handle a destroy event.

    /// Initialize systray panel plugin.
extern void SystrayInit(void);

    /// Cleanup systray panel plugin.
extern void SystrayExit(void);

    /// Parse systray panel plugin configuration.
Plugin *SystrayConfig(const ConfigObject *);

#ifndef USE_SYSTRAY			// {

    /// Dummy for handle a client message sent to systray window.
#define SystrayHandleClientMessageEvent(event)
    /// Dummy for initialize systray panel plugin.
#define SystrayInit()
    /// Dummy for cleanup systray panel plugin.
#define SystrayExit()
    /// Dummy for parse systray panel plugin configuration.
#define SystrayConfig(object)	NULL

#endif // } USE_SYSTRAY

/// @}
