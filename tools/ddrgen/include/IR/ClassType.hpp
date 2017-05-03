/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef CLASSTYPE_HPP
#define CLASSTYPE_HPP

#include "EnumMember.hpp"
#include "Field.hpp"
#include "NamespaceUDT.hpp"
#include "UDT.hpp"

using std::vector;

class ClassType : public NamespaceUDT
{
public:
	vector<Field *> _fieldMembers;
	vector<EnumMember *> _enumMembers; /* used for anonymous enums*/

	ClassType(SymbolType symbolType, size_t size, unsigned int lineNumber = 0);
	virtual ~ClassType();

	virtual bool isAnonymousType();
	virtual bool equal(Type const& type, set<Type const*> *checked) const;
	virtual void replaceType(Type *typeToReplace, Type *replaceWith);
};

#endif /* CLASSTYPE_HPP */
