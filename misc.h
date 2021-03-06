///
///	@file misc.h		@brief misc function header file
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

/// @addtogroup misc
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Get ticks in ms.
extern uint32_t GetMsTicks(void);

    /// Expand a path.
extern char *ExpandPath(const char *);

    /// Draw rounded rectangle.
extern void xmu_draw_rounded_rectangle(xcb_connection_t *, xcb_drawable_t,
    xcb_gcontext_t, int, int, int, int, int, int);

    //	Fill rounded rectangle.
extern void xmu_fill_rounded_rectangle(xcb_connection_t *, xcb_drawable_t,
    xcb_gcontext_t, int, int, int, int, int, int);

/// @}
