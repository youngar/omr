/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#if defined(OMR_OS_WINDOWS)

#ifdef BOOLEAN
/* There is a collision between J9's definition of BOOLEAN and Windows headers */
#define BOOLEAN_COLLISION_DETECTED BOOLEAN
#undef BOOLEAN
#endif /* defined(OMR_OS_WINDOWS) */

#ifdef boolean
/* There is a collision between J9's definition of boolean and Windows headers */
#define boolean_COLLISION_DETECTED boolean
#undef boolean
#endif

#define WINDOWS_API_INCLUDED 1
#define NOMINMAX 1
#include <Windows.h>

#ifdef boolean_COLLISION_DETECTED
#define boolean bool
#undef boolean_COLLISION_DETECTED
#endif

#ifdef BOOLEAN_COLLISION_DETECTED
#define BOOLEAN UDATA
#undef BOOLEAN_COLLISION_DETECTED
#endif

#endif
