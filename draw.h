///
///	@file draw.h	@brief drawing function header file
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

#ifdef USE_MENU
    Color MenuFG;			///< menu foreground
    Color MenuBG;			///< menu background
    Color MenuOutline;			///< menu item outline
    Color MenuActiveFG;			///< active menu item foreground
    Color MenuActiveBG1;		///< active menu item background 1
    Color MenuActiveBG2;		///< active menu item background 2
    Color MenuActiveOutline;		///< active menu item outline
#endif

#ifdef USE_TOOLTIP
    Color TooltipFG;			///< tooltip foreground
    Color TooltipBG;			///< tooltip background
    Color TooltipOutline;		///< tooltip outline
#endif

#ifdef USE_PANEL
    Color PanelFG;			///< panel foreground
    Color PanelBG;			///< panel background
#endif

#ifdef USE_TASK
    Color TaskFG;			///< task foreground
    Color TaskBG1;			///< task background 1
    Color TaskBG2;			///< task background 2
    Color TaskActiveFG;			///< active task foreground
    Color TaskActiveBG1;		///< active task background 1
    Color TaskActiveBG2;		///< active task background 2
#endif

#ifdef USE_PAGER
    Color PagerFG;			///< pager foreground
    Color PagerBG;			///< pager background
    Color PagerActiveFG;		///< active pager foreground
    Color PagerActiveBG;		///< active pager background
    Color PagerOutline;			///< pager outline
    Color PagerText;			///< pager text
#endif

#ifdef USE_BUTTON
    Color PanelButtonFG;		///< foreground of panel button
    Color PanelButtonBG;		///< background of panel button
#endif

#ifdef USE_CLOCK
    Color ClockFG;			///< clock plugin foreground
    Color ClockBG;			///< clock plugin background
#endif

#ifdef USE_NETLOAD
    Color NetloadFG;			///< netload plugin foreground
    Color NetloadBG;			///< netload plugin background
    Color NetloadRX;			///< netload plugin RX color
    Color NetloadTX;			///< netload plugin TX color
#endif

    // Colors below this point are calculated from the above values.

#ifdef USE_MENU
    Color MenuUp;			///< menu 3d up color
    Color MenuDown;			///< menu 3d down color
    Color MenuActiveUp;			///< active menu item 3d up color
    Color MenuActiveDown;		///< active menu item 3d down color
#endif

#ifdef USE_PANEL
    Color PanelUp;			///< panel 3d up color
    Color PanelDown;			///< panel 3d down color
#endif

#ifdef USE_TASK
    Color TaskUp;			///< task 3d up color
    Color TaskDown;			///< task 3d down color
    Color TaskActiveUp;			///< active task 3d up color
    Color TaskActiveDown;		///< active task 3d down color
#endif
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

    /// Initialize color module.
extern void ColorInit(void);

    /// Cleanup color module.
extern void ColorExit(void);

    /// Parse the color configuration.
extern void ColorConfig(const Config *);

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
**
**	@warning #Fonts.Titlebar must be first font.
**	@warning #Fonts.Fallback must be last font.
*/
typedef struct _fonts_
{
    Font Titlebar;			///< titlebar font
#ifdef USE_MENU
    Font Menu;				///< menu font
#endif
#ifdef USE_TOOLTIP
    Font Tooltip;			///< tooltip font
#endif
#ifdef USE_PANEL
    Font Panel;				///< panel font
#endif
#ifdef USE_TASK
    Font Task;				///< task font
#endif
#ifdef USE_PAGER
    Font Pager;				///< pager font
#endif
#ifdef USE_BUTTON
    Font PanelButton;			///< panel button font
#endif
#ifdef USE_CLOCK
    Font Clock;				///< clock font
#endif
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

    /// Display a string.
extern void FontDrawString(xcb_drawable_t, const Font *, uint32_t, int, int,
    unsigned, xcb_rectangle_t * region, const char *);

    /// Pre-initialize font support.
extern void FontPreInit(void);

    /// Initialize font support.
extern void FontInit(void);

    /// Cleanup font support.
extern void FontExit(void);

    /// Parse the font configuration.
extern void FontConfig(const Config *);

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

/// @}
