///
///	@file systray.h @brief systray panel plugin header file.
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
///	@addtogroup systray_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Handle a client message sent to systray window.
extern void SystrayHandleClientMessageEvent(const xcb_client_message_event_t
    *);

    /// Intialize systray panel plugin.
extern void SystrayInit(void);

    /// Cleanup systray panel plugin.
extern void SystrayExit(void);

    /// Parse systray panel plugin config.
Plugin *SystrayConfig(const ConfigObject *);

#ifndef USE_SYSTRAY			// {

    /// Dummy for initialize systray(s).
#define SystrayInit()
    /// Dummy for cleanup systray(s).
#define SystrayExit()

#endif // } USE_SYSTRAY

/// @}
