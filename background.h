///
///	@file background.h	@brief background control header file
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

/// @addtogroup background
/// @{

#ifdef USE_BACKGROUND			// {

    /// Load background for specified desktop.
extern void BackgroundLoad(int);

    /// Initialize background support.
extern void BackgroundInit(void);

    /// Cleanup background support.
extern void BackgroundExit(void);

    /// Parse config for background module.
extern void BackgroundConfig(void);

#else // }{ USE_BACKGROUND

    /// Dummy for load background for specified desktop.
#define BackgroundLoad(desktop)

    /// Dummy for initialize background module.
#define BackgroundInit()
    /// Dummy for cleanup background module.
#define BackgroundExit()

    /// Dummy for parse config for background module.
#define BackgroundConfig()

#endif // } !USE_BACKGROUND

/// @}
