///
///	@file button.c	@brief button panel plugin functions.
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

///
///	@ingroup panel
///	@defgroup button_plugin The button panel plugin
///
///	This module add buttons to the panel.  Buttons shows an icon and/or
///	text.  Commands can be configured to be run on pointer button clicks.
///
///	@todo desktop name is shown if icon and text of button aren't set.
///	This is a hack. Add desktop name plugin?
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_BUTTON			// {

#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb_icccm.h>

#include "array.h"
#include "config.h"

#include "draw.h"
#include "tooltip.h"
#include "screen.h"
#include "image.h"
#include "pointer.h"
#include "client.h"

#include "icon.h"
#include "menu.h"
#include "desktop.h"
#include "panel.h"
#include "plugin/button.h"

// ------------------------------------------------------------------------ //

#define BUTTON_BORDER	1		///< border around button

/**
**	Panel button plugin typedef.
*/
typedef struct _button_plugin_ ButtonPlugin;

/**
**	Panel button plugin structure.
*/
struct _button_plugin_
{
    SLIST_ENTRY(_button_plugin_) Next;	///< singly-linked button list
    Plugin *Plugin;			///< common plugin data

    char *Text;				///< button text
    char *Tooltip;			///< tooltip shown, if mouse over

#ifdef USE_ICON
    union
    {
	char *IconName;			///< icon file name
	Icon *Icon;			///< loaded/cached icon
    };
    unsigned IconLoaded:1;		///< flag icon loaded
    unsigned IconOrText:1;		///< flag display icon or text
#endif

    unsigned UserWidth:1;		///< user-specified width flag
    unsigned UserHeight:1;		///< user-specified height flag

    MenuButton *Buttons;		///< commands to run on click

    /// cookie for label width query
    xcb_query_text_extents_cookie_t Cookie;
};

    /// Button plugin list head structure
SLIST_HEAD(_button_head_, _button_plugin_);
    //
    /// List of all buttons of the plugin
static struct _button_head_ Buttons = SLIST_HEAD_INITIALIZER(Buttons);

// ------------------------------------------------------------------------ //
// Drawing

/**
**	Draw a panel plugin button.
**
**	@param plugin	common plugin data
**	@param active	flag draw active button
*/
static void PanelButtonDraw(Plugin * plugin, int active)
{
    Label label;
    const ButtonPlugin *button_plugin;

    PanelClearPluginBackgroundWithColor(plugin, &Colors.PanelButtonBG.Pixel);

    LabelReset(&label, plugin->Pixmap, RootGC);

    label.Type = active ? LABEL_PANEL_ACTIVE : LABEL_PANEL;
    label.X = PANEL_INNER_SPACE;
    label.Y = PANEL_INNER_SPACE;
    label.Width = plugin->Width - PANEL_INNER_SPACE * 2;
    label.Height = plugin->Height - PANEL_INNER_SPACE * 2;

    button_plugin = plugin->Object;
    label.Text = button_plugin->Text;
    label.Font = &Fonts.PanelButton;
#ifdef USE_ICON
    label.Icon = button_plugin->Icon;
    if (button_plugin->IconOrText && label.Icon) {
	label.Text = NULL;
    }
#endif
    // FIXME: hack current work space name
    if (!label.Icon && !label.Text) {
	label.Text = DesktopGetName(DesktopCurrent);
    }

    LabelDraw(&label);
}

// ------------------------------------------------------------------------ //
// Callbacks

/**
**	Create/initialize a button panel plugin.
**
**	@param plugin	panel plugin common data
*/
static void PanelButtonCreate(Plugin * plugin)
{
    PanelPluginCreatePixmap(plugin);
    PanelButtonDraw(plugin, 0);
}

#ifdef USE_ICON

/**
**	Set the size of a button panel plugin.
**
**	@param plugin	common panel plugin data of button
**	@param width	width of plugin
**	@param height	height of plugin
*/
static void PanelButtonSetSize(Plugin * plugin, unsigned width,
    unsigned height)
{
    ButtonPlugin *button_plugin;

    button_plugin = plugin->Object;
    if (button_plugin->Icon) {		// only if icon
	unsigned label_width;
	unsigned label_height;
	unsigned icon_width;
	unsigned icon_height;
	double ratio;

	if (button_plugin->Text) {
	    button_plugin->Cookie =
		FontQueryExtentsRequest(&Fonts.PanelButton,
		strlen(button_plugin->Text), button_plugin->Text);
	}

	icon_width = button_plugin->Icon->Image->Width;
	icon_height = button_plugin->Icon->Image->Height;
	ratio = (double)icon_width / icon_height;

	label_width = 0;
	label_height = 0;
	if (!button_plugin->IconOrText && button_plugin->Text) {
	    label_width = FontTextWidthReply(button_plugin->Cookie);
	    // space between icon + text
	    label_width += LABEL_INNER_SPACE;
	    label_height = Fonts.PanelButton.Height;
	}
	if (button_plugin->UserHeight) {
	    label_height = plugin->RequestedHeight;
	}

	if (width > 0) {		// compute height from width
	    icon_width =
		width - label_width - 2 * LABEL_INNER_SPACE - 2 * LABEL_BORDER;
	    icon_height = icon_width / ratio;
	    height = MAX(icon_height, label_height)
		+ 2 * LABEL_INNER_SPACE + 2 * LABEL_BORDER;

	} else if (height > 0) {	// compute width from height
	    icon_height = height - 2 * LABEL_INNER_SPACE - 2 * LABEL_BORDER;
	    icon_width = icon_height * ratio;
	    width =
		icon_width + label_width + 2 * LABEL_INNER_SPACE +
		2 * LABEL_BORDER;
	}
	if (button_plugin->UserHeight) {
	    height = plugin->RequestedHeight;
	}
	if (button_plugin->UserWidth) {
	    width = plugin->RequestedWidth;
	}
	plugin->Width = width;
	plugin->Height = height;
    }
}
#endif

/**
**	Resize a button panel plugin.
**
**	@param plugin	panel plugin
*/
static void PanelButtonResize(Plugin * plugin)
{
    PanelPluginDeletePixmap(plugin);
    PanelButtonCreate(plugin);
}

/**
**	Handle a click event on a button panel plugin.
**
**	@param plugin	panel plugin
**	@param x	x-coordinate of button press
**	@param y	y-coordinate of button press
**	@param mask	button mask
*/
static void PanelButtonHandleButtonPress(Plugin * plugin, int
    __attribute__ ((unused)) x, int __attribute__ ((unused)) y, int
    __attribute__ ((unused)) mask)
{
    ButtonPlugin *button_plugin;

    PanelButtonDraw(plugin, 1);
    PanelUpdatePlugin(plugin->Panel, plugin);

    button_plugin = plugin->Object;
    PanelExecuteButton(plugin, button_plugin->Buttons, mask);

    PanelButtonDraw(plugin, 0);
    PanelUpdatePlugin(plugin->Panel, plugin);

#if 0
    plugin->Grabbed =
	PointerGrabReply(PointerGrabDefaultRequest(plugin->Panel->Window));
    Debug(3, "pointer grabbed\n");
#endif
}

/**
**	Handle a release event on a button panel plugin.
**
**	@param plugin	panel plugin
**	@param x	x-coordinate of button release
**	@param y	y-coordinate of button release
**	@param mask	button mask
*/
static void PanelButtonHandleButtonRelease(Plugin * plugin, int
    __attribute__ ((unused)) x, int __attribute__ ((unused)) y, int
    __attribute__ ((unused)) mask)
{
    PanelButtonDraw(plugin, 0);
    PanelUpdatePlugin(plugin->Panel, plugin);

    // since we grab the mouse, make sure the mouse is actually
    // over the button.
    if (x < 0 || x >= plugin->Width || y < 0 || y >= plugin->Height) {
	return;
    }

    Debug(0, "button release on panel button still needed?\n");
}

/**
**	Show tooltip of panel button panel plugin.
**
**	@param plugin	common panel plugin data of panel button
**	@param x	current mouse x-coordinate
**	@param y	current mouse y-coordinate
**
**	@todo tooltip is redrawn with each timeout (reduce it)
*/
static void PanelButtonTooltip(Plugin * plugin, int x, int y)
{
    const ButtonPlugin *button_plugin;
    const char *tooltip;

    button_plugin = plugin->Object;
    if (button_plugin->Tooltip) {
	tooltip = button_plugin->Tooltip;
    } else if (button_plugin->Text) {
	tooltip = button_plugin->Text;
    } else {
	return;
    }
    TooltipShow(x, y, tooltip);
}

/**
**	Update desktop change.
**
**	@todo FIXME: didn't change the size of button, for longer desktop names.
*/
void PanelButtonDesktopUpdate(void)
{
    ButtonPlugin *button_plugin;

    // search desktop labels
    SLIST_FOREACH(button_plugin, &Buttons, Next) {
	if (!button_plugin->IconName && !button_plugin->Text) {
	    PanelButtonDraw(button_plugin->Plugin, 0);
	    PanelUpdatePlugin(button_plugin->Plugin->Panel,
		button_plugin->Plugin);
	}
    }
}

// ------------------------------------------------------------------------ //

/**
**	Initialize panel buttons.
**
**	@todo FIXME: rework INNER_SPACES!
*/
void PanelButtonInit(void)
{
    ButtonPlugin *button_plugin;
    unsigned width;
    unsigned height;

    height = Fonts.PanelButton.Height;
    // send request for label size
    SLIST_FOREACH(button_plugin, &Buttons, Next) {
	// FIXME: desktop name button. size can be changed!!
	if (!button_plugin->IconName && !button_plugin->Text) {
	    button_plugin->Cookie =
		FontQueryExtentsRequest(&Fonts.PanelButton,
		strlen(DesktopGetName(DesktopCurrent)),
		DesktopGetName(DesktopCurrent));
	} else if (button_plugin->Text) {
	    button_plugin->Cookie =
		FontQueryExtentsRequest(&Fonts.PanelButton,
		strlen(button_plugin->Text), button_plugin->Text);
	}
    }

    // collect label+size size
    SLIST_FOREACH(button_plugin, &Buttons, Next) {
	Plugin *plugin;
	char *name;
	unsigned button_width;
	unsigned button_height;

	plugin = button_plugin->Plugin;
	button_width = 0;
	button_height = 0;
#ifdef USE_ICON
	if ((name = button_plugin->IconName)) {
	    button_plugin->Icon = IconLoadNamed(button_plugin->IconName);
	    button_plugin->IconLoaded = 1;
	    if (button_plugin->Icon) {
		button_width += button_plugin->Icon->Image->Width;
		button_height += button_plugin->Icon->Image->Height;
	    } else {
		Warning("could not load button panel icon: \"%s\"\n", name);
	    }
	    free(name);
	}
#endif
	if ((!button_plugin->IconName && !button_plugin->Text)
	    || button_plugin->Text) {
	    width = FontTextWidthReply(button_plugin->Cookie);
	    if (!button_plugin->IconOrText || !button_plugin->Icon) {
		button_width += width;
		// INNER_SPACE for icon offset
		if (button_plugin->Icon) {
		    width += LABEL_INNER_SPACE;
		}
		if (height > button_height) {
		    button_height = height;
		}
	    }
	}
	if (!button_plugin->UserWidth) {
	    plugin->RequestedWidth =
		button_width + 2 * LABEL_INNER_SPACE + 2 * LABEL_BORDER;
	}
	if (!button_plugin->UserHeight) {
	    plugin->RequestedHeight =
		button_height + 2 * LABEL_INNER_SPACE + 2 * LABEL_BORDER;
	}
    }
}

/**
**	Cleanup Panel button data.
*/
void PanelButtonExit(void)
{
    ButtonPlugin *button_plugin;

    while (!SLIST_EMPTY(&Buttons)) {	// list deletion
	button_plugin = SLIST_FIRST(&Buttons);
	MenuButtonDel(button_plugin->Buttons);
	if (button_plugin->IconLoaded) {
	    IconDel(button_plugin->Icon);
	} else {
	    free(button_plugin->Icon);
	}
	free(button_plugin->Text);
	free(button_plugin->Tooltip);
	SLIST_REMOVE_HEAD(&Buttons, Next);
	free(button_plugin);
    }
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Create a new panel button plugin.
**
**	@param item	menu item for icon, text and action
**	@param tooltip	extra tooltip to display, NULL shows text
**	@param width	button width, 0 auto sized
**	@param height	button height, 0 auto sized
**
**	@returns created button panel plugin.
*/
Plugin *PanelButtonNew(MenuItem * item, const char *tooltip, unsigned width,
    unsigned height)
{
    ButtonPlugin *button_plugin;
    Plugin *plugin;

    // validate arguments
    if (!item) {
	Warning("no menu item for button\n");
	return NULL;
    }
    if (width > RootWidth / 2) {
	Warning("invalid panel button width: %d\n", width);
	width = 0;
    }
    if (height > RootHeight / 2) {
	Warning("invalid panel button height: %d\n", height);
	height = 0;
    }

    button_plugin = calloc(1, sizeof(ButtonPlugin));
    SLIST_INSERT_HEAD(&Buttons, button_plugin, Next);

    button_plugin->Item = item;
    if (tooltip) {
	button_plugin->Tooltip = strdup(tooltip);
    }

    plugin = PanelPluginNew();
    plugin->Object = button_plugin;
    button_plugin->Plugin = plugin;

    plugin->RequestedWidth = width;
    plugin->RequestedHeight = height;

    plugin->Create = PanelButtonCreate;
    plugin->Delete = PanelPluginDeletePixmap;
#ifdef USE_ICON
    plugin->SetSize = PanelButtonSetSize;
#endif
    plugin->Resize = PanelButtonResize;
    if (tooltip || item->Text) {
	plugin->Tooltip = PanelButtonTooltip;
    }

    plugin->HandleButtonPress = PanelButtonHandleButtonPress;
    plugin->HandleButtonRelease = PanelButtonHandleButtonRelease;

    return plugin;
}

#else

/**
**	Create a new button panel plugin from config data.
**
**	@param array	configuration array for button panel plugin
**
**	@returns created button panel plugin.
*/
Plugin *PanelButtonConfig(const ConfigObject * array)
{
    ButtonPlugin *button_plugin;
    Plugin *plugin;
    const char *sval;
    ssize_t ival;

    button_plugin = calloc(1, sizeof(ButtonPlugin));
    SLIST_INSERT_HEAD(&Buttons, button_plugin, Next);

#ifdef USE_ICON
    if (ConfigGetString(array, &sval, "icon", NULL)) {
	button_plugin->IconName = strdup(sval);
    }
    if (ConfigGetInteger(array, &ival, "icon-or-text", NULL)) {
	button_plugin->IconOrText = ival != 0;
    }
#endif
    if (ConfigGetString(array, &sval, "text", NULL)) {
	button_plugin->Text = strdup(sval);
    }
    if (ConfigGetString(array, &sval, "tooltip", NULL)) {
	button_plugin->Tooltip = strdup(sval);
    }
    // common config of pointer buttons to commands
    MenuButtonsConfig(array, &button_plugin->Buttons);
    // FIXME: loose memory why????

    plugin = PanelPluginNew();
    plugin->Object = button_plugin;
    button_plugin->Plugin = plugin;

    // FIXME: generic width/height?
    if (ConfigGetInteger(array, &ival, "width", NULL)) {
	plugin->RequestedWidth = ival;
	button_plugin->UserWidth = 1;
    }
    if (ConfigGetInteger(array, &ival, "height", NULL)) {
	plugin->RequestedHeight = ival;
	button_plugin->UserHeight = 1;
    }

    plugin->Create = PanelButtonCreate;
    plugin->Delete = PanelPluginDeletePixmap;
#ifdef USE_ICON
    plugin->SetSize = PanelButtonSetSize;
#endif
    plugin->Resize = PanelButtonResize;
    if (button_plugin->Tooltip || button_plugin->Text) {
	plugin->Tooltip = PanelButtonTooltip;
    }

    plugin->HandleButtonPress = PanelButtonHandleButtonPress;
    plugin->HandleButtonRelease = PanelButtonHandleButtonRelease;

    return plugin;
}

#endif

#endif // } USE_BUTTON

/// @}
