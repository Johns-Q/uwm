///
///	@file draw.h	@brief drawing function header file
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

/// @addtogroup draw
/// @{

/// @addtogroup color
/// @{

// ------------------------------------------------------------------------ //
// Color
// ------------------------------------------------------------------------ //

//////////////////////////////////////////////////////////////////////////////
//	Declares
//////////////////////////////////////////////////////////////////////////////

/**
**	Color typedef
*/
typedef struct _color_ Color;

/**
**	Color structure
*/
struct _color_
{
    const char *Name;			///< color name
    char *Value;			///< color value name
    uint32_t Pixel;			///< x11 pixel value
    const char *Default;		///< default color value name
};

/**
**	Color table typedef
*/
typedef struct _color_table_ ColorTable;

/**
**	Color table structure
*/
struct _color_table_
{
    /// @warning TitleFG must be first element in ColorTable
    Color TitleFG;			///< window title foreground
    Color TitleBG1;			///< window title background start
    Color TitleBG2;			///< window title background end

    Color TitleActiveFG;		///< active window title foreground
    Color TitleActiveBG1;		///< active window title background 1
    Color TitleActiveBG2;		///< active window title background 2

    Color BorderLine;			///< window border
    Color BorderActiveLine;		///< active window border
    Color BorderCorner;			///< window corner (resize)
    Color BorderActiveCorner;		///< active window corner (resize)

    Color PanelFG;			///< panel foreground
    Color PanelBG;			///< panel background

    Color TaskFG;			///< task foreground
    Color TaskBG1;			///< task background 1
    Color TaskBG2;			///< task background 2
    Color TaskActiveFG;			///< active task foreground
    Color TaskActiveBG1;		///< active task background 1
    Color TaskActiveBG2;		///< active task background 2

    Color PagerFG;			///< pager foreground
    Color PagerBG;			///< pager background
    Color PagerActiveFG;		///< active pager foreground
    Color PagerActiveBG;		///< active pager background
    Color PagerOutline;			///< pager outline
    Color PagerText;			///< pager text

    Color MenuFG;			///< menu foreground
    Color MenuBG;			///< menu background
    Color MenuActiveFG;			///< active menu item foreground
    Color MenuActiveBG1;		///< active menu item background 1
    Color MenuActiveBG2;		///< active menu item background 2
    Color MenuActiveOutline;		///< active menu item outline

    // Colors below this point are calculated from the above values.

    Color TooltipFG;			///< tooltip foreground
    Color TooltipBG;			///< tooltip background
    Color TooltipOutline;		///< tooltip outline

    Color PanelButtonFG;		///< foreground of panel button
    Color PanelButtonBG;		///< background of panel button

    Color ClockFG;			///< clock plugin foreground
    Color ClockBG;			///< clock plugin background

    Color PanelUp;			///< panel 3d up color
    Color PanelDown;			///< panel 3d down color

    Color TaskUp;			///< task 3d up color
    Color TaskDown;			///< task 3d down color
    Color TaskActiveUp;			///< active task 3d up color
    Color TaskActiveDown;		///< active task 3d down color

    Color MenuUp;			///< menu 3d up color
    Color MenuDown;			///< menu 3d down color
    Color MenuActiveUp;			///< active menu item 3d up color
    /// @warning MenuActiveDown must be last element in ColorTable
    Color MenuActiveDown;		///< active menu item 3d down color
};

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern ColorTable Colors;		///< colors for all modules

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Compute the pixel value from RGB components.
extern void ColorGetPixel(xcb_coloritem_t *);

    /// Parse a color for a component.
extern int ColorParse(const char *, xcb_coloritem_t *);

    /// Get the RGB components from a pixel value.
extern void ColorGetFromPixel(xcb_coloritem_t *);

    /// Get a RGB pixel value from RGB components.
extern void ColorGetIndex(xcb_coloritem_t *);

    /// Intialize color module.
extern void ColorInit(void);

    /// Cleanup color module.
extern void ColorExit(void);

#ifdef USE_LUA
    /// Set the color to use for a modul.
extern void ColorSet(const char *, const char *);
#else
    /// Parse the color config.
extern void ColorConfig(void);
#endif

/// @}
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
    size_t, const char *);
    /// Font width of string.
extern int FontTextWidthReply(xcb_query_text_extents_cookie_t);

    /// Display an utf8 string.
extern void FontDrawUtf8(xcb_drawable_t, Font *, uint32_t, int, int, unsigned,
    xcb_rectangle_t * region, const char *);
    /// Display a string.
extern void FontDrawString(xcb_drawable_t, Font *, uint32_t, int, int,
    unsigned, xcb_rectangle_t * region, const char *);

    /// Pre-initialize font support.
extern void FontPreInit(void);

    /// Initialize font support.
extern void FontInit(void);

    /// Cleanup font support.
extern void FontExit(void);

#ifdef USE_LUA
    /// Set the font to use for a component.
extern void FontSet(const char *, const char *);
#else
    /// Parse the font config.
extern void FontConfig(void);
#endif

/// @}

/// @addtogroup gradient
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Draw a horizontal gradient.
void GradientDrawHorizontal(xcb_drawable_t, xcb_gcontext_t, uint32_t, uint32_t,
    int, int, unsigned, unsigned);

/// @}

/// @addtogroup tooltip
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Variables
//////////////////////////////////////////////////////////////////////////////

extern int TooltipDelay;		///< tooltip delay in milliseconds

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Show a tooltip window.
extern void TooltipShow(int, int, const char *);

/// @}

/// @}
