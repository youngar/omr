/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcfg.h"

#if defined(OMR_OS_WINDOWS)
/* Undefine the winsockapi because winsock2 defines it.  Removes warnings. */
#if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#endif /* defined(OMR_OS_WINDOWS) */

#include "omrport.h"
#include "omr.h"
#include "omrTest.h"
#include "omrTestHelpers.h"

#include "OMR_Agent.hpp"
#include "rasTestHelpers.hpp"

class RASAgentNegativeTest: public ::testing::TestWithParam<const char *>
{
protected:
	virtual void
	SetUp()
	{
		OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, rasTestEnv->getPortLibrary()));
#if defined(OMR_OS_WINDOWS)
		/* initialize sockets so that we can use gethostname() */
		WORD wsaVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		int wsaErr = WSAStartup(wsaVersionRequested, &wsaData);
		ASSERT_EQ(0, wsaErr);
#endif /* defined(OMR_OS_WINDOWS) */
	}

	virtual void
	TearDown()
	{
#if defined(OMR_OS_WINDOWS)
		WSACleanup();
#endif /* defined(OMR_OS_WINDOWS) */
		OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
	}

	OMRTestVM testVM;
};

/*
 * Test scenarios for InvalidAgentC and InvalidAgentCPP:
 *  - missing OnLoad entry point in agent
 *  - missing OnUnload entry point in agent
 *  - <dllpath> indicates nonexistent file
 *
 *  Please see INSTANTIATE_TEST_CASE_P for GetParam() values
 */
TEST_P(RASAgentNegativeTest, InvalidAgentC)
{
	struct OMR_Agent *agent = omr_agent_create(&testVM.omrVM, GetParam());
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() failed";
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, omr_agent_openLibrary(agent));
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, omr_agent_callOnLoad(agent));
	OMRTEST_ASSERT_ERROR(OMR_ERROR_INTERNAL, omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);
}

TEST_P(RASAgentNegativeTest, InvalidAgentCPP)
{
	OMR_Agent *agent = OMR_Agent::createAgent(&testVM.omrVM, GetParam());
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() failed";
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, agent->openLibrary());
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, agent->callOnLoad());
	OMRTEST_ASSERT_ERROR(OMR_ERROR_INTERNAL, agent->callOnUnload());
	OMR_Agent::destroyAgent(agent);
}

INSTANTIATE_TEST_CASE_P(InvalidAgentOpts, RASAgentNegativeTest,
	::testing::Values("invalidAgentMissingOnLoad=test",
		"invalidAgentMissingOnUnload", "/tmp/invalidAgentPath/agent=def"));

/*
 * Test scenarios for AgentReturnErrorC and AgentReturnErrorCPP:
 *  - agent's OnLoad() function returns OMR_ERROR_OUT_OF_NATIVE_MEMORY
 *  - agent's OnUnload() function returns OMR_ERROR_ILLEGAL_ARGUMENT
 */
TEST_F(RASAgentNegativeTest, AgentReturnErrorC)
{
	struct OMR_Agent *agent = omr_agent_create(&testVM.omrVM, "invalidAgentReturnError=abc");
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() failed";
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));
	OMRTEST_ASSERT_ERROR(OMR_ERROR_OUT_OF_NATIVE_MEMORY, omr_agent_callOnLoad(agent));
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);
}

TEST_F(RASAgentNegativeTest, AgentReturnErrorCPP)
{
	OMR_Agent *agent = OMR_Agent::createAgent(&testVM.omrVM, "invalidAgentReturnError=abc");
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() failed";
	OMRTEST_ASSERT_ERROR_NONE(agent->openLibrary());
	OMRTEST_ASSERT_ERROR(OMR_ERROR_OUT_OF_NATIVE_MEMORY, agent->callOnLoad());
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, agent->callOnUnload());
	OMR_Agent::destroyAgent(agent);
}

/*
 * Test scenarios for ArgEmptyC and ArgEmptyCPP:
 *  - dllPath is empty string
 *  - arg is empty string
 *
 * This test is disabled until it is fixed. We expect it has consistent behavior as arg is NULL
 */
TEST_F(RASAgentNegativeTest, DISABLED_ArgEmptyC)
{
	struct OMR_Agent *agent = omr_agent_create(&testVM.omrVM, "=abc");
	ASSERT_TRUE(NULL == agent);

	agent = omr_agent_create(&testVM.omrVM, "");
	ASSERT_TRUE(NULL == agent);
}

TEST_F(RASAgentNegativeTest, DISABLED_ArgEmptyCPP)
{
	OMR_Agent *agent = OMR_Agent::createAgent(&testVM.omrVM, "=abc");
	ASSERT_TRUE(NULL == agent);

	agent = OMR_Agent::createAgent(&testVM.omrVM, "");
	ASSERT_TRUE(NULL == agent);
}

/*
 * Test scenario for CorruptedAgent:
 * 	- create an empty agent file
 *  - call omr_agent_create() and OMR_Agent::createAgent by providing the empty agent file
 *  - expect openLibrary() to fail
 */
TEST_F(RASAgentNegativeTest, CorruptedAgent)
{
	intptr_t fileDescriptor;
	char fileName[256];
	char agentName[256];
	char hostname[128];
	OMRPORT_ACCESS_FROM_OMRVM(&testVM.omrVM);
	ASSERT_EQ(0, gethostname(hostname, sizeof(hostname)));

	/* generate machine specific file name to prevent conflict between multiple tests running on shared drive */
	omrstr_printf(agentName, sizeof(agentName), "corruptedAgent_%s", hostname);

	/* create fileName with platform-dependent shared library prefix & suffix.
	 *  for Windows, fileName = corruptedAgent_<hostname>.dll
	 *  for Unix, fileName = libcorruptedAgent_<hostname>.so
	 *  for OSX, fileName = libcorruptedAgent_<hostname>.dylib
	 */
#if defined(OMR_OS_WINDOWS)
	omrstr_printf(fileName, sizeof(fileName), "%s.dll", agentName);
#elif defined(OSX)
	omrstr_printf(fileName, sizeof(fileName), "lib%s.dylib", agentName);
#else /* defined(OSX) */
	omrstr_printf(fileName, sizeof(fileName), "lib%s.so", agentName);
#endif /* defined(OMR_OS_WINDOWS) */

	/* create the empty agent file with permission 751*/
	fileDescriptor = omrfile_open(fileName, EsOpenCreate | EsOpenWrite, 0751);
	ASSERT_NE(-1, fileDescriptor) << "omrfile_open \"" << fileName << "\" failed";
	omrfile_close(fileDescriptor);

	/* call omr_agent_create() by providing the empty agent file */
	struct OMR_Agent *agentC = omr_agent_create(&testVM.omrVM, agentName);
	ASSERT_FALSE(NULL == agentC) << "testAgent: createAgent() " << agentName << " failed";
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, omr_agent_openLibrary(agentC));

	/* call OMR_Agent::createAgent() by providing the empty agent file */
	OMR_Agent *agentCPP = OMR_Agent::createAgent(&testVM.omrVM, agentName);
	ASSERT_FALSE(NULL == agentCPP) << "testAgent: createAgent() failed";
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT, agentCPP->openLibrary());

	omrfile_unlink(fileName);
}
