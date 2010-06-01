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
extern void StatusConfig(void);

#else

    /// Dummy for parse status window configuration.
#define StatusConfig()

#endif

/// @}
