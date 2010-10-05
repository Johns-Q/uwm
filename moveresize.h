///
///	@file moveresize.h	@brief move/resize window header file.
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

/// @ingroup client
/// @addtogroup status
/// @{

// ------------------------------------------------------------------------ //
// Status
// ------------------------------------------------------------------------ //

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

#ifdef USE_STATUS

    /// Parse status window configuration.
extern void StatusConfig(const Config *);

#else

    /// Dummy for parse status window configuration.
#define StatusConfig()

#endif

/// @}

/// @ingroup client
/// @addtogroup outline
/// @{

// ------------------------------------------------------------------------ //
// Outline
// ------------------------------------------------------------------------ //

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Initialize the outline module
extern void OutlineInit(void);

    /// Cleanup the outline module
extern void OutlineExit(void);

    /// Parse outline configuration.
extern void OutlineConfig(const Config *);

#ifndef USE_OUTLINE

    /// Dummy for Initialize outline module.
#define OutlineInit()
    /// Dummy for Cleanup outline module.
#define OutlineExit()

#endif

    /// Dummy for parse outline configuration.
#define OutlineConfig(config)

/// @}
