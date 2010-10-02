///
///	@file dia.h	@brief dia show application header file
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

/// @addtogroup dia
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Create/destroy the dia-show window.
extern void DiaCreate(const char *);

    /// Handle an expose event.
extern int DiaHandleExpose(const xcb_expose_event_t *);

    /// Handle a mouse button release event.
extern int DiaHandleButtonPress(const xcb_button_press_event_t *);

    /// Handle a button release event.
extern int DiaHandleButtonRelease(const xcb_button_release_event_t *);

    /// Timeout for dia-show.
extern void DiaTimeout(uint32_t, int, int);

    // Initialize dia-show module.
//extern void DiaInit(void);
    // Cleanup dia-show module.
//extern void DiaExit(void);

    /// Parse dia-show configuration.
extern void DiaConfig(const Config *);

#ifndef USE_DIA

    /// Dummy for create/destroy the dia-show window.
#define DiaCreate(x)

    /// Dummy for handle an expose event.
#define DiaHandleExpose(x)	0
    /// Dummy for handle a mouse button release event.
#define DiaHandleButtonPress(x)	0
    /// Dummy for handle a button release event.
#define DiaHandleButtonRelease(x)	0

    /// Dummy for timeout for dia-show.
#define DiaTimeout(x, y, z);

    /// Dummy for parse dia-show configuration.
#define DiaConfig(config)

#endif

/// @}
