///
///	@file screen.h @brief physical screen management header file
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

/// @addtogroup screen
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Screen typedef.
*/
typedef struct _screen_ Screen;

/**
**	Screen structure.
**
**	Screens are physical monitors.	There will be atleast one screen.
*/
struct _screen_
{
    int16_t X;				///< x-coordinate of screen
    int16_t Y;				///< y-coordinate of screen
    uint16_t Width;			///< width of screen
    uint16_t Height;			///< height of screen
};

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern Screen *Screens;			///< all available screens
extern int ScreenN;			///< number of screens

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Get screen at global screen coordinates.
extern const Screen *ScreenGetByXY(int, int);

    /// Get screen mouse is currently on.
extern const Screen *ScreenGetPointer(void);

    /// Initialize screen.
extern void ScreenInit(void);

    /// Cleanup the screen / xinerama module.
extern void ScreenExit(void);

/// @}
