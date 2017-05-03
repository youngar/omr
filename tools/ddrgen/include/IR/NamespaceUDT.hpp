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

#ifndef NAMESPACEUDT_HPP
#define NAMESPACEUDT_HPP

#include <string>

#include "Macro.hpp"
#include "UDT.hpp"

class NamespaceUDT : public UDT
{
public:
	std::vector<UDT *> _subUDTs;
	std::vector<Macro> _macros;

	NamespaceUDT(unsigned int lineNumber = 0);
	~NamespaceUDT();

	virtual bool equal(Type const& type, set<Type const*> *checked) const;
	virtual void replaceType(Type *typeToReplace, Type *replaceWith);

	virtual DDR_RC scanChildInfo(Scanner *scanner, void *data);
	virtual DDR_RC enumerateType(BlobGenerator *blobGenerator, bool addFieldsOnly);
	virtual DDR_RC buildBlob(BlobGenerator *blobGenerator, bool addFieldsOnly, string prefix);
	virtual DDR_RC printToSuperset(SupersetGenerator *supersetGenerator, bool addFieldsOnly, string prefix);
};

#endif /* NAMESPACEUDT_HPP */
