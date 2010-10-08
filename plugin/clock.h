///
///	@file clock.h	@brief clock panel plugin header file.
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
///	@addtogroup clock_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Intialize clock panel plugin.
extern void ClockInit(void);

    /// Cleanup clock panel plugin.
extern void ClockExit(void);

    /// Parse clock panel plugin configuration
Plugin *ClockConfig(const ConfigObject *);

#ifndef USE_CLOCK			// {

    /// Dummy for initialize clock(s).
#define ClockInit()
    /// Dummy for cleanup clock(s).
#define ClockExit()
    /// Dummy for parse clock panel plugin config
#define ClockConfig(o)	NULL

#endif // } USE_CLOCK

/// @}
