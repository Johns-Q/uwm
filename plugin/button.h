///
///	@file button.h	@brief button panel plugin header file.
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
///	@addtogroup button_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Update desktop change.
extern void PanelButtonDesktopUpdate(void);

    /// Intialize button panel plugin.
extern void PanelButtonInit(void);

    /// Cleanup button panel plugin.
extern void PanelButtonExit(void);

    /// Parse button panel plugin configuration.
Plugin *PanelButtonConfig(const ConfigObject *);

#ifndef USE_BUTTON			// {

    /// Dummy for update desktop change.
#define PanelButtonDesktopUpdate()
    /// Dummy for initialize button(s).
#define PanelButtonInit()
    /// Dummy for cleanup button(s).
#define PanelButtonExit()
    /// Dummy for parse button panel plugin configuration.
#define PanelButtonConfig(o)	NULL

#endif // } USE_BUTTON

/// @}
