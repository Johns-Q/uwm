///
///	@file rule.h	@brief window/client rules header file
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

/// @addtogroup rule
/// @{

//////////////////////////////////////////////////////////////////////////////
//	Prototypes
//////////////////////////////////////////////////////////////////////////////

    /// Apply rules to new client.
extern void RulesApplyNewClient(Client *, int);

    /// Apply rules for delete client.
extern void RulesApplyDelClient(const Client *);

    /// Dummy for initialize the rule module
#define RuleInit()
    /// Cleanup the rule module.
extern void RuleExit(void);

    /// Parse client rules configuration.
extern void RuleConfig(const Config *);

#ifndef USE_RULE

    /// Dummy for cleanup the rule module.
#define RuleExit(void)
    /// Dummy for parse client rules configuration.
#define RuleConfig(config)

#endif

/// @}
