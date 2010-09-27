///
///	@file tooltip.h		@brief tooltip header file
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

/// @ingroup draw
/// @addtogroup tooltip
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern int TooltipDelay;		///< tooltip delay in milliseconds

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Show tooltip window.
extern void TooltipShow(int, int, const char *);

    /// Hide tooltip.
extern void TooltipHide(void);

    /// Handle an expose event on tooltip window.
extern int TooltipHandleExpose(const xcb_expose_event_t *);

    /// Timeout tooltip (this is used to hide tooltips after movement).
extern void TooltipTimeout(uint32_t, int, int);

    /// Initialize the tooltip module.
extern void TooltipInit(void);

    /// Cleanup the tooltip module.
extern void TooltipExit(void);

    /// Parse tooltip configuration.
extern void TooltipConfig(const Config *);

    /// Initialize the tooltip module (noop)
#define TooltipInit()	do { } while(0)

/// @}
