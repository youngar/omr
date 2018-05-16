/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#ifndef CREATETESTHELPER_H_
#define CREATETESTHELPER_H_

#include <assert.h>
#include <stdlib.h>
#if defined(J9ZOS390)
#include "atoe.h"
#endif
#include <stdio.h>
#include <string.h>
#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "omrport.h"

#if defined(OMR_OS_WINDOWS)
#define SPEC_WIN_API
#endif /* defined(OMR_OS_WINDOWS) */

#if defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX)
#define SPEC_PTHREAD_API
#endif /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OSX) */

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif

#define J9THREAD_VERBOSE(x) omrthread_verboseCall(#x, (x))
#define PTHREAD_VERBOSE(x) pthread_verboseCall(#x, (x))

/* OS-specific values */
#if defined(AIXPPC) || defined(LINUX) || defined(OSX)
#define OS_PTHREAD_INHERIT_SCHED PTHREAD_INHERIT_SCHED
#define OS_PTHREAD_EXPLICIT_SCHED PTHREAD_EXPLICIT_SCHED
#define OS_PTHREAD_SCOPE_SYSTEM PTHREAD_SCOPE_SYSTEM
#define OS_SCHED_OTHER SCHED_OTHER
#define OS_SCHED_RR SCHED_RR
#define OS_SCHED_FIFO SCHED_FIFO

#else /* defined(AIXPPC) || defined(LINUX) || defined(OSX) */

#define OS_PTHREAD_INHERIT_SCHED (-1)
#define OS_PTHREAD_EXPLICIT_SCHED (-2)
#define OS_PTHREAD_SCOPE_SYSTEM (-1)
#define OS_SCHED_OTHER (-1)
#define OS_SCHED_RR (-2)
#define OS_SCHED_FIFO (-3)
#endif /* defined(AIXPPC) || defined(LINUX) || defined(OSX) */

#if defined(J9ZOS390)
#define OS_MEDIUM_WEIGHT __MEDIUM_WEIGHT
#define OS_HEAVY_WEIGHT __HEAVY_WEIGHT
#else
#define OS_MEDIUM_WEIGHT (-1)
#define OS_HEAVY_WEIGHT (-1)
#endif

/* return values indicating how a test case failed */
#define WRONG_NAME 0x1
#define WRONG_STACKSIZE 0x2
#define WRONG_SCHEDPOLICY 0x4
#define WRONG_PRIORITY 0x8
#define WRONG_OS_STACKSIZE 0x10
#define WRONG_OS_SCHEDPOLICY 0x20
#define WRONG_OS_PRIORITY 0x40
#define WRONG_OS_SCOPE 0x80
#define WRONG_OS_THREADWEIGHT 0x100
#define WRONG_OS_DETACHSTATE 0x200
#define WRONG_OS_INHERITSCHED 0x400
#define NULL_ATTR 0x1000
#define NONNULL_ATTR 0x2000
#define DESTROY_FAILED 0x4000
#define DESTROY_MODIFIED_PTR 0x8000
#define CREATE_FAILED 0x10000
#define EXPECTED_INVALID 0x20000
#define EXPECTED_VALID 0x40000
#define INIT_FAILED 0x80000
#define BAD_TEST 0x100000

/* ospriority.c */
extern void initPrioMap(void);
extern const char *mapOSPolicy(intptr_t policy);
#if defined(LINUX) || defined(OSX)
extern void initRealtimePrioMap(void);
#endif /* defined(LINUX) || defined(OSX) */

#if defined(LINUX) || defined(OSX)
extern int getRTPolicy(omrthread_prio_t priority);
#endif /* defined(LINUX) || defined(OSX) */

typedef int osprio_t;
extern osprio_t getOsPriority(omrthread_prio_t priority);


#endif /*CREATETESTHELPER_H_*/
