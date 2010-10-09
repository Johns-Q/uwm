///
///	@file rule.c	@brief client/window rule functions
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
///	@defgroup rule The rule module.
///
///	This module contains functions for handling window rules.
///
///	Window rules allow one to specify options which apply to a group
///	of windows by title and/or name and/or class.
///
///	It used the ICCCM hints WM_CLASS and WM_NAME or EWMH _NET_WM_NAME.
///
///	The module is only available, if compiled with #USE_RULE.
///
///	Instead of the buildin rules fe. x11-misc/devilspie
///	http://www.burtonini.com/blog/computers/devilspie can be used.
///
///	@todo regex are compiled for each rule/client, try other: peg (Parsing
///	expression grammer) or simple shell pattern matching.
///
/// @{

#define _GNU_SOURCE	1		///< fix stpcpy strchrnul

#include <xcb/xcb.h>
#include "uwm.h"

#ifdef USE_RULE				// {

#include <sys/queue.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <regex.h>

#include <xcb/xcb_icccm.h>

#include "core-array/core-array.h"
#include "core-rc/core-rc.h"

#include "draw.h"
#include "image.h"

#include "client.h"
#include "icon.h"
#include "menu.h"
#include "desktop.h"
#include "panel.h"
#include "rule.h"

// ------------------------------------------------------------------------ //
// Rule
// ------------------------------------------------------------------------ //

/**
**	Enumeration of rule options action.
*/
typedef enum
{
    RULE_ACTION_INVALID,		///< invalid no option to set

    RULE_ACTION_STICKY = (1 << 0),	///< start in sticky state
    RULE_ACTION_FULLSCREEN = (1 << 1),	///< start in fullscreen state
    RULE_ACTION_NOLIST = (1 << 2),	///< don't display in task list
    RULE_ACTION_NOPAGER = (1 << 3),	///< don't display in pager

    RULE_ACTION_LAYER = (1 << 4),	///< start on specific layer
    RULE_ACTION_DESKTOP = (1 << 5),	///< start on specific desktop
    RULE_ACTION_ICON = (1 << 6),	///< set icon to use

    RULE_ACTION_BORDER = (1 << 7),	///< force window border
    RULE_ACTION_NOBORDER = (1 << 8),	///< don't draw window border
    RULE_ACTION_TITLE = (1 << 9),	///< force window title bar
    RULE_ACTION_NOTITLE = (1 << 10),	///< don't draw window title bar
    RULE_ACTION_PIGNORE = (1 << 11),	///< ignore program-specified location
    RULE_ACTION_MAXIMIZED = (1 << 12),	///< start maximized
    RULE_ACTION_MINIMIZED = (1 << 13),	///< start minimized
    RULE_ACTION_SHADED = (1 << 14),	///< start shaded
    RULE_ACTION_OPACITY = (1 << 15),	///< set opacity
    RULE_ACTION_MAXIMIZE_HORZ = (1 << 16),	///< use horizontal maximization
    RULE_ACTION_MAXIMIZE_VERT = (1 << 17),	///< use vertical maximization

    RULE_ACTION_X = (1 << 18),		///< set x-coordinate
    RULE_ACTION_Y = (1 << 19),		///< set y-coordinate
    RULE_ACTION_WIDTH = (1 << 20),	///< set window width
    RULE_ACTION_HEIGHT = (1 << 21),	///< set window height

    RULE_ACTION_GRAVITY = (1 << 22),	///< window gravity
} RuleAction;

/**
**	Rule pattern typedef and structure.
**
**	Contains combined string of the patterns name, class and title.
**	(name \\0 class \\0 title \\0 \\0)
*/
typedef struct _rule_pattern_
{
    char *Strings;			///< strings name, class, title
} RulePattern;

/**
**	Rule option typedef and structure.
*/
typedef struct _rule_option_
{
    uint32_t Actions;			///< actions population
    union
    {
	char *String;			///< extra string value of option
	int Integer;			///< integer value of option
    } Values[1];			///< values of options
} RuleOption;

/**
**	Rule typedef and structure.
*/
typedef struct _rule_
{
    RulePattern *Patterns;		///< alternative pattern to match
    int PatternN;			///< number of patterns
    RuleOption *Options;		///< table of all options to apply

    int Instances;			///< for how many windows to match
    int Matches;			///< how many matches for this rule
} Rule;

static Rule *Rules;			///< table of rules
static int RuleN;			///< number of rules in table

// ---------------------------------------------------------------------------

/**
**	Apply rule to client.
**
**	When the client is already mapped, move and resize rules are skipped.
**
**	@param client		client which options are set
**	@param already_mapped	true if already mapped, false if unmapped
**	@param options		options of client to set
*/
static void RuleApplyOptions(Client * client, int already_mapped,
    const RuleOption * options)
{
    int i;

    Debug(3, "	 apply the options %x\n", options->Actions);
    i = 0;
    if (options->Actions & RULE_ACTION_STICKY) {
	client->State |= WM_STATE_STICKY;
    }
    if (options->Actions & RULE_ACTION_FULLSCREEN) {
	client->State |= WM_STATE_FULLSCREEN;
    }
    if (options->Actions & RULE_ACTION_NOLIST) {
	client->State |= WM_STATE_NOLIST;
    }
    if (options->Actions & RULE_ACTION_NOPAGER) {
	client->State |= WM_STATE_NOPAGER;
    }

    if (options->Actions & RULE_ACTION_LAYER) {
	Debug(3, "   layer %d\n", options->Values[i].Integer);
	if (options->Values[i].Integer < LAYER_MAX) {
	    client->OnLayer = options->Values[i].Integer;
	} else {
	    Warning("invalid rule layer: %d\n", options->Values[i].Integer);
	}
	++i;
    }
    if (options->Actions & RULE_ACTION_DESKTOP) {
	Debug(3, "   desktop %d\n", options->Values[i].Integer);
	if (options->Values[i].Integer >= 0
	    && options->Values[i].Integer < DesktopN) {
	    client->Desktop = options->Values[i].Integer;
	} else {
	    Warning("invalid rule desktop: %d\n", options->Values[i].Integer);
	}
	++i;
    }
#ifdef USE_ICON
    if (options->Actions & RULE_ACTION_ICON) {
	Debug(3, "   icon %s\n", options->Values[i].String);
	IconDel(client->Icon);
	client->Icon = IconLoadNamed(options->Values[i].String);
	++i;
    }
#endif
    if (options->Actions & RULE_ACTION_BORDER) {
	client->Border |= BORDER_OUTLINE;
    }
    if (options->Actions & RULE_ACTION_NOBORDER) {
	client->Border &= ~BORDER_OUTLINE;
    }
    if (options->Actions & RULE_ACTION_TITLE) {
	client->Border |= BORDER_TITLE;
    }
    if (options->Actions & RULE_ACTION_NOTITLE) {
	client->Border &= ~BORDER_TITLE;
	// shaded window needs window title
	client->State &= ~WM_STATE_SHADED;
    }

    if (options->Actions & RULE_ACTION_PIGNORE) {
	client->State |= WM_STATE_PIGNORE;
    }
    if (options->Actions & RULE_ACTION_MAXIMIZED) {
	client->State |= WM_STATE_MAXIMIZED_HORZ | WM_STATE_MAXIMIZED_VERT;
    }
    if (options->Actions & RULE_ACTION_MINIMIZED) {
	client->State |= WM_STATE_MINIMIZED;
    }
    if (options->Actions & RULE_ACTION_SHADED) {
	client->State |= WM_STATE_SHADED;
	// shaded window needs window title
	client->Border |= BORDER_TITLE;
    }
    if (options->Actions & RULE_ACTION_OPACITY) {
	Debug(3, "   opacity %d\n", options->Values[i].Integer);
	if (!options->Values[i].Integer) {
	    client->State &= ~WM_STATE_OPACITY;
	} else {
	    client->Opacity = options->Values[i].Integer;
	    client->State |= WM_STATE_OPACITY;
	}
	++i;
    }
    if (options->Actions & RULE_ACTION_MAXIMIZE_VERT) {
	client->Border &= ~BORDER_MAXIMIZE_HORZ;
    }
    if (options->Actions & RULE_ACTION_MAXIMIZE_HORZ) {
	client->Border &= ~BORDER_MAXIMIZE_VERT;
    }
    if (options->Actions & RULE_ACTION_X) {
	Debug(3, "   x %d\n", options->Values[i].Integer);
	if (!already_mapped) {
	    client->X = options->Values[i].Integer;
	    client->SizeHints.flags |= XCB_SIZE_HINT_US_POSITION;
	}
	++i;
    }
    if (options->Actions & RULE_ACTION_Y) {
	Debug(3, "   y %d\n", options->Values[i].Integer);
	if (!already_mapped) {
	    client->Y = options->Values[i].Integer;
	    client->SizeHints.flags |= XCB_SIZE_HINT_US_POSITION;
	}
	++i;
    }
    if (options->Actions & RULE_ACTION_WIDTH) {
	Debug(3, "   width %d\n", options->Values[i].Integer);
	if (!already_mapped) {
	    if (options->Values[i].Integer <= 0) {
		// FIXME: should use strut? and don't use RootWidth
		client->Width = RootWidth - options->Values[i].Integer;
	    } else {
		client->Width = options->Values[i].Integer;
	    }
	    client->SizeHints.flags |= XCB_SIZE_HINT_US_SIZE;
	}
	// FIXME: update dimensions of client
	++i;
    }
    if (options->Actions & RULE_ACTION_HEIGHT) {
	Debug(3, "   height %d\n", options->Values[i].Integer);
	if (!already_mapped) {
	    if (options->Values[i].Integer <= 0) {
		// FIXME: should use strut? and don't use RootHeight
		client->Height = RootHeight - options->Values[i].Integer;
	    } else {
		client->Height = options->Values[i].Integer;
	    }
	    client->SizeHints.flags |= XCB_SIZE_HINT_US_SIZE;
	    // FIXME: update dimensions of client
	}
	++i;
    }
    // @note: must be after x, y, width, height
    if (options->Actions & RULE_ACTION_GRAVITY) {
	Debug(3, "   gravity %d - %+d%+d\n", options->Values[i].Integer,
	    client->X, client->Y);
	if (!already_mapped) {
	    // FIXME: use strut? and don't use RootWidth/RootHeight
	    switch (options->Values[i].Integer) {
		case PANEL_GRAVITY_STATIC:
		    break;
		case PANEL_GRAVITY_NORTH_WEST:
		    break;
		case PANEL_GRAVITY_NORTH:
		    client->X = RootWidth / 2 - client->Width / 2 + client->X;
		    break;
		case PANEL_GRAVITY_NORTH_EAST:
		    client->X = RootWidth - client->Width + client->X;
		    break;
		case PANEL_GRAVITY_WEST:
		    client->Y =
			RootHeight / 2 - client->Height / 2 + client->Y;
		    break;
		case PANEL_GRAVITY_CENTER:
		    client->X = RootWidth / 2 - client->Width / 2 + client->X;
		    client->Y =
			RootHeight / 2 - client->Height / 2 + client->Y;
		    break;
		case PANEL_GRAVITY_EAST:
		    client->X = RootWidth - client->Width + client->X;
		    client->Y =
			RootHeight / 2 - client->Height / 2 + client->Y;
		    break;
		case PANEL_GRAVITY_SOUTH_WEST:
		    client->Y = RootHeight - client->Height + client->Y;
		    break;
		case PANEL_GRAVITY_SOUTH:
		    client->X = RootWidth / 2 - client->Width / 2 + client->X;
		    client->Y = RootHeight - client->Height + client->Y;
		    break;
		case PANEL_GRAVITY_SOUTH_EAST:
		    client->X = RootWidth - client->Width + client->X;
		    client->Y = RootHeight - client->Height + client->Y;
		    break;
	    }
	    Debug(3, "	 gravity: %dx%d%+d%+d\n", client->Width,
		client->Height, client->X, client->Y);
	    // disable automatic window placement
	    client->SizeHints.flags |= XCB_SIZE_HINT_US_POSITION;
	}
    }
}

/**
**	Match pattern against client.
**
**	@param pattern	pattern string
**	@param name	client name string
**
**	@returns string after pattern if matched, NULL otherwise.
*/
static const char *RuleMatchPattern(const char *pattern, const char *name)
{
    if (*pattern) {			// match name
	regex_t preg;
	int status;

	Debug(3, "  Name %s/%s\n", pattern, name);
	if (regcomp(&preg, pattern, 0)) {
	    Warning("invalid regular expression '%s'\n", pattern);
	    return NULL;
	}
	status = regexec(&preg, name ? name : "", 0, NULL, 0);
	regfree(&preg);
	if (status) {
	    return NULL;
	}
	pattern = strchr(pattern, '\0');
    }
    return pattern + 1;
}

/**
**	Apply rules to new client.
**
**	@param client		client to get default options
**	@param already_mapped	true if already mapped, false if unmapped
*/
void RulesApplyNewClient(Client * client, int already_mapped)
{
    Rule *rule;

    for (rule = Rules; rule < Rules + RuleN; ++rule) {
	int i;

	Debug(3, " rule %p %d/%d\n", rule, rule->Matches, rule->Instances);
	//
	//	any of the patterns must match.
	//
	for (i = 0; i < rule->PatternN; ++i) {
	    const char *s;

	    if (!(s =
		    RuleMatchPattern(rule->Patterns[i].Strings,
			client->InstanceName))) {
		continue;
	    }
	    if (!(s = RuleMatchPattern(s, client->ClassName))) {
		continue;
	    }
	    if (!RuleMatchPattern(s, client->Name)) {
		continue;
	    }
	    ++rule->Matches;
	    if (rule->Instances < rule->Matches) {	// rule out of order
		continue;
	    }
	    // all pattern of this string matched, apply options
	    RuleApplyOptions(client, already_mapped, rule->Options);
	    break;			// next rule
	}
    }
}

/**
**	Apply rules for delete client.
**
**	@param client	client to update match count
*/
void RulesApplyDelClient(const Client * client)
{
    Rule *rule;

    for (rule = Rules; rule < Rules + RuleN; ++rule) {
	int i;

	Debug(3, " rule %p %d/%d\n", rule, rule->Matches, rule->Instances);
	if (!rule->Matches) {		// no match yet
	    continue;
	}
	//
	//	any of the patterns must match.
	//
	for (i = 0; i < rule->PatternN; ++i) {
	    const char *s;

	    if (!(s =
		    RuleMatchPattern(rule->Patterns[i].Strings,
			client->InstanceName))) {
		continue;
	    }
	    if (!(s = RuleMatchPattern(s, client->ClassName))) {
		continue;
	    }
	    if (!RuleMatchPattern(s, client->Name)) {
		continue;
	    }
	    // all pattern of this string matched, reduce matches
	    --rule->Matches;
	    break;			// next rule
	}
    }
}

// ---------------------------------------------------------------------------

/**
**	Cleanup the rule module.
*/
void RuleExit(void)
{
    int i;

    for (i = 0; i < RuleN; ++i) {
	int j;

	// free strings in options
	j = 0;
	if (Rules[i].Options->Actions & RULE_ACTION_LAYER) {
	    ++j;
	}
	if (Rules[i].Options->Actions & RULE_ACTION_DESKTOP) {
	    ++j;
	}
	if (Rules[i].Options->Actions & RULE_ACTION_ICON) {
	    free(Rules[i].Options->Values[j].String);
	    ++j;
	}
	free(Rules[i].Options);

	for (j = 0; j < Rules[i].PatternN; ++j) {
	    free(Rules[i].Patterns[j].Strings);
	}
	free(Rules[i].Patterns);
    }
    free(Rules);
    Rules = NULL;
    RuleN = 0;
}

// ------------------------------------------------------------------------ //
// Config

/**
**	Add option to rule.
**
**	@param rule	rule to update
**	@param n	number of values already used
**	@param action	action of option
**	@param val	value of action
*/
static void RuleConfigAddOption(Rule * rule, int n, RuleAction action, int val)
{
    // sizeof already contains one value
    rule->Options =
	realloc(rule->Options,
	sizeof(*rule->Options) + n * sizeof(rule->Options->Values));
    rule->Options->Actions |= action;
    rule->Options->Values[n].Integer = val;
}

/**
**	Parse single rule configuration.
**
**	@param array	array of rule config values.
*/
static void RuleConfigRule(const ConfigObject * array)
{
    Rule *rule;
    const ConfigObject *index;
    const ConfigObject *value;
    const ConfigObject *aval;
    ssize_t ival;

    Rules = realloc(Rules, (RuleN + 1) * sizeof(*Rules));
    rule = Rules + RuleN;
    ++RuleN;

    //
    //	options
    //
    rule->Options = NULL;
    if (ConfigGetArray(array, &aval, "option", NULL)) {
	int n;
	const ConfigObject *oval;
	const char *sval;
	double dval;

	n = 0;
	rule->Options =
	    malloc(sizeof(*rule->Options) + (n -
		1) * sizeof(rule->Options->Values));
	rule->Options->Actions = RULE_ACTION_INVALID;
	if (ConfigGetObject(aval, &oval, "none", NULL)) {
	} else {
	    // FIXME: write ConfigGetBoolean
	    if (ConfigGetInteger(aval, &ival, "sticky", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_STICKY;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "fullscreen", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_FULLSCREEN;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "no-list", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_NOLIST;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "no-pager", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_NOPAGER;
		}
	    }
	    // FIXME: should support layer names!
	    // FIXME: use ParseLayer
	    if (ConfigGetInteger(aval, &ival, "layer", NULL)) {
		RuleConfigAddOption(rule, n++, RULE_ACTION_LAYER, ival);
	    }
	    if (ConfigGetInteger(aval, &ival, "desktop", NULL)) {
		RuleConfigAddOption(rule, n++, RULE_ACTION_DESKTOP, ival);
	    }
#ifdef USE_ICON
	    if (ConfigGetString(aval, &sval, "icon", NULL)) {
		RuleConfigAddOption(rule, n, RULE_ACTION_ICON, 0);
		rule->Options->Values[n++].String = strdup(sval);
	    }
#endif

	    if (ConfigGetInteger(aval, &ival, "border", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_BORDER;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "no-border", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_NOBORDER;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "title", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_TITLE;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "no-title", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_NOTITLE;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "pignore", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_PIGNORE;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "maximized", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_MAXIMIZED;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "minimized", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_MINIMIZED;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "shaded", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_SHADED;
		}
	    }
	    if (ConfigGetDouble(aval, &dval, "opacity", NULL)) {
		uint32_t uval;

		if (dval <= 0.0 || dval > 1.0) {
		    Warning("invalid panel opacity: %g\n", dval);
		    dval = 1.0;
		}
		uval = UINT32_MAX * dval;
		if (uval != UINT32_MAX) {
		    RuleConfigAddOption(rule, n++, RULE_ACTION_OPACITY, uval);
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "maximize-horizontal", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_MAXIMIZE_HORZ;
		}
	    }
	    if (ConfigGetInteger(aval, &ival, "maximize-vertical", NULL)) {
		if (ival) {
		    rule->Options->Actions |= RULE_ACTION_MAXIMIZE_VERT;
		}
	    }

	    if (ConfigGetInteger(aval, &ival, "x", NULL)) {
		RuleConfigAddOption(rule, n++, RULE_ACTION_X, ival);
	    }
	    if (ConfigGetInteger(aval, &ival, "y", NULL)) {
		RuleConfigAddOption(rule, n++, RULE_ACTION_Y, ival);
	    }
	    if (ConfigGetInteger(aval, &ival, "width", NULL)) {
		RuleConfigAddOption(rule, n++, RULE_ACTION_WIDTH, ival);
	    }
	    if (ConfigGetInteger(aval, &ival, "height", NULL)) {
		RuleConfigAddOption(rule, n++, RULE_ACTION_HEIGHT, ival);
	    }
	    if (ConfigGetString(aval, &sval, "gravity", NULL)) {
		int i;

		i = ParseGravity(sval, "rule");
		if (i >= 0) {		// static, ...
		    RuleConfigAddOption(rule, n++, RULE_ACTION_GRAVITY, i);
		}
	    }
	}
    }
    Debug(3, " options %x\n", rule->Options->Actions);

    rule->Matches = 0;
    rule->Instances = INT_MAX;
    if (ConfigGetInteger(array, &ival, "instances", NULL)) {
	rule->Instances = ival;
    }
    //
    //	pattern
    //
    rule->Patterns = NULL;
    rule->PatternN = 0;
    index = NULL;
    value = ConfigArrayFirstFixedKey(array, &index);
    while (value) {

	if (ConfigCheckArray(value, &aval)) {
	    const char *name;
	    const char *class;
	    const char *title;
	    char *s;
	    int n;

	    Debug(3, " rule %p\n", aval);

	    rule->Patterns =
		realloc(rule->Patterns,
		(rule->PatternN + 1) * sizeof(*rule->Patterns));

	    n = 3;
	    name = NULL;
	    if (ConfigGetString(aval, &name, "name", NULL)) {
		n += strlen(name);
	    }
	    class = NULL;
	    if (ConfigGetString(aval, &class, "class", NULL)) {
		n += strlen(class);
	    }
	    title = NULL;
	    if (ConfigGetString(aval, &title, "title", NULL)) {
		n += strlen(title);
	    }
	    Debug(3, "	%s/%s/%s\n", name, class, title);
	    s = rule->Patterns[rule->PatternN++].Strings = malloc(n);
	    if (name) {
		s = stpcpy(s, name);
	    }
	    *s++ = '\0';
	    if (class) {
		s = stpcpy(s, class);
	    }
	    *s++ = '\0';
	    if (title) {
		s = stpcpy(s, title);
	    }
	    *s = '\0';
	} else {
	    Warning("value in pattern config ignored\n");
	}
	value = ConfigArrayNextFixedKey(array, &index);
    }
}

/**
**	Parse client rules configuration.
**
**	@param config	global config dictionary
*/
void RuleConfig(const Config * config)
{
    const ConfigObject *array;

    //
    //	array of rules
    //
    if (ConfigGetArray(ConfigDict(config), &array, "rule", NULL)) {
	const ConfigObject *index;
	const ConfigObject *value;

	//
	//	rules
	//
	index = NULL;
	value = ConfigArrayFirstFixedKey(array, &index);
	while (value) {
	    const ConfigObject *aval;

	    if (ConfigCheckArray(value, &aval)) {
		Debug(3, "rule %p\n", aval);
		RuleConfigRule(aval);
	    } else {
		Warning("value in rule config ignored\n");
	    }
	    value = ConfigArrayNextFixedKey(array, &index);
	}
    }
}

#endif // } USE_RULE

/// @}
