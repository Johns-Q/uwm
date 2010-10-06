///
///	@file task.h	@brief task panel plugin header file.
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

///	@ingroup panel
///	@addtogroup task_plugin
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Focus next client in task.
extern void TaskFocusNext(void);

    /// Focus previous client in task.
extern void TaskFocusPrevious(void);

    /// Update all task plugin(s).
extern void TaskUpdate(void);

    /// Intialize task panel plugin.
extern void TaskInit(void);

    /// Cleanup task panel plugin.
extern void TaskExit(void);

    /// Parse task panel plugin configuration.
Plugin *TaskConfig(const ConfigObject *);

#ifndef USE_TASK			// {

    /// Dummy for Update all task plugin(s).
#define TaskUpdate()

    /// Dummy for initialize task(s).
#define TaskInit()
    /// Dummy for cleanup task(s).
#define TaskExit()

    /// Dummy for Parse task panel plugin configuration.
#define TaskConfig(config)	NULL

#endif // } USE_TASK

/// @}
