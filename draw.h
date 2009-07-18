///
///	@file draw.h	@brief drawing function header file
///
///	Copyright (c) 2009 by Johns.  All Rights Reserved.
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

/// @addtogroup draw
/// @{

/// @addtogroup font
/// @{

// ------------------------------------------------------------------------ //
// Font
// ------------------------------------------------------------------------ //

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Font typedef
*/
typedef struct _font_ Font;

/**
**	Font structure
*/
struct _font_
{
    const char *ModuleName;		///< module name
    char *FontName;			///< font name

    xcb_font_t Font;			///< x11 font id
    int16_t Ascent;			///< font ascent
    int16_t Height;			///< font height
    union
    {
	xcb_void_cookie_t Cookie;	///< xcb request cookie
	xcb_query_font_cookie_t QCookie;	///< xcb request cookie
    };
};

/**
**	This structure contains all fonts.
*/
typedef struct _fonts_
{
    Font Titlebar;			///< titlebar font
    Font Menu;				///< menu font
    Font Task;				///< task font
    Font Tooltip;			///< tooltip font
    Font Clock;				///< clock font
    Font Panel;				///< panel font
    Font PanelButton;			///< panel button font
    Font Pager;				///< pager font
    Font Fallback;			///< fallback font
} FontTable;

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern FontTable Fonts;			///< contains all our used fonts

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Send query for text extents of string.
extern xcb_query_text_extents_cookie_t FontQueryExtentsRequest(const Font *,
    int, const char *);
    /// Get font width of string.
extern int FontGetTextWidth(xcb_query_text_extents_cookie_t, unsigned *);

    /// Display a string.
extern void FontDrawString(xcb_drawable_t, Font *, uint32_t, int, int,
    unsigned, xcb_rectangle_t * region, const char *);

    /// Pre-initialize font support.
extern void FontPreInit(void);

    /// Initialize font support.
extern void FontInit(void);

    /// Cleanup font support.
extern void FontExit(void);

    /// Set the font to use for a component.
extern void FontSet(const char *, const char *);

/// @}
/// @}
