///
///	@file netload.h @brief netload panel plugin header file.
///
///	Copyright (c) 2010 by Lutz Sammer.  All Rights Reserved.
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
///	@addtogroup netload_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Initialize netload panel plugin.
extern void NetloadInit(void);

    /// Cleanup netload panel plugin.
extern void NetloadExit(void);

    /// Parse netload panel plugin configuration.
Plugin *NetloadConfig(const ConfigObject *);

#ifndef USE_NETLOAD			// {

    /// Dummy for initialize netload planel plugin.
#define NetloadInit()
    /// Dummy for cleanup netload planel plugin.
#define NetloadExit()
    /// Dummy for parse netload panel plugin configuration.
#define NetloadConfig(o)	NULL

#endif // } USE_NETLOAD

/// @}
