/*******************************************************************************
 * Copyright (c) 2014, 2020 IBM Corp. and others
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

#if !defined(OBJECTITERATOR_HPP_)
#define OBJECTITERATOR_HPP_

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "modronbase.h"

#include "Base.hpp"
#include "GCExtensionsBase.hpp"
#include "objectdescription.h"
#include "ObjectModel.hpp"
#include "SlotObject.hpp"

class GC_ObjectIterator
{
/* Data Members */
private:
protected:
	GC_SlotObject _slotObject; /**< Create own SlotObject class to provide output */
	fomrobject_t *_scanPtr; /**< current scan pointer */
	fomrobject_t *_endPtr; /**< end scan pointer */
#if defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS)
	bool _compressed; /**< If compressed pointers is enabled */
#endif /* defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS) */
public:

/* Member Functions */
private:
	MMINLINE bool
	compressed()
	{
#if defined(OMR_GC_FULL_POINTERS)
#if defined(OMR_GC_COMPRESSED_POINTERS)
		return _compressed;
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return false;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#else /* defined(OMR_GC_FULL_POINTERS) */
		return true;
#endif /* defined(OMR_GC_FULL_POINTERS) */
	}

	MMINLINE void
	initialize(OMR_VM *omrVM, omrobjectptr_t objectPtr)
	{
		MM_GCExtensionsBase *extensions = (MM_GCExtensionsBase *)omrVM->_gcOmrVMExtensions;

#if defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS)
		_compressed = extensions->compressObjectReferences();
#endif /* defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS) */

		/* Start _scanPtr after header */
		_scanPtr = extensions->objectModel.getHeadlessObject(objectPtr);
		_endPtr = extensions->objectModel.getEnd(objectPtr);
	}

protected:
public:

	/**
	 * Return back SlotObject for next reference
	 * @return SlotObject
	 */
	MMINLINE GC_SlotObject *nextSlot()
	{
		if (_scanPtr < _endPtr) {
			_slotObject.writeAddressToSlot(_scanPtr);
			GC_SlotObject::addToSlotAddress(_scanPtr, 1, compressed());
			return &_slotObject;
		}
		return NULL;
	}

	/**
	 * Restore iterator state: nextSlot() will be at this index
	 * @param index[in] The index of slot to be restored
	 */
	MMINLINE void restore(int32_t index)
	{
		_scanPtr = GC_SlotObject::addToSlotAddress(_scanPtr, index, compressed());
	}

	/**
	 * @param omrVM[in] pointer to the OMR VM
	 * @param objectPtr[in] the object to be processed
	 */
	GC_ObjectIterator(OMR_VM *omrVM, omrobjectptr_t objectPtr)
		: _slotObject(GC_SlotObject(omrVM, NULL))
		, _scanPtr(NULL)
		, _endPtr(NULL)
#if defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS)
		, _compressed(FALSE)
#endif /* defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS) */
	{
		initialize(omrVM, objectPtr);
	}
};

#endif /* OBJECTITERATOR_HPP_ */
