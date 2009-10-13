///
///	@file desktop.h @brief desktop management header file
///
///	Copyright (c) 2009 by Lutz Sammer.  All Rights Reserved.
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

/// @addtogroup desktop
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern int DesktopN;			///< number of desktops
extern int DesktopCurrent;		///< current desktop

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

extern void DesktopChange(int);		///< Change to the specified desktop.
extern void DesktopNext(void);		///< Change to the next desktop.
extern void DesktopPrevious(void);	///< Change to the previous desktop.

extern const char *DesktopGetName(int);	///< Get the name of a desktop.

    /// Create a desktop menu.
extern Menu *DesktopCreateMenu(unsigned);

extern void DesktopToggleShow(void);	///< Toggle the "show desktop" state.
extern void DesktopToggleShade(void);	///< Toggle the "shade desktop" state.

extern void DesktopInit(void);		///< Initialize desktop support.
extern void DesktopExit(void);		///< Cleanup desktop support.

extern void DesktopConfig(void);	///< Parse config for desktop module.

/// @}
