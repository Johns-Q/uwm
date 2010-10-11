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

// ------------------------------------------------------------------------ //
// Status
// ------------------------------------------------------------------------ //

/// @ingroup client
/// @addtogroup status
/// @{

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

// ------------------------------------------------------------------------ //
// Outline
// ------------------------------------------------------------------------ //

/// @ingroup client
/// @addtogroup outline
/// @{

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
    /// Dummy for parse outline configuration.
#define OutlineConfig(config)

#endif

/// @}

// ------------------------------------------------------------------------ //
// Client snap
// ------------------------------------------------------------------------ //

/// @ingroup client
/// @addtogroup snap
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Snap to screen and/or neighboring windows.
extern void ClientSnap(Client * client);

    /// Parse snap configuration.
extern void SnapConfig(const Config *);

/// @}

// ------------------------------------------------------------------------ //
// Client move
// ------------------------------------------------------------------------ //

/// @ingroup client
/// @addtogroup move
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Interactive move client window.
extern int ClientMoveLoop(Client *, int, int, int);

    /// Move client window (keyboard or menu initiated).
extern int ClientMoveKeyboard(Client *);

/// @}

// ------------------------------------------------------------------------ //
// Client resize
// ------------------------------------------------------------------------ //

/// @ingroup client
/// @addtogroup resize
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Interactive resize client window.
extern void ClientResizeLoop(Client *, int, int, int, int);

    /// Resize client window (keyboard or menu initiated).
extern void ClientResizeKeyboard(Client *);

/// @}
