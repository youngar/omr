/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2018, 2018
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


 #include <stdint.h>
 #include <iostream>
 #include <fstream>

 #include "infra/Assert.hpp"
 #include "ilgen/JitBuilderReplay.hpp"
 #include "ilgen/MethodBuilderReplay.hpp"


 OMR::JitBuilderReplay::JitBuilderReplay()
    {
    }

 OMR::JitBuilderReplay::~JitBuilderReplay()
    {
    }

void
OMR::JitBuilderReplay::StoreReservedIDs()
   {
      StoreIDPointerPair(0, 0);
      StoreIDPointerPair(1, (const void *)1);
   }

void
OMR::JitBuilderReplay::initializeMethodBuilder(TR::MethodBuilderReplay * replay)
   {
       _mb = replay;
       StoreIDPointerPair(2, _mb);
   }

void
OMR::JitBuilderReplay::StoreIDPointerPair(TypeID ID, TypePointer ptr)
   {
     TypeMapPointer::iterator it = _pointerMap.find(ID);
     if (it != _pointerMap.end())
        TR_ASSERT_FATAL(0, "Unexpected pointer already defined for ID %d", ID);

     _pointerMap.insert(std::make_pair(ID, ptr));
   }
