///
///	@file icon.h	@brief icon handling header file
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

/// @addtogroup icon
/// @{

#ifdef USE_ICON				// {

//////////////////////////////////////////////////////////////////////////////
//	Defines
//////////////////////////////////////////////////////////////////////////////

#define ICON_HASH_SIZE 128		///< icon hash table size (power of 2)!

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Scaled icon typedef.
*/
typedef struct _scaled_icon_ ScaledIcon;

/**
**	Icon typedef.
*/
typedef struct _icon_ Icon;

/**
**	Icon structure.
*/
struct _icon_
{
    LIST_ENTRY(_icon_) Node;		///< list of icons in hash

    char *Name;				///< name of icon
    Image *Image;			///< image data

#ifdef USE_RENDER
    uint32_t UseRender;			///< if render can be used
#endif
    int RefCnt;				///< reference counter

    /// scaled icon cache
     SLIST_HEAD(_scaled_head_, _scaled_icon_) Scaled;
};

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Draw an icon.
extern void IconDraw(Icon *, xcb_drawable_t, int, int, unsigned, unsigned);

    /// Destroy an icon.
extern void IconDel(Icon *);

    /// Load an icon given a path, name, and suffix.
extern Icon *IconLoadSuffixed(const char *, const char *, const char *);

    /// Load an icon from a file.
extern Icon *IconLoadNamed(const char *);

    /// Load an icon for a client.
extern void IconLoadClient(Client *);

    /// Initialize icon module.
extern void IconInit(void);

    /// Cleanup icon module.
extern void IconExit(void);

    /// Parse icon module configuration.
extern void IconConfig(const Config *);

#else // }{ USE_ICON

    /// Dummy for initialize icon module.
#define IconInit()
    /// Dummy for cleanup icon module.
#define IconExit()

    /// Dummy for parse icon module configuration.
#define IconConfig()

#endif // } !USE_ICON

/// @}
