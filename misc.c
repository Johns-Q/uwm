///
///	@file misc.c	@brief misc functions
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
///	@defgroup misc The misc module.
///
///	This module contains the functions wich didn't fit in other modules.
///
/// @{

#include <xcb/xcb.h>
#include "uwm.h"

#include <sys/time.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>			// PATH_MAX

// ------------------------------------------------------------------------ //
//	Time
// ------------------------------------------------------------------------ //

/**
**	Get ticks in ms.
**
**	@returns ticks in ms,
*/
uint32_t GetMsTicks(void)
{
    struct timeval tval;

    if (gettimeofday(&tval, NULL) < 0) {
	return 0;
    }
    return (tval.tv_sec * 1000) + (tval.tv_usec / 1000);
}

// ------------------------------------------------------------------------ //
//	Tools
// ------------------------------------------------------------------------ //

/**
**	Get users home directory.
**
**	@param username	get the home directory of user with username
**
**	@returns user home directory in a static buffer.
*/
static const char *GetUserHomeDir(const char *username)
{
    struct passwd *user;

    if (!(user = getpwnam(username))) {
	Warning("Could not get password entry for user %s\n", username);
	return "/";
    }
    if (!user->pw_dir) {
	return "/";
    }
    return user->pw_dir;
}

/**
**	Get current users home directory.
**
**	@returns current user home directory in a static buffer.
*/
static const char *GetHomeDir(void)
{
    const char *home;
    struct passwd *user;

    if ((home = getenv("HOME"))) {	// environment exists: easy way
	return home;
    }
    // get home directory from password file entry
    if (!(user = getpwuid(getuid()))) {
	Warning("Could not get password entry for UID %i\n", getuid());
	return "/";
    }
    if (!user->pw_dir) {		// user has no home dir
	return "/";
    }
    return user->pw_dir;
}

/**
**	Expand path. Do shell like macro expansion.
**
**	- ~/ is expanded to the current user home directory.
**	- ~user/ is expanded to the named user home directory.
**	- $macro/ is expanded to the environment value of macro.
**	- $(macro) is expanded to the environment value of macro.
**
**	@param path	expand ~/ and $macro in path
**
**	@returns expanded path, malloced.
*/
char *ExpandPath(const char *path)
{
    char nambuf[PATH_MAX];
    char buffer[PATH_MAX];
    const char *s;
    char *d;
    int j;
    const char *tmp;

    d = buffer;
    s = path;
    if (*s == '~') {			// home directory expansion
	s++;
	if (*s == '/' || !*s) {
	    tmp = GetHomeDir();
	} else {
	    for (j = 0; *s && *s != '/'; ++s) {
		nambuf[j++] = *s;
	    }
	    nambuf[j] = '\0';
	    tmp = GetUserHomeDir(nambuf);
	}
	d = stpcpy(d, tmp);
    }

    while (*s) {
	if (*s == '$') {		// macro expansion
	    s++;
	    // expand $(HOME) or $HOME style environment variables
	    if (*s == '(') {
		s++;
		for (j = 0; *s && *s != ')'; ++s) {
		    nambuf[j++] = *s;
		}
		nambuf[j] = '\0';
		if (*s == ')') {
		    s++;
		}

		if ((tmp = getenv(nambuf))) {
		    d = stpcpy(d, tmp);
		} else {
		    // not found: reinsert macro.
		    d = stpcpy(d, "$(");
		    d = stpcpy(d, nambuf);
		    d = stpcpy(d, ")");
		}
	    } else {
		for (j = 0; *s && *s != '/'; ++s) {
		    nambuf[j++] = *s;
		}
		nambuf[j] = '\0';
		if ((tmp = getenv(nambuf))) {
		    d = stpcpy(d, tmp);
		} else {
		    // not found: reinsert macro.
		    d = stpcpy(d, "$");
		    d = stpcpy(d, nambuf);
		}
	    }
	} else {
	    *d++ = *s++;
	}
    }
    if (d > buffer + 1 && d[-1] == '/') {	// remove any trailing slash
	--d;
    }
    *d = '\0';

    Debug(3, "%s(%s) -> %s\n", __FUNCTION__, path, buffer);

    return strdup(buffer);
}

/// @}
