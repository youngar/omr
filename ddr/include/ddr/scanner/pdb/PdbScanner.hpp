/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#ifndef PDBSCANNER_HPP
#define PDBSCANNER_HPP

#include <map>
#include "ddr/std/unordered_map.hpp"
#include <utility>
#include <vector>

#if defined(OMR_OS_WINDOWS)
/* windows.h defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win_
#include "dia2.h"
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#endif /* defined(OMR_OS_WINDOWS) */

#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/scanner/Scanner.hpp"
#include "ddr/ir/Symbol_IR.hpp"

using std::hash;
using std::ifstream;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

typedef struct PostponedType
{
	Type **type;
	string name;
} PostponedType;

class PdbScanner : public Scanner
{
public:
	virtual DDR_RC startScan(OMRPortLibrary *portLibrary, Symbol_IR *ir,
			vector<string> *debugFiles, const char *blacklistPath);

private:
	Symbol_IR *_ir;
	unordered_map<string, Type *> _typeMap;
	vector<PostponedType> _postponedFields;

	void addType(UDT *type, NamespaceUDT *outerUDT);
	DDR_RC addFieldMember(IDiaSymbol *symbol, ClassUDT *udt);
	DDR_RC setBaseType(IDiaSymbol *typeSymbol, Type **type);
	DDR_RC setBaseTypeFloat(ULONGLONG ulLen, Type **type);
	DDR_RC setBaseTypeInt(ULONGLONG ulLen, Type **type);
	DDR_RC setBaseTypeUInt(ULONGLONG ulLen, Type **type);
	DDR_RC setPointerType(IDiaSymbol *symbol, Modifiers *modifiers);
	DDR_RC setTypeUDT(IDiaSymbol *typeSymbol, Type **type, NamespaceUDT *outerUDT);
	DDR_RC setType(IDiaSymbol *symbol, Type **type, Modifiers *modifiers, NamespaceUDT *outerUDT);
	DDR_RC setTypeModifier(IDiaSymbol *symbol, Modifiers *modifiers);

	DDR_RC addSymbol(IDiaSymbol *symbol, NamespaceUDT *outerNamespace);
	DDR_RC setMemberOffset(IDiaSymbol *symbol, Field *newField);

	DDR_RC addChildrenSymbols(IDiaSymbol *symbol, enum SymTagEnum symTag, NamespaceUDT *outerUDT);
	DDR_RC addEnumMembers(IDiaSymbol *diaSymbol, EnumUDT *e);
	DDR_RC createClassUDT(IDiaSymbol *symbol, ClassUDT **newClass, NamespaceUDT *outerUDT);
	DDR_RC createEnumUDT(IDiaSymbol *symbol, NamespaceUDT *outerUDT);
	DDR_RC createTypedef(IDiaSymbol *symbol, NamespaceUDT *outerUDT);
	DDR_RC loadDataFromPdb(const wchar_t *filename, IDiaDataSource **diaDataSource, IDiaSession **diaSession, IDiaSymbol **diaSymbol);
	DDR_RC setSuperClassName(IDiaSymbol *symbol, ClassUDT *newUDT);
	void getNamespaceFromName(const string &name, NamespaceUDT **outerUDT);
	static DDR_RC getName(IDiaSymbol *symbol, string *name);
	static string getSimpleName(const string &name);
	static DDR_RC getSize(IDiaSymbol *symbol, ULONGLONG *size);
	Type *getType(const string &name);
	void initBaseTypeList();
	DDR_RC updatePostponedFieldNames();
};

#endif /* PDBSCANNER_HPP */
