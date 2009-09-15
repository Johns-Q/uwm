///
///	@file swallow.h @brief swallow panel plugin header file.
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

///	@ingroup panel
///	@addtogroup swallow_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Determine if a window should be swallowed.
extern int SwallowTryWindow(int, xcb_window_t);

    /// Determine if a map event was for a window that should be swallowed.
extern int SwallowHandleMapRequest(const xcb_map_request_event_t *);

    /// Handle a destroy notify.
extern int SwallowHandleDestroyNotify(const xcb_destroy_notify_event_t *);

    /// Handle a configure notify.
int SwallowHandleConfigureNotify(const xcb_configure_notify_event_t *);

    /// Handle a resize request.
int SwallowHandleResizeRequest(const xcb_resize_request_event_t *);

    /// Intialize swallow panel plugin.
extern void SwallowInit(void);

    /// Cleanup swallow panel plugin.
extern void SwallowExit(void);

    /// Parse swallow panel plugin config.
Plugin *SwallowConfig(const ConfigObject *);

#ifndef USE_SWALLOW			// {

    /// Dummy for initialize swallow(s).
#define SwallowInit()
    /// Dummy for cleanup swallow(s).
#define SwallowExit()

#endif // } USE_SWALLOW

/// @}
