/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH
 *Assembly-exception
 *******************************************************************************/

#if !defined(OBJECT_HPP_)
#define OBJECT_HPP_

#include "omrcfg.h"

#include <cstdlib>
#include <stdint.h>

/**
 * Represents if references are in compressed or full pointer mode
 */
enum RefMode
{
#if defined(OMR_GC_FULL_POINTERS)
	FULL,
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
	COMP,
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
};

class Model
{
public:
	Model() {}

	Model(bool compressed)
	{
#if defined(OMR_GC_FULL_POINTERS)
#if defined(OMR_GC_COMPRESSED_POINTERS)
		_mode = FULL;
		if (compressed) {
			_mode = COMP;
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#endif /* defined(OMR_GC_FULL_POINTERS) */
	}

	RefMode
	mode() const
	{
#if defined(OMR_GC_FULL_POINTERS)
#if defined(OMR_GC_COMPRESSED_POINTERS)
	return _mode;
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
	return FULL;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#else /* defined(OMR_GC_FULL_POINTERS) */
	return COMP;
#endif /* defined(OMR_GC_FULL_POINTERS) */
	}

private:
#if defined(OMR_GC_FULL_POINTERS)
#if defined(OMR_GC_COMPRESSED_POINTERS)
	RefMode _mode;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#endif /* defined(OMR_GC_FULL_POINTERS) */
};

/**
 * The GC flags in the ObjectHeader
 */
typedef uint8_t ObjectFlags;


/**
 * The size of an object in bytes.
 */
#if defined(OMR_GC_FULL_POINTERS)
typedef uintptr_t ObjectSize;
#else /* defined(OMR_GC_FULL_POINTERS) */
typedef uint32_t ObjectSize;
#endif


/**
 * The header of an object. Stores the GC flags and object size
 */
template <RefMode M>
struct ObjectHeader;

#if defined(OMR_GC_FULL_POINTERS)

template <>
struct ObjectHeader<FULL>
{
	uintptr_t _value;
};

#endif /* defined(OMR_GC_FULL_POINTERS) */

#if defined(OMR_GC_COMPRESSED_POINTERS)

template <>
struct ObjectHeader<COMP>
{
	uint32_t _value;
};

#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

/**
 * A slot in the object.
 */
template <RefMode M>
struct Slot;

#if defined(OMR_GC_FULL_POINTERS)

template<>
struct Slot<FULL>
{
	uintptr_t _value;
};

#endif /* defined(OMR_GC_FULL_POINTERS) */

#if defined(OMR_GC_COMPRESSED_POINTERS)

template<>
struct Slot<COMP>
{
	uint32_t _value;
};

#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

class ObjectBase
{
public:
	static uintptr_t getObjectHeaderSlotOffset() { return HEADER_OFFSET; }

	static uintptr_t getObjectHeaderSlotFlagsShift() { return HEADER_FLAGS_SHIFT; }

	static uintptr_t getObjectHeaderSlotSizeShift() { return HEADER_SIZE_SHIFT; }

private:

	static const uintptr_t HEADER_OFFSET = 0;
	static const uintptr_t HEADER_FLAGS_SHIFT = 0;
	static const uintptr_t HEADER_SIZE_SHIFT = 8;
};

/**
 * A header containing basic information about an object. Contains the object's size and an 8 bit object flag.
 * The size and flags are masked together into a single fomrobjectptr_t.
 */
template <RefMode M>
class Object : public ObjectBase
{
public:
	static ObjectSize 
	allocSize(ObjectSize nslots)
	{
		return ObjectSize(sizeof(ObjectHeader<M>) + sizeof(Slot<M>) * nslots);
	}

	Object(ObjectSize sizeInBytes, ObjectFlags flags = 0) 
	{
		assign(sizeInBytes, flags);
	}

	ObjectFlags flags() const { return _header._value; }

	void flags(ObjectFlags value) { assign(sizeInBytes(), value); }

	ObjectSize sizeInBytes() const { return _header._value >> SIZE_SHIFT; }

	void sizeInBytes(ObjectSize value) { assign(value, flags()); }

	static size_t sizeOfHeaderInBytes() { return sizeof(ObjectHeader<M>); }

	size_t sizeOfSlotsInBytes() const { return sizeInBytes() - sizeOfHeaderInBytes(); }

	size_t slotCount() const { return sizeOfSlotsInBytes() / sizeof(Slot<M>); }

	Slot<M>* slots() { return ((Slot<M>*)(ObjectHeader<M> *)this + 1); }

	fomrobject_t* begin() { return (fomrobject_t *)slots(); }

	fomrobject_t* end() { return (fomrobject_t *)(slots() + slotCount()); }

private:
	void assign(ObjectSize sizeInBytes, ObjectFlags flags) { _header._value = (sizeInBytes << SIZE_SHIFT) | flags; }

	static const size_t SIZE_SHIFT = sizeof(ObjectFlags) * 8;
	
	ObjectHeader<M> _header;
};

#if defined(OMR_GC_COMPRESSED_POINTERS)
inline Object<COMP> *
toComp(ObjectBase *object)
{
	return static_cast<Object<COMP>*>(object);
}
#endif

#if defined(OMR_GC_FULL_POINTERS)
inline Object<FULL> *
toFull(ObjectBase *object)
{
	return static_cast<Object<FULL>*>(object);
}
#endif

class ObjectHandle
{
public:
	ObjectHandle(ObjectBase *object, RefMode mode)
		: _object(object), _mode(mode) {}

	static ObjectSize 
	allocSize(RefMode mode, ObjectSize nslots)
	{
		switch(mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return Object<FULL>::allocSize(nslots);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return Object<COMP>::allocSize(nslots);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	static uintptr_t
	sizeOfHeaderInBytes(RefMode mode)
	{
		switch(mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return Object<FULL>::sizeOfHeaderInBytes();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return Object<COMP>::sizeOfHeaderInBytes();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	fomrobject_t *
	begin() const {
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return (fomrobject_t *)toFull(_object)->begin();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return (fomrobject_t *)toComp(_object)->begin();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return NULL;
		}
	}

	fomrobject_t *
	end() const {
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return (fomrobject_t *)toFull(_object)->end();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return (fomrobject_t *)toComp(_object)->end();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return NULL;
		}
	}

	ObjectSize
	sizeInBytes() const
	{
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return toFull(_object)->sizeInBytes();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return toComp(_object)->sizeInBytes();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	void
	sizeInBytes(ObjectSize objectSize)
	{
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return toFull(_object)->sizeInBytes(objectSize);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return toComp(_object)->sizeInBytes(objectSize);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return;
		}
	}

	ObjectFlags
	flags() const
	{
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return toFull(_object)->flags();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return toComp(_object)->flags();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	void
	flags(ObjectFlags objectFlags)
	{
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return toFull(_object)->flags(objectFlags);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return toComp(_object)->flags(objectFlags);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return;
		}
	}

	size_t 
	slotCount() const
	{
		switch(_mode) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return toFull(_object)->slotCount();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return toComp(_object)->slotCount();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	size_t
	sizeOfSlotsInBytes() const
	{
		return sizeInBytes() - sizeOfHeaderInBytes(_mode);
	}

private:
	ObjectBase *_object;
	RefMode _mode;
};

#endif /* OBJECT_HPP_ */
