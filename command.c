///
///	@file command.c @brief command functions.
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

///
///	@defgroup command The command module.
///
///	This module handles running startup, exiting, and restart commands,
///	And starting external commands.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "array.h"
#include "config.h"

#include "command.h"

// ------------------------------------------------------------------------ //

/**
**	command list typedef.
*/
typedef struct _commands_ Commands;

/**
**	command list structure
**
**	commands are separated by nul, an empty strings terminates the list.
*/
struct _commands_
{
    char *Commands;			///< list of 0-terminated strings
};

    /// Command which should be run at startup
static Commands CommandsStartup;

    /// Command which should be run at shutdown
static Commands CommandsExiting;

    /// Command which should be run at restart
static Commands CommandsRestart;

char *ExitCommand;			///< command to be run at exit
const char *Shell;			///< shell for commands

// ------------------------------------------------------------------------ //

/**
**	Execute an external program.
**
**	@param command	command string to be executed in shell
**
**	@see DisplayString, Shell
*/
void CommandRun(const char *command)
{
    if (!command) {
	return;
    }

    if (!fork()) {
	if (!fork()) {
	    const char *display;

	    // prepare environment
	    display = DisplayString ? DisplayString : getenv("DISPLAY");
	    if (display && *display) {
		char *str;

		str = alloca(9 + strlen(display));
		stpcpy(stpcpy(str, "DISPLAY="), display);
		putenv(str);
	    }

	    close(xcb_get_file_descriptor(Connection));
	    execl(Shell, Shell, "-c", command, NULL);
	    Warning("exec failed: %s -c %s\n", Shell, command);
	    exit(-1);
	}
	exit(0);
    }
    wait(NULL);
}

/**
**	Run commands in a command list.
**
**	@param list	list of 0-terminated strings
*/
static void CommandsRun(const Commands list)
{
    const char *s;

    if ((s = list.Commands)) {
	do {
	    CommandRun(s);
	    while (*s++) {
	    }
	} while (*s);
    }
}

/**
**	Free a command list.
**
**	@param list	list of 0-terminated strings
*/
static void CommandsFree(Commands * list)
{
    free(list->Commands);
    list->Commands = NULL;
}

/**
**	Initialize command module.
**
**	Run commands in startup on boot, in restart if restarting.
**
**	@see KeepRunning.
*/
void CommandInit(void)
{
    // prepare shell
    if (!Shell && !(Shell = getenv("SHELL"))) {
	Shell = SHELL;
    }

    if (KeepRunning) {
	CommandsRun(CommandsRestart);
    } else {
	CommandsRun(CommandsStartup);
    }
}

/**
**	Cleanup command module.
**
**	Run commands in exiting on exit.
**
**	@see KeepRunning.
*/
void CommandExit(void)
{
    if (!KeepRunning) {
	CommandsRun(CommandsExiting);
    }

    CommandsFree(&CommandsStartup);
    CommandsFree(&CommandsExiting);
    CommandsFree(&CommandsRestart);
}

// ------------------------------------------------------------------------ //
// Config

#ifdef USE_LUA

/**
**	Add a command to a command list.
*/
static void CommandAdd(CommandHead * head, const char *command)
{
    Command *entry;

    if (!command) {
	return;
    }

    entry = malloc(sizeof(*entry));
    entry->Command = strdup(command);
    SLIST_INSERT_HEAD(head, entry, Next);
}

/**
**	Add a startup command.
*/
void CommandAddStartup(const char *command)
{
    CommandAdd(&CommandsStartup, command);
}

/**
**	Add a stoping command.
*/
void CommandAddExiting(const char *command)
{
    CommandAdd(&CommandsExiting, command);
}

/**
**	Add a restart command.
*/
void CommandAddRestart(const char *command)
{
    CommandAdd(&CommandsRestart, command);
}

#else

/**
**	Parse configuration of a command list.
**
**	@param config	global config dictionary
**	@param head	command list head
**	@param index	config index key
*/
void CommandAdd(const Config * config, Commands * head, const char *index)
{
    const ConfigObject *array;

    if (ConfigGetArray(ConfigDict(config), &array, index, NULL)) {
	const ConfigObject *index;
	const ConfigObject *value;
	int len;

	len = 0;

	//
	//	Calculate length of commands
	//
	index = NULL;
	value = ConfigArrayFirst(array, &index);
	while (value) {
	    const char *command;

	    if (ConfigCheckString(value, &command)) {
		len += strlen(command) + 1;
	    }
	    value = ConfigArrayNext(array, &index);
	}

	if (len) {
	    char *dst;

	    head->Commands = dst = malloc(len + 1);

	    //
	    //	Copy the commands
	    //
	    index = NULL;
	    value = ConfigArrayFirst(array, &index);
	    while (value) {
		const char *command;

		if (ConfigCheckString(value, &command)) {
		    dst = stpcpy(dst, command);
		    ++dst;
		}
		value = ConfigArrayNext(array, &index);
	    }
	    *dst = '\0';		// final nul to terminate list
	}
    }
}

/**
**	Parse configuration for command module.
**
**	@param config	global config dictionary
*/
void CommandConfig(const Config * config)
{
    CommandAdd(config, &CommandsStartup, "command-startup");
    CommandAdd(config, &CommandsRestart, "command-restart");
    CommandAdd(config, &CommandsExiting, "command-exiting");
}

#endif

/// @}
