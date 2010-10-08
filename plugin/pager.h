///
///	@file pager.h	@brief pager panel plugin header file.
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
///	@addtogroup pager_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Update all pager plugin(s).
extern void PagerUpdate(void);

    /// Intialize pager panel plugin.
extern void PagerInit(void);

    /// Cleanup pager panel plugin.
extern void PagerExit(void);

    /// Parse pager panel plugin configuration.
Plugin *PagerConfig(const ConfigObject *);

    /// Dummy for initialize pager(s).
#define PagerInit()

#ifndef USE_PAGER			// {
    /// Dummy for update all pager plugin(s).
#define PagerUpdate()
    /// Dummy for cleanup pager(s).
#define PagerExit()
    /// Dummy parse pager panel plugin configuration.
#define PagerConfig(config)	NULL

#endif // } !USE_PAGER

/// @}
