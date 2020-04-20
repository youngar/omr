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
enum PointerMode
{
#if defined(OMR_GC_FULL_POINTERS)
	FULL,
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
	COMP,
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
};

/**
 * Answers properties about the format of references between objects.
 */
class PointerModel
{
public:
	PointerModel() {}

#if defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS)
	PointerModel(bool compressed) : _mode(compressed ? COMP : FULL) {}
#else /* defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS) */
	PointerModel(bool) {}
#endif /* defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS) */

	PointerMode
	mode() const
	{
#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
		return _mode;
#else /* defined(OMR_GC_FULL_POINTERS) */
		return COMP;
#endif /* defined(OMR_GC_FULL_POINTERS) */
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return FULL;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	}

private:
#if defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS)
	PointerMode _mode;
#endif /* defined(OMR_GC_FULL_POINTERS) && defined(OMR_GC_COMPRESSED_POINTERS) */
};

/**
 * The GC flags in the ObjectHeader.
 */
typedef uint8_t ObjectFlags;

/**
 * The size of an object in bytes.
 */
#if defined(OMR_GC_FULL_POINTERS)
typedef uintptr_t ObjectSize;
#else /* defined(OMR_GC_FULL_POINTERS) */
typedef uint32_t ObjectSize;
#endif /* defined(OMR_GC_FULL_POINTERS) */

#define OBJECT_HEADER_FLAGS_SHIFT 0
#define OBJECT_HEADER_SIZE_SHIFT 8

/**
 * A header containing basic information about an object. Contains the object's size and an 8 bit object flag.
 * The size and flags are masked together into a single value of size T.
 */
template<typename T>
class ObjectHeaderBase
{
public:
	ObjectHeaderBase() : _value(0) {}

	ObjectHeaderBase(T value) : _value(value) {}

	ObjectHeaderBase(ObjectSize size, ObjectFlags flags)
	{
		assign(size, flags);
	}

	ObjectFlags flags() const { return (ObjectFlags)_value; }

	void flags(ObjectFlags value) { assign(objectSizeInBytes(), value); }

	ObjectSize objectSizeInBytes() const { return _value >> OBJECT_HEADER_SIZE_SHIFT; }

	void objectSizeInBytes(ObjectSize value) { assign(value, flags()); }

private:
	void assign(ObjectSize sizeInBytes, ObjectFlags flags) { _value = (sizeInBytes << OBJECT_HEADER_SIZE_SHIFT) | flags; }

	T _value;
};

/**
 * The header of an object. Stores the GC flags and object size.
 */
template <PointerMode M>
class ObjectHeader;

#if defined(OMR_GC_FULL_POINTERS)
template <>
class ObjectHeader<FULL> : public ObjectHeaderBase<uintptr_t>
{
public:
	using ObjectHeaderBase::ObjectHeaderBase;
};
#endif /* defined(OMR_GC_FULL_POINTERS) */

#if defined(OMR_GC_COMPRESSED_POINTERS)
template <>
class ObjectHeader<COMP> : public ObjectHeaderBase<uint32_t>
{
public:
	using ObjectHeaderBase::ObjectHeaderBase;
};
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

/**
 * A slot in the object.
 */
template <PointerMode M>
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

template <PointerMode>
class Object;

class ObjectBase
{
public:
#if defined(OMR_GC_FULL_POINTERS)
	Object<FULL> *toFull();
	const Object<FULL> *toFull() const;
#endif /* defined(OMR_GC_FULL_POINTERS) */

#if defined(OMR_GC_COMPRESSED_POINTERS)
	Object<COMP> *toComp();
	const Object<COMP> *toComp() const;
#endif /* OMR_GC_COMPRESSED_POINTERS */
};

template <PointerMode M>
class Object : public ObjectBase
{
public:
	static ObjectSize
	allocSize(ObjectSize nslots)
	{
		return ObjectSize(sizeof(ObjectHeader<M>) + (sizeof(Slot<M>) * nslots));
	}

	Object(ObjectSize sizeInBytes, ObjectFlags flags = 0) : _header(sizeInBytes, flags) {}

	ObjectFlags flags() const { return _header.flags(); }

	void flags(ObjectFlags value) { _header.flags(value); }

	ObjectSize sizeInBytes() const { return _header.objectSizeInBytes(); }

	void sizeInBytes(ObjectSize value) { _header.objectSizeInBytes(value); }

	size_t sizeOfSlotsInBytes() const { return sizeInBytes() - sizeof(ObjectHeader<M>); }

	size_t slotCount() const { return sizeOfSlotsInBytes() / sizeof(Slot<M>); }

	Slot<M>* slots() { return ((Slot<M>*)(ObjectHeader<M> *)this + 1); }

	fomrobject_t* begin() { return (fomrobject_t *)slots(); }

	fomrobject_t* end() { return (fomrobject_t *)(slots() + slotCount()); }

private:
	ObjectHeader<M> _header;
};

#if defined(OMR_GC_FULL_POINTERS)
inline Object<FULL> *
ObjectBase::toFull()
{
	return static_cast<Object<FULL>*>(this);
}

inline const Object<FULL> *
ObjectBase::toFull() const
{
	return static_cast<const Object<FULL>*>(this);
}
#endif /* defined(OMR_GC_FULL_POINTERS) */

#if defined(OMR_GC_COMPRESSED_POINTERS)
inline Object<COMP> *
ObjectBase::toComp()
{
	return static_cast<Object<COMP>*>(this);
}

inline const Object<COMP> *
ObjectBase::toComp() const
{
	return static_cast<Object<COMP>*>(object);
}
#endif /* OMR_GC_COMPRESSED_POINTERS */

inline ObjectSize
objectAllocSize(PointerModel pointerModel, size_t nslots)
{
	switch(pointerModel.mode()) {
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

inline uintptr_t
objectHeaderSizeInBytes(PointerModel pointerModel)
{
	switch(pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
	case FULL:
		return sizeof(ObjectHeader<FULL>);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
	case COMP:
		return sizeof(ObjectHeader<COMP>);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	default:
		return 0;
	}
}

class ObjectHandle
{
public:
	ObjectHandle(ObjectBase *object, PointerModel pointerModel)
		: _object(object), _pointerModel(pointerModel) {}

	fomrobject_t *
	begin() const
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->begin();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->begin();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return NULL;
		}
	}

	fomrobject_t *
	end() const
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->end();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->end();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return NULL;
		}
	}

	ObjectSize
	sizeInBytes() const
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->sizeInBytes();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->sizeInBytes();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	void
	sizeInBytes(ObjectSize objectSize)
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->sizeInBytes(objectSize);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->sizeInBytes(objectSize);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return;
		}
	}

	ObjectFlags
	flags() const
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->flags();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->flags();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	void
	flags(ObjectFlags objectFlags)
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->flags(objectFlags);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->flags(objectFlags);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return;
		}
	}

	size_t 
	slotCount() const
	{
		switch(_pointerModel.mode()) {
#if defined(OMR_GC_FULL_POINTERS)
		case FULL:
			return _object->toFull()->slotCount();
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
		case COMP:
			return _object->toComp()->slotCount();
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		default:
			return 0;
		}
	}

	size_t
	sizeOfSlotsInBytes() const
	{
		return sizeInBytes() - objectHeaderSizeInBytes(_pointerModel);
	}

private:
	ObjectBase *_object;
	PointerModel _pointerModel;
};

#endif /* OBJECT_HPP_ */
