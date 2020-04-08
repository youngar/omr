/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMRGCTYPES_H_
#define OMRGCTYPES_H_

#include "omrcfg.h"

/**
 * omrobjecttoken_t represents the value stored or read from a slot in an
 * object.  A token must be decoded to a pointer before it can be used. It can
 * be decoded using GC_SlotObject::convertPointerFromToken.
 * 
 * In compressed references, object references are compressed from 64 bits into
 * 32 bit slots.  In order to use a slot's value, you must decompress the pointer,
 * changing the type from omrobjecttoken_t to omrobjectptr_t.
 */ 
#if defined(OMR_GC_FULL_POINTERS)
typedef uintptr_t omrobjecttoken_t;
#else /* defined(OMR_GC_FULL_POINTERS) */
typedef uint32_t omrobjecttoken_t;
#endif

#endif /* OMRGCTYPES_H_ */