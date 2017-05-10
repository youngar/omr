/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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

#ifndef IA32FPCONVERSIONSNIPPET_INCL
#define IA32FPCONVERSIONSNIPPET_INCL

#include <stdint.h>                      // for uint8_t, int32_t, uint32_t
#include "codegen/RealRegister.hpp"      // for toRealRegister
#include "codegen/Snippet.hpp"           // for TR::X86Snippet::Kind, etc
#include "il/SymbolReference.hpp"        // for SymbolReference
#include "infra/Assert.hpp"              // for TR_ASSERT
#include "x/codegen/RestartSnippet.hpp"  // for TR::X86RestartSnippet
#include "codegen/X86Instruction.hpp"  // for TR::X86RegInstruction, etc

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class X86FPConversionSnippet : public TR::X86RestartSnippet
   {
   TR::SymbolReference *_helperSymRef;

   public:

   X86FPConversionSnippet(TR::CodeGenerator   *codeGen,
                          TR::Node            *node,
                          TR::LabelSymbol      *restartlab,
                          TR::LabelSymbol      *snippetlab,
                          TR::SymbolReference *helperSymRef)
      : TR::X86RestartSnippet(codeGen, node, restartlab, snippetlab, helperSymRef->canCauseGC()),
           _helperSymRef(helperSymRef)
      {
      // The code generation for this snippet does not allow a proper GC map
      // to be built, so assert that GC will not happen.
      //
      TR_ASSERT(!helperSymRef->canCauseGC(), "assertion failure");
      }

   TR::SymbolReference *getHelperSymRef()                      {return _helperSymRef;}
   TR::SymbolReference *setHelperSymRef(TR::SymbolReference *s) {return (_helperSymRef = s);}

   uint8_t *emitCallToConversionHelper(uint8_t *buffer);
   virtual uint8_t *emitSnippetBody();
   virtual uint8_t *genFPConversion(uint8_t *buffer) = 0;
   virtual Kind getKind() {return IsFPConversion;}
   };


class X86FPConvertToIntSnippet  : public TR::X86FPConversionSnippet
   {
   TR::X86RegInstruction  *_convertInstruction;

   public:

   X86FPConvertToIntSnippet(TR::LabelSymbol            *restartlab,
                            TR::LabelSymbol            *snippetlab,
                            TR::SymbolReference       *helperSymRef,
                            TR::X86RegInstruction     *convertInstr,
                            TR::CodeGenerator *codeGen)
      : TR::X86FPConversionSnippet(codeGen, convertInstr->getNode(), restartlab, snippetlab, helperSymRef),
           _convertInstruction(convertInstr) {}

   TR::X86RegInstruction  * getConvertInstruction() {return _convertInstruction;}
   uint8_t *genFPConversion(uint8_t *buffer);
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual Kind getKind() {return IsFPConvertToInt;}
   };


class X86FPConvertToLongSnippet  : public TR::X86FPConversionSnippet
   {
   TR::X86RegMemInstruction          *_loadHighInstruction,
                                    *_loadLowInstruction;
   TR::X86FPST0STiRegRegInstruction  *_clobberInstruction;
   TR::RealRegister               *_lowRegister,
                                    *_highRegister,
                                    *_doubleRegister;
   uint8_t                           _action;

   void analyseLongConversion();

   public:

   static const uint8_t              _registerActions[16];

   enum actionFlags
      {
      kXCHG        = 0x01,
      kMOVHigh     = 0x02,
      kMOVLow      = 0x04,
      kPreserveEDX = 0x08,
      kPreserveEAX = 0x10,
      kNeedFXCH    = 0x80
      };

   X86FPConvertToLongSnippet(TR::LabelSymbol                    *restartlab,
                             TR::LabelSymbol                    *snippetlab,
                             TR::SymbolReference               *helperSymRef,
                             TR::X86FPST0STiRegRegInstruction  *clobInstr,
                             TR::X86RegMemInstruction          *loadHighInstr,
                             TR::X86RegMemInstruction          *loadLowInstr,
                             TR::CodeGenerator *codeGen)
      : TR::X86FPConversionSnippet(codeGen, clobInstr->getNode(), restartlab, snippetlab, helperSymRef),
           _loadHighInstruction(loadHighInstr),
           _loadLowInstruction(loadLowInstr),
           _clobberInstruction(clobInstr),
           _lowRegister(0),
           _highRegister(0),
           _doubleRegister(0),
           _action(0) {}

   uint8_t getAction() { return _action; }

   TR::RealRegister * getLowRegister()   { return _lowRegister; }
   TR::RealRegister * getHighRegister()   { return _highRegister; }
   TR::RealRegister * getDoubleRegister() { return _doubleRegister; }

   uint8_t *genFPConversion(uint8_t *buffer);
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual Kind getKind() {return IsFPConvertToLong;}
   };

}

#endif
