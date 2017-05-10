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

#include <stddef.h>                                  // for NULL
#include <stdint.h>                                  // for int32_t, etc
#include "codegen/AheadOfTimeCompile.hpp"            // for AheadOfTimeCompile
#include "codegen/CodeGenerator.hpp"                 // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                      // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"                    // for InstOpCode, etc
#include "codegen/Instruction.hpp"                   // for Instruction, etc
#include "codegen/Linkage.hpp"                       // for addDependency
#include "codegen/Machine.hpp"                       // for Machine, etc
#include "codegen/MemoryReference.hpp"               // for MemoryReference, etc
#include "codegen/RealRegister.hpp"                  // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"                      // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"                   // for Compilation, etc
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                            // for intptrj_t, uintptrj_t
#include "il/DataTypes.hpp"                          // for TR::DataType
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                              // for ILOpCode
#include "il/Node.hpp"                               // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                             // for Symbol
#include "il/SymbolReference.hpp"                    // for SymbolReference
#include "il/TreeTop.hpp"                            // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/MethodSymbol.hpp"                // for MethodSymbol
#include "il/symbol/StaticSymbol.hpp"                // for StaticSymbol
#include "infra/Assert.hpp"                          // for TR_ASSERT
#include "infra/List.hpp"                            // for List
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCAOTRelocation.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"               // for Op_load, Op_st
#include "p/codegen/PPCTableOfConstants.hpp"         // for PTOC_FULL_INDEX, etc
#include "runtime/Runtime.hpp"

class TR_OpaqueClassBlock;

static TR::RealRegister::RegNum choose_rX(TR::Instruction *, TR::RealRegister *);


OMR::Power::MemoryReference::MemoryReference(
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _modBase(NULL), _length(0),
   _staticRelocation(NULL),
   _unresolvedSnippet(NULL),
   _conditions(NULL),
   _flag(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }

OMR::Power::MemoryReference::MemoryReference(
      TR::Register *br,
      TR::Register *ir,
      uint8_t len,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(ir),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _staticRelocation(NULL),
   _conditions(NULL),
   _length(len),
   _flag(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }

OMR::Power::MemoryReference::MemoryReference(
      TR::Register *br,
      int32_t disp,
      uint8_t len,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _staticRelocation(NULL),
   _conditions(NULL),
   _length(len),
   _offset(disp),
   _flag(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   }

//
// constructor called to generate a memory reference for all kinds of
// stores and non-constant(or address) loads
//

OMR::Power::MemoryReference::MemoryReference(TR::Node *rootLoadOrStore, uint32_t len, TR::CodeGenerator *cg)
   : _baseRegister(NULL), _indexRegister(NULL), _unresolvedSnippet(NULL),
     _modBase(NULL), _flag(0), _baseNode(NULL), _indexNode(NULL),
     _symbolReference(rootLoadOrStore->getSymbolReference()), _offset(0),
     _conditions(NULL), _length(len), _staticRelocation(NULL)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *ref = rootLoadOrStore->getSymbolReference();
   TR::Symbol   *symbol = ref->getSymbol();
   bool        isStore = rootLoadOrStore->getOpCode().isStore();
   self()->checkRegisters(cg);

   TR::MethodSymbol * methodSymbol = rootLoadOrStore->getSymbol()->getMethodSymbol();
   if (rootLoadOrStore->getOpCode().isIndirect())
      {
      TR::Node *base = rootLoadOrStore->getFirstChild();
      bool     isLocalObject = ((base->getOpCodeValue() == TR::loadaddr) &&
                                base->getSymbol()->isLocalObject());

      if (!ref->isUnresolved() && isLocalObject)
         {
         _baseRegister = cg->getStackPointerRegister();
         _symbolReference = base->getSymbolReference();
         self()->checkRegisters(cg);
         _baseNode = base;
         }
      else
         {
         if (ref->isUnresolved())
            {
            // Stack late mapping forces us to evaluate local object's base into a register
            // rather than getting the stack pointer as the base which is not correct.
            if (isLocalObject)
               cg->evaluate(base);

            self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, ref, isStore, false));
            cg->addSnippet(self()->getUnresolvedSnippet());
            }
         // if an aconst feeds a iaload, we need to load the constant
         if (base->getOpCode().isLoadConst())
            {
            cg->evaluate(base);
            }
         if (symbol->isMethodMetaData())
            {
            _baseRegister = cg->getMethodMetaDataRegister();
            }
         self()->populateMemoryReference(base, cg);
         }
      }
   else
      {
      if (symbol->isStatic())
         {
         self()->accessStaticItem(rootLoadOrStore, ref, isStore, cg);
         }
      else
         {
         if (!symbol->isMethodMetaData())
            { // must be either auto or parm or error.
            _baseRegister = cg->getStackPointerRegister();
            }
         else
            {
            _baseRegister = cg->getMethodMetaDataRegister();
            }
         }
      }

   if (TR::Compiler->target.is32Bit() || !symbol->isConstObjectRef())
      self()->addToOffset(rootLoadOrStore, ref->getOffset(), cg);
   if (self()->getUnresolvedSnippet() != NULL)
      self()->adjustForResolution(cg);
   if (symbol->isParm())
      {
      //        setOffset(getOffset() + cg->getLinkage()->calculateActualParameterOffset(0, *symbol->castToParmSymbol()));
      }
   // TODO: aliasing sets?
   }

OMR::Power::MemoryReference::MemoryReference(TR::Node *node, TR::SymbolReference *symRef, uint32_t len, TR::CodeGenerator *cg)
   : _baseRegister(NULL), _indexRegister(NULL), _unresolvedSnippet(NULL),
     _modBase(NULL), _flag(0), _baseNode(NULL), _indexNode(NULL),
     _symbolReference(symRef), _offset(0),
     _conditions(NULL), _length(len), _staticRelocation(NULL)
   {
   TR::Symbol *symbol = symRef->getSymbol();


   if (symbol->isStatic())
      {
      self()->accessStaticItem(node, symRef, false, cg);
      }

   if (symbol->isRegisterMappedSymbol())
      {
      if (!symbol->isMethodMetaData())
         { // must be either auto or parm or error.
         _baseRegister = cg->getStackPointerRegister();
         }
      else
         {
         _baseRegister = cg->getMethodMetaDataRegister();
         }
      }

   if (TR::Compiler->target.is32Bit() || !symbol->isConstObjectRef())
      self()->addToOffset(0, symRef->getOffset(), cg);
   if (self()->getUnresolvedSnippet() != NULL)
      self()->adjustForResolution(cg);
   // TODO: aliasing sets?
   }


OMR::Power::MemoryReference::MemoryReference(TR::Node *node, TR::MemoryReference& mr, int32_t displacement, uint32_t len, TR::CodeGenerator *cg)
   {
   _baseRegister      = mr._baseRegister;
   _baseNode          = NULL;
   _indexRegister     = mr._indexRegister;
   _indexNode         = NULL;
   _modBase           = mr._modBase;
   _symbolReference   = mr._symbolReference;
   _offset            = mr.getOffset();
   _flag              = 0;
   _length            = len;
   _staticRelocation  = NULL;
   _conditions        = NULL;
   mr._flag           = 0;

   TR_ASSERT(mr.getStaticRelocation() == NULL, "Relocated memory reference should not be re-used.");

   if (mr.getUnresolvedSnippet() != NULL)
      {
      _unresolvedSnippet = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, _symbolReference, mr.getUnresolvedSnippet()->isUnresolvedStore(), false);
      cg->addSnippet(_unresolvedSnippet);

      self()->adjustForResolution(cg);
      }
   else
      {
      _unresolvedSnippet = NULL;
      }
   self()->addToOffset(0, displacement, cg);
   }

bool OMR::Power::MemoryReference::useIndexedForm()
   {
   return((_indexRegister==NULL && !self()->isUsingDelayedIndexedForm()) ? false : true );
   }

bool OMR::Power::MemoryReference::isTOCAccess()
   {
   // TODO: add checking for other type of TOC usages.
   return(self()->isUsingStaticTOC());
   }

int32_t OMR::Power::MemoryReference::getTOCOffset()
   {
   // TODO: add checking for other type of TOC usages.
   if (self()->isUsingStaticTOC())
      {
      return _symbolReference->getSymbol()->getStaticSymbol()->getTOCIndex() * sizeof(intptrj_t);
      }
   return 0;
   }

void OMR::Power::MemoryReference::addToOffset(TR::Node * node, intptrj_t amount, TR::CodeGenerator *cg)
   {
   // in compressedRefs mode, amount maybe heapBase constant, which in
   // most cases is quite large
   //
   TR::Compilation *comp = cg->comp();
   if (self()->getUnresolvedSnippet() != NULL &&
         (amount > LOWER_IMMED && amount < UPPER_IMMED))
      {
      self()->setOffset(self()->getOffset() + amount);
      return;
      }

   if (amount == 0)
      return;

   if (_baseRegister!=NULL && _indexRegister!=NULL)
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   intptrj_t displacement = self()->getOffset() + amount;
   if (displacement<LOWER_IMMED || displacement>UPPER_IMMED)
      {
      TR::Register  *newBase;
      intptrj_t     upper, lower;

      self()->setOffset(0);
      lower = displacement & 0x0000ffff;
      upper = displacement >> 16;
      if (lower & (1<<15))
         upper++;
      if (_baseRegister!=NULL && self()->isBaseModifiable())
         newBase = _baseRegister;
      else
         {
         newBase = cg->allocateRegister();

         if (_baseRegister && _baseRegister->containsInternalPointer())
            {
            newBase->setContainsInternalPointer();
            newBase->setPinningArrayPointer(_baseRegister->getPinningArrayPointer());
            }
         }

      if (!node)
         node = comp->getAppendInstruction()->getNode();

      if (_baseRegister != NULL)
         {
         if (node->getOpCode().isLoadConst() && node->getRegister())
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, newBase, _baseRegister, node->getRegister());
            }
         else
            {
            if (TR::Compiler->target.is64Bit() && (upper<LOWER_IMMED || upper>UPPER_IMMED))
               {
               TR::Register *tempReg = cg->allocateRegister();
               loadActualConstant(cg, node, upper<<16, tempReg);
               generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, newBase, _baseRegister, tempReg);
               cg->stopUsingRegister(tempReg);
               }
            else
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, newBase, _baseRegister, upper);
            if (lower != 0)
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, newBase, newBase, lower);
            }
         }
      else
         loadActualConstant(cg, node, displacement, newBase);

      // the following "if" is just to avoid stopUsingRegister being called
      // for newBase by decNodeReferenceCounts(cg);
      if (_baseRegister == newBase && _baseNode == NULL) _baseRegister = NULL;

      self()->decNodeReferenceCounts(cg);
      _baseRegister = newBase;
      _baseNode = NULL;
      self()->setBaseModifiable();
      }
   else
      self()->setOffset(displacement);
   }

void OMR::Power::MemoryReference::forceIndexedForm(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction *cursor)
   {
   if (self()->useIndexedForm())
      return;  // already X-form

   TR_ASSERT(!self()->isTOCAccess(), "X-form TOC access not supported");

   if (self()->getUnresolvedSnippet() != NULL)
      {
      // nothing to do except force an X-form at binary encoding time
      self()->setUsingDelayedIndexedForm();
      return;
      }

   if (self()->hasDelayedOffset())
      {
      self()->setUsingDelayedIndexedForm();
      if (!self()->isBaseModifiable())
         {
         _indexRegister = cg->allocateRegister();  // for encoding time use
         self()->setIndexModifiable();
         }
      return;
      }

   // true displacement available now
   intptrj_t displacement = self()->getOffset();

   if (displacement == 0)
      {
      // force use of gr0 as RA in addressing
      _indexRegister = _baseRegister;
      _indexNode = _baseNode;
      if (self()->isBaseModifiable())
         {
         self()->setIndexModifiable();
         self()->clearBaseModifiable();
         }
      _baseRegister = NULL;
      _baseNode = NULL;
      return;
      }

   // load displacement into a register, make it the index
   self()->setOffset(0);
   TR::Register  *newIndex = cg->allocateRegister();
   if (!cursor)
      cursor = cg->comp()->getAppendInstruction();
   if (!node)
      node = cursor->getNode();
   loadActualConstant(cg, node, displacement, newIndex, cursor);
   _indexRegister = newIndex;
   _indexNode = NULL;
   self()->setIndexModifiable();
   cg->stopUsingRegister(newIndex);
   }

void OMR::Power::MemoryReference::adjustForResolution(TR::CodeGenerator *cg)
   {
   _modBase = cg->allocateRegister();
   _conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   addDependency(_conditions, _modBase, TR::RealRegister::gr11, TR_GPR, cg);
   }

void OMR::Power::MemoryReference::decNodeReferenceCounts(TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      if (_baseNode != NULL)
         {
         cg->decReferenceCount(_baseNode);
         }
      else
         cg->stopUsingRegister(_baseRegister);
      }

   if (_indexRegister != NULL)
      {
      if (_indexNode != NULL)
         cg->decReferenceCount(_indexNode);
      else
         cg->stopUsingRegister(_indexRegister);
      }

   if (_modBase != NULL)
      cg->stopUsingRegister(_modBase);
   }

void OMR::Power::MemoryReference::bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      instr->useRegister(_baseRegister);
      }
   if (_indexRegister != NULL)
      {
      instr->useRegister(_indexRegister);
      }
   if (_modBase != NULL)
      {
      instr->useRegister(_modBase);
      _conditions->bookKeepingRegisterUses(instr, cg);
      }
   }

static bool isLoadConstAndShift(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   if (TR::Compiler->target.is64Bit())
      {
      if (subTree->getOpCodeValue() == TR::lshl)
         {
         // lshl
         //    lconst
         //    iconst
         if (subTree->getFirstChild()->getOpCode().isLoadConst() &&
             subTree->getSecondChild()->getOpCode().isLoadConst())
            return true;

         // lshl
         //    i2l
         //       iconst
         //    iconst
         else if (subTree->getFirstChild()->getOpCodeValue() == TR::i2l &&
                  subTree->getFirstChild()->getFirstChild()->getOpCode().isLoadConst() &&
                  subTree->getSecondChild()->getOpCode().isLoadConst())
            return true;
         else
            return false;
         }
      else
         return false;
      }
   else // 32-bit
      {
      if (subTree->getOpCodeValue() == TR::ishl &&
          subTree->getFirstChild()->getOpCode().isLoadConst() &&
          subTree->getSecondChild()->getOpCode().isLoadConst())
         return true;
      else
         return false;
      }
   }


void OMR::Power::MemoryReference::populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   if (cg->comp()->useCompressedPointers())
      {
      if (subTree->getOpCodeValue() == TR::l2a && subTree->getReferenceCount() == 1 && subTree->getRegister() == NULL)
         {
         cg->decReferenceCount(subTree);
         subTree = subTree->getFirstChild();
         }
      }

   // Skip sign extension if the offset is known to be >= 0; this is only safe to do if the child is the result of a load, otherwise the upper 32 bits may be undefined
   if (subTree->getOpCodeValue() == TR::i2l && subTree->isNonNegative() && subTree->getFirstChild()->getOpCode().isLoadVar() && subTree->getReferenceCount() == 1 && subTree->getRegister() == NULL)
      {
      if (performTransformation(cg->comp(), "O^O PPC Evaluator: Skip unecessary sign extension on index [%p].\n", subTree))
         {
         static bool dontSkipIndexSignExt = feGetEnv("TR_dontSkipIndexSignExt") != NULL;
         if (!dontSkipIndexSignExt)
            {
            cg->decReferenceCount(subTree);
            subTree = subTree->getFirstChild();
            }
         }
      }

   if (subTree->getReferenceCount()>1 || subTree->getRegister()!=NULL)
      {
      if (_baseRegister != NULL)
         {
         self()->consolidateRegisters(cg->evaluate(subTree), subTree, false, cg);
         }
      else
         {
         _baseRegister  = cg->evaluate(subTree);
         _baseNode      = subTree;
         }
      }
   else
      {
      if (subTree->getOpCode().isArrayRef() ||
          (subTree->getOpCodeValue() == TR::iadd || subTree->getOpCodeValue() == TR::ladd))
         {
         TR::Node     *addressChild    = subTree->getFirstChild();
         TR::Node     *integerChild    = subTree->getSecondChild();

         if (integerChild->getOpCode().isLoadConst())
            {
            self()->populateMemoryReference(addressChild, cg);
            if (TR::Compiler->target.is64Bit())
               {
               intptrj_t amount = (integerChild->getOpCodeValue() == TR::iconst) ?
                                   integerChild->getInt() :
                                   integerChild->getLongInt();
               ////addToOffset(subTree, amount, cg);
               self()->addToOffset(integerChild, amount, cg);
               }
            else
               self()->addToOffset(subTree, integerChild->getInt(), cg);
            cg->decReferenceCount(integerChild);
            }
         else if (integerChild->getEvaluationPriority(cg)>addressChild->getEvaluationPriority(cg) &&
                  !(subTree->getOpCode().isArrayRef() && TR::Compiler->target.cpu.id()==TR_PPCp6))
            {
            self()->populateMemoryReference(integerChild, cg);
            self()->populateMemoryReference(addressChild, cg);
            }
         else
            {
            self()->populateMemoryReference(addressChild, cg);
            self()->populateMemoryReference(integerChild, cg);
            }
         cg->decReferenceCount(subTree);
         }
      else if (isLoadConstAndShift(subTree, cg))
         {
         if (TR::Compiler->target.is64Bit())
            { // 64-bit
            intptrj_t amount = (subTree->getSecondChild()->getOpCodeValue() == TR::iconst) ?
                                subTree->getSecondChild()->getInt() :
                                subTree->getSecondChild()->getLongInt();
            // lshl
            //    lconst
            //    iconst(lconst)
            if (subTree->getFirstChild()->getOpCode().isLoadConst())
               self()->addToOffset(subTree, subTree->getFirstChild()->getLongInt() << amount, cg);
            else
            // lshl
            //    i2l
            //       iconst
            //    iconst(lconst)
               {
               self()->addToOffset(subTree, subTree->getFirstChild()->getFirstChild()->getInt() << amount,
                           cg);
               cg->decReferenceCount(subTree->getFirstChild()->getFirstChild());
               }
            }
         else
            self()->addToOffset(subTree, subTree->getFirstChild()->getInt() <<
                        subTree->getSecondChild()->getInt(), cg);
         cg->decReferenceCount(subTree->getFirstChild());
         cg->decReferenceCount(subTree->getSecondChild());
         }
      else if ((subTree->getOpCodeValue() == TR::loadaddr) && !cg->comp()->compileRelocatableCode())
         {
         TR::SymbolReference *ref = subTree->getSymbolReference();
         TR::Symbol *symbol = ref->getSymbol();
         bool       isStore = subTree->getOpCode().isStore();
         _symbolReference = ref;
         self()->checkRegisters(cg);

         if (symbol->isStatic())
            {
            self()->accessStaticItem(subTree, ref, isStore, cg);
            }

         if (symbol->isRegisterMappedSymbol())
            {
            if (_baseRegister != NULL)
               {
               TR::Register *tempReg;
               if (!symbol->isMethodMetaData())
                  { // must be either auto or parm or error.
                  tempReg = cg->getStackPointerRegister();
                  }
               else
                  {
                  tempReg = cg->getMethodMetaDataRegister();
                  }
               self()->consolidateRegisters(tempReg, NULL, false, cg);
               }
            else
               {
               if (!symbol->isMethodMetaData())
                  { // must be either auto or parm or error.
                  _baseRegister = cg->getStackPointerRegister();
                  }
               else
                  {
                  _baseRegister = cg->getMethodMetaDataRegister();
                  }
               _baseNode = NULL;
               }
            }
         if (TR::Compiler->target.is32Bit() || !symbol->isConstObjectRef())
            self()->addToOffset(subTree, subTree->getSymbolReference()->getOffset(), cg);
         // TODO: aliasing sets?
         cg->decReferenceCount(subTree); // need to decrement ref count because
                                              // nodes weren't set on memoryreference
         }
      else if (subTree->getOpCodeValue() == TR::aconst ||
               subTree->getOpCodeValue() == TR::iconst || // subTree->getOpCode().isLoadConst ?
               subTree->getOpCodeValue() == TR::lconst)
         {
         intptrj_t amount = (subTree->getOpCodeValue() == TR::iconst) ?
                             subTree->getInt() : subTree->getLongInt();
         self()->addToOffset(subTree, amount, cg);
         }
      else
         {
         if (_baseRegister != NULL)
            {
            self()->consolidateRegisters(cg->evaluate(subTree), subTree, cg->canClobberNodesRegister(subTree), cg);
            }
         else
            {
            _baseRegister  = cg->evaluate(subTree);
            _baseNode      = subTree;
            if (cg->canClobberNodesRegister(subTree)) self()->setBaseModifiable();
            }
         }
      }
   }


void OMR::Power::MemoryReference::consolidateRegisters(TR::Register *srcReg, TR::Node *srcTree, bool srcModifiable, TR::CodeGenerator *cg)
   {
   TR::Register *tempTargetRegister;
   TR::Compilation *comp = cg->comp();

       TR::Node *tempNode = (srcTree==NULL)?comp->getAppendInstruction()->getNode():srcTree;

   if (self()->getUnresolvedSnippet() != NULL)
      {
      if (srcReg == NULL)
         return;
      if (_indexRegister != NULL)
         {
         if (self()->isIndexModifiable())
            tempTargetRegister = _indexRegister;
         else if ((srcReg && (srcReg->containsCollectedReference() || srcReg->containsInternalPointer())) ||
                  (_indexRegister && (_indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())))
           {
           if (srcTree!=NULL && srcTree->isInternalPointer() &&
               srcTree->getPinningArrayPointer())
              {
              tempTargetRegister = cg->allocateRegister();
              tempTargetRegister->setContainsInternalPointer();
              tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
              }
           else
              tempTargetRegister = cg->allocateCollectedReferenceRegister();
           }
        else
           {
           tempTargetRegister = cg->allocateRegister();
           }

         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, tempNode, tempTargetRegister, _indexRegister, srcReg);
         if (_indexRegister != tempTargetRegister)
            {
            if (_indexNode != NULL)
               cg->decReferenceCount(_indexNode);
            else
               cg->stopUsingRegister(_indexRegister);
            _indexNode = NULL;
            }
         if (srcTree != NULL)
            cg->decReferenceCount(srcTree);
         else
            cg->stopUsingRegister(srcReg);
         _indexRegister = tempTargetRegister;
         self()->setIndexModifiable();
         }
      else
         {
         _indexRegister = srcReg;
         _indexNode = srcTree;
         if (srcModifiable)
            self()->setIndexModifiable();
         else
            self()->clearIndexModifiable();
         }
      }
   else
      {
      if (_indexRegister != NULL)
         {
         if (self()->isBaseModifiable())
            tempTargetRegister = _baseRegister;
         else if ((_baseRegister && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())) ||
                  (_indexRegister && (_indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())))
            {
            if (srcTree!=NULL && srcTree->isInternalPointer() &&
                srcTree->getPinningArrayPointer())
               {
               tempTargetRegister = cg->allocateRegister();
               tempTargetRegister->setContainsInternalPointer();
               tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
               }
            else
               tempTargetRegister = cg->allocateCollectedReferenceRegister();
            }
         else
            {
            tempTargetRegister = cg->allocateRegister();
            }

         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, tempNode, tempTargetRegister, _baseRegister, _indexRegister);
         if (_baseRegister != tempTargetRegister)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = NULL;
            }
         else
            {
            if (_indexNode != NULL)
               cg->decReferenceCount(_indexNode);
            else
               cg->stopUsingRegister(_indexRegister);
            }
         _baseRegister  = tempTargetRegister;
         self()->setBaseModifiable();
         }
      else if (srcReg!=NULL && (self()->getOffset(*comp)!=0 || self()->hasDelayedOffset()))
         {
         if (self()->isBaseModifiable())
            tempTargetRegister = _baseRegister;
         else if (srcModifiable)
            tempTargetRegister = srcReg;
         else if ((srcReg && (srcReg->containsCollectedReference() || srcReg->containsInternalPointer())) ||
                  (_baseRegister && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())))
            {
            if (srcTree!=NULL && srcTree->isInternalPointer() &&
               srcTree->getPinningArrayPointer())
               {
               tempTargetRegister = cg->allocateRegister();
               tempTargetRegister->setContainsInternalPointer();
               tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
               }
            else
               tempTargetRegister = cg->allocateCollectedReferenceRegister();
            }
         else
            {
            tempTargetRegister = cg->allocateRegister();
            }

         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, tempNode, tempTargetRegister, _baseRegister, srcReg);
         if (_baseRegister != tempTargetRegister)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = NULL;
            }
         if (srcReg == tempTargetRegister)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = srcTree;
            }
         else
            {
            if (srcTree != NULL)
               cg->decReferenceCount(srcTree);
            else
               cg->stopUsingRegister(srcReg);
            }
         _baseRegister  = tempTargetRegister;
         self()->setBaseModifiable();
         srcReg=NULL;
         srcTree=NULL;
         srcModifiable=false;
         }
      _indexRegister = srcReg;
      _indexNode = srcTree;
      if (srcModifiable)
         self()->setIndexModifiable();
      else
         self()->clearIndexModifiable();
      }
   }



void OMR::Power::MemoryReference::assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();
   TR::RealRegister *assignedBaseRegister;
   TR::RealRegister *assignedIndexRegister;

   if (_modBase != NULL)
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }

      _conditions->assignPostConditionRegisters(currentInstruction, _modBase->getKind(), cg);
      _conditions->assignPreConditionRegisters(currentInstruction->getPrev(), _modBase->getKind(), cg);

      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      }

   if (_baseRegister != NULL)
      {
      assignedBaseRegister = _baseRegister->getAssignedRealRegister();
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }
      if (_modBase != NULL)
         {
         _modBase->block();
         }

      if ((assignedBaseRegister != NULL) &&
          (toRealRegister(assignedBaseRegister)->getRegisterNumber() ==
           TR::RealRegister::gr0))
         {
         TR::RealRegister     *alternativeRegister;
         if ((alternativeRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, true, false, _baseRegister)) == NULL)
            {
            alternativeRegister = machine->freeBestRegister(currentInstruction, _baseRegister, NULL, true);
            }
         machine->coerceRegisterAssignment(currentInstruction, _baseRegister, toRealRegister(alternativeRegister)->getRegisterNumber());
         assignedBaseRegister = alternativeRegister;
         }
      else
         {
         if (assignedBaseRegister == NULL)
            {
            assignedBaseRegister = machine->assignOneRegister(currentInstruction, _baseRegister, true);
            }
         }

      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      if (_modBase != NULL)
         {
         _modBase->unblock();
         }
      }

   if (_indexRegister != NULL)
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }
      if (_modBase != NULL)
         {
         _modBase->block();
         }

      assignedIndexRegister = _indexRegister->getAssignedRealRegister();
      if (assignedIndexRegister == NULL)
         {
         assignedIndexRegister = machine->assignOneRegister(currentInstruction, _indexRegister, false);
         }

      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      if (_modBase != NULL)
         {
         _modBase->unblock();
         }

      }

   //////////////////////////////////////
   // _modBase has to be freed outside.//
   //////////////////////////////////////

   if (_modBase != NULL)
      {
      _modBase = _modBase->getAssignedRealRegister();
      }

   if (_baseRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(_baseRegister);
      _baseRegister = assignedBaseRegister;
      }

   if (_indexRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(_indexRegister);
      _indexRegister = assignedIndexRegister;
      }

   if (self()->getUnresolvedSnippet() != NULL)
      {
      currentInstruction->PPCNeedsGCMap(0xFFFFFFFF);
      }
   }

void OMR::Power::MemoryReference::mapOpCode(TR::Instruction *currentInstruction)
   {
   TR::InstOpCode::Mnemonic op = currentInstruction->getOpCodeValue();
   TR::Register  *index = self()->getIndexRegister();

   if (index == NULL && !self()->isUsingDelayedIndexedForm())
      return;

   switch (op)
      {
      case TR::InstOpCode::lbz:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lbzx);
         break;
      case TR::InstOpCode::ld:
         currentInstruction->setOpCodeValue(TR::InstOpCode::ldx);
         break;
      case TR::InstOpCode::lfd:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfdx);
         break;
      case TR::InstOpCode::lfdu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfdux);
         break;
      case TR::InstOpCode::lfs:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfsx);
         break;
      case TR::InstOpCode::lfsu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfsux);
         break;
      case TR::InstOpCode::lha:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhax);
         break;
      case TR::InstOpCode::lhau:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhaux);
         break;
      case TR::InstOpCode::lhz:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhzx);
         break;
      case TR::InstOpCode::lhzu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhzux);
         break;
      case TR::InstOpCode::lwa:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lwax);
         break;
      case TR::InstOpCode::lwz:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lwzx);
         break;
      case TR::InstOpCode::lwzu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lwzux);
         break;
      case TR::InstOpCode::stb:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stbx);
         break;
      case TR::InstOpCode::stbu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stbux);
         break;
      case TR::InstOpCode::std:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stdx);
         break;
      case TR::InstOpCode::stdu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stdux);
         break;
      case TR::InstOpCode::stfd:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfdx);
         break;
      case TR::InstOpCode::stfdu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfdux);
         break;
      case TR::InstOpCode::stfs:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfsx);
         break;
      case TR::InstOpCode::stfsu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfsux);
         break;
      case TR::InstOpCode::sth:
         currentInstruction->setOpCodeValue(TR::InstOpCode::sthx);
         break;
      case TR::InstOpCode::sthu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::sthux);
         break;
      case TR::InstOpCode::stw:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stwx);
         break;
      case TR::InstOpCode::stwu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stwux);
         break;
      default:
         break;
      }
   }


uint8_t *OMR::Power::MemoryReference::generateBinaryEncoding(TR::Instruction *currentInstruction, uint8_t *cursor, TR::CodeGenerator *cg)
   {
   uint32_t             *wcursor = (uint32_t *)cursor;
   TR::RealRegister   *base, *index;
   TR::Compilation *comp = cg->comp();

   if (self()->getStaticRelocation() != NULL)
      self()->getStaticRelocation()->setSource2Instruction(currentInstruction);

   base = (self()->getBaseRegister()==NULL)?cg->machine()->getPPCRealRegister(TR::RealRegister::gr0):toRealRegister(self()->getBaseRegister());
   index=(self()->getIndexRegister()==NULL)?NULL:toRealRegister(self()->getIndexRegister());

   if (TR::Compiler->target.is64Bit() && self()->isTOCAccess())
      {
      uint32_t preserve = *wcursor;
      TR::RealRegister *target = toRealRegister(currentInstruction->getMemoryDataRegister());
      int32_t  displacement = self()->getTOCOffset();
      int32_t  lower = LO_VALUE(displacement) & 0x0000ffff;
      int32_t  upper = cg->hiValue(displacement) & 0x0000ffff;

      if (self()->getUnresolvedSnippet() != NULL)
         {
#ifdef J9_PROJECT_SPECIFIC
         getUnresolvedSnippet()->setAddressOfDataReference(cursor);
         getUnresolvedSnippet()->setMemoryReference(self());
         getUnresolvedSnippet()->setDataRegister(target);
         cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, getUnresolvedSnippet()->getSnippetLabel()));
         *wcursor = 0x48000000;                        // b SnippetLabel;
         wcursor++;
         cursor += PPC_INSTRUCTION_LENGTH;

         if (displacement != PTOC_FULL_INDEX)
            {
            if (displacement<LOWER_IMMED || displacement>UPPER_IMMED)
               {
               *wcursor = preserve | lower;
               toRealRegister(getModBase())->setRegisterFieldRA(wcursor);      // mb is the modified base
               cursor += PPC_INSTRUCTION_LENGTH;
               }
            }
         else
            {
            *wcursor = 0x60000000;                    // ori target, target, bits16-31
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x780007c6;                    // rldicr target, target, 32, 31
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x64000000;                    // oris target, target, bits32-47
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x60000000;                    // ori target, target, bits48-63
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;
            }
#endif
         }
      else
         {
         TR::Symbol *symbol = self()->getSymbolReference()->getSymbol();
         uint64_t addr = symbol->isStatic()?(uint64_t)symbol->getStaticSymbol()->getStaticAddress():
                                            (uint64_t)symbol->getMethodSymbol()->getMethodAddress();
         if (displacement != PTOC_FULL_INDEX)
            {
            if (!self()->getSymbolReference()->isUnresolved() && TR_PPCTableOfConstants::getTOCSlot(displacement) == 0)
               {
               TR_PPCTableOfConstants::setTOCSlot(displacement, addr);
               }

            if (displacement<LOWER_IMMED || displacement>UPPER_IMMED)
               {
               *wcursor = 0x3c000000;
               target->setRegisterFieldRT(wcursor);
               base->setRegisterFieldRA(wcursor);
               *wcursor |= upper;                         // addis rT, rB, upper
               wcursor++;
               cursor += PPC_INSTRUCTION_LENGTH;
               base = target;                             // rT is the modified base
               }

            *wcursor = preserve | lower;
            base->setRegisterFieldRA(wcursor);            // Original with potential new base
            cursor += PPC_INSTRUCTION_LENGTH;
            }
         else
            {
            *wcursor = 0x3c000000;                        // lis target, bits0-15
            target->setRegisterFieldRT(wcursor);
            *wcursor |= (addr>>48) & 0x0000ffff;
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x60000000;                        // ori target, target, bits16-31
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            *wcursor |= (addr>>32) & 0x0000ffff;
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x780007c6;                        // rldicr target, target, 32, 31
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x64000000;                        // oris target, target, bits32-47
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            *wcursor |= (addr>>16) & 0x0000ffff;
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;

            *wcursor = 0x60000000;                        // ori target, target, bits48-63
            target->setRegisterFieldRS(wcursor);
            target->setRegisterFieldRA(wcursor);
            *wcursor |= addr & 0x0000ffff;
            wcursor++;
            cursor += PPC_INSTRUCTION_LENGTH;
            }
         }

      return(cursor);
      }

   if (self()->getUnresolvedSnippet() != NULL)
      {
#ifdef J9_PROJECT_SPECIFIC
      uint32_t preserve = *wcursor;
      TR::RealRegister *mBase = toRealRegister(getModBase());

      getUnresolvedSnippet()->setAddressOfDataReference(cursor);
      getUnresolvedSnippet()->setMemoryReference(self());
      cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, getUnresolvedSnippet()->getSnippetLabel()));
      *wcursor = 0x48000000;                        // b SnippetLabel;
      wcursor++;
      cursor += PPC_INSTRUCTION_LENGTH;

      if (index != NULL)
         {
         *wcursor = 0x38000000;
         mBase->setRegisterFieldRT(wcursor);
         mBase->setRegisterFieldRA(wcursor);
         wcursor++;
         cursor += PPC_INSTRUCTION_LENGTH;              // addi Rmb, Rmb, 0
                                                        // backpatch fills in SI field

         *wcursor = preserve;
         mBase->setRegisterFieldRA(wcursor);
         index->setRegisterFieldRB(wcursor);        // Original Instruction
         }
      else if (isUsingDelayedIndexedForm())
         {
         *wcursor = 0x38000000;
         mBase->setRegisterFieldRT(wcursor);
         mBase->setRegisterFieldRA(wcursor);
         wcursor++;
         cursor += PPC_INSTRUCTION_LENGTH;              // addi Rmb, Rmb, 0
                                                        // backpatch fills in SI field

         *wcursor = preserve;
         // just use 0 as RA, mBase as RB
         mBase->setRegisterFieldRB(wcursor);        // Original Instruction
         }
      else
         {
         *wcursor = preserve;
         mBase->setRegisterFieldRA(wcursor);
         *wcursor &= 0xffff0000;                    // Original with disp clear
         }

      return(cursor + PPC_INSTRUCTION_LENGTH);
#endif
      }

   if (index != NULL)
      {
      if (self()->isUsingDelayedIndexedForm())
         {
         int32_t  displacement = self()->getOffset(*comp);
         if (displacement<LOWER_IMMED || displacement>UPPER_IMMED)
            {
            uint32_t preserve = *wcursor;

            *wcursor = 0x3c000000 | ((displacement >> 16) & 0x0000ffff);
            index->setRegisterFieldRT(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // lis Ri,upper

            *wcursor = 0x60000000 | (displacement & 0x0000ffff);
            index->setRegisterFieldRA(wcursor);
            index->setRegisterFieldRS(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // ori Ri,Ri,lower

            *wcursor = preserve;
            base->setRegisterFieldRA(wcursor);
            index->setRegisterFieldRB(wcursor);
            }
         else if (displacement != 0)
            {
            uint32_t preserve = *wcursor;

            *wcursor = 0x38000000 | (displacement & 0x0000ffff);
            index->setRegisterFieldRT(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // li Ri,disp

            *wcursor = preserve;
            base->setRegisterFieldRA(wcursor);
            index->setRegisterFieldRB(wcursor);
            }
         else
            {
            // we don't need to use the index reg
            // just use 0 as RA, base as RB
            base->setRegisterFieldRB(wcursor);
            }
         }
      else
         {
         base->setRegisterFieldRA(wcursor);
         index->setRegisterFieldRB(wcursor);
         }
      }
   else
      {
      int32_t  displacement = self()->getOffset(*comp);
      uint32_t preserve = *wcursor;
      int32_t  lower = LO_VALUE(displacement) & 0x0000ffff;
      int32_t  upper = cg->hiValue(displacement) & 0x0000ffff;

      if (self()->isUsingDelayedIndexedForm())
         {
         // must be isBaseModifiable()
         if (displacement<LOWER_IMMED || displacement>UPPER_IMMED)
            {
            *wcursor = 0x3c000000 | upper;
            base->setRegisterFieldRT(wcursor);
            base->setRegisterFieldRA(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // addis Rb,Rb,upper

            *wcursor = 0x38000000 | lower;
            base->setRegisterFieldRT(wcursor);
            base->setRegisterFieldRA(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // addi Rb,Rb,lower
            }
         else if (displacement != 0)
            {
            *wcursor = 0x38000000 | (displacement & 0x0000ffff);
            index->setRegisterFieldRT(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // addi Rb,Rb,disp
            }

         *wcursor = preserve;
         // just use 0 as RA, base as RB
         base->setRegisterFieldRB(wcursor);
         }
      else if (displacement<LOWER_IMMED || displacement>UPPER_IMMED ||
               (self()->offsetRequireWordAlignment() && (displacement & 3 != 0)))
         {
         if (self()->isBaseModifiable())
            {
            *wcursor = 0x3c000000 | upper;
            base->setRegisterFieldRT(wcursor);
            base->setRegisterFieldRA(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // addis Rb,Rb,upper

            *wcursor = preserve | lower;
            base->setRegisterFieldRA(wcursor);          // Original instruction
            }
         else
            {
            TR::RealRegister   *stackPtr = cg->getStackPointerRegister();
            TR::RealRegister *rX = cg->machine()->getPPCRealRegister(choose_rX(currentInstruction, base));
            *wcursor = (TR::Compiler->target.is64Bit())?0xf800fff8:0x9000fffc;
            rX->setRegisterFieldRS(wcursor);
            stackPtr->setRegisterFieldRA(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                 // stw [sp,-4], rX | std [sp, -8], rX

            *wcursor = 0x3c000000 | upper;
            rX->setRegisterFieldRT(wcursor);
            base->setRegisterFieldRA(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // addis rX, Rb, upper

            *wcursor = preserve | lower;
            rX->setRegisterFieldRA(wcursor);
            cursor += PPC_INSTRUCTION_LENGTH;
            wcursor++;                                  // ld Rt, [rX, lower]
                                                        // st Rs, [rX, lower]

            *wcursor = (TR::Compiler->target.is64Bit())?0xe800fff8:0x8000fffc;
            rX->setRegisterFieldRT(wcursor);
            stackPtr->setRegisterFieldRA(wcursor);
                                                       // lwz rX, [sp, -4] | ld rX, [sp, -8]
            }
         }
      else
         {
         base->setRegisterFieldRA(wcursor);
         *wcursor |= lower;
         }
      }
   cursor += PPC_INSTRUCTION_LENGTH;
   return(cursor);
   }

static TR::RealRegister::RegNum choose_rX(TR::Instruction *currentInstruction, TR::RealRegister *base)
   {
   TR::RealRegister::RegNum TSnum = toRealRegister(currentInstruction->getMemoryDataRegister())->getRegisterNumber();
   TR::RealRegister::RegNum basenum = base->getRegisterNumber();
   int32_t  exclude = ((TSnum == TR::RealRegister::gr10)?1:0)|
                      ((TSnum == TR::RealRegister::gr9)?2:0)|
                      ((basenum == TR::RealRegister::gr10)?1:0)|
                      ((basenum == TR::RealRegister::gr9)?2:0);
   switch (exclude)
      {
      case 0:
      case 2:
         return(TR::RealRegister::gr10);
      case 1:
         return(TR::RealRegister::gr9);
      case 3:
         return(TR::RealRegister::gr8);
      }
   return(TR::RealRegister::gr10);
   }

uint32_t OMR::Power::MemoryReference::estimateBinaryLength(TR::CodeGenerator& codeGen)
   {
   TR::Compilation *comp = TR::comp();
   if (self()->isTOCAccess())
      {
      int32_t tocOffset = self()->getTOCOffset();
      if (tocOffset == PTOC_FULL_INDEX)
         return(5*PPC_INSTRUCTION_LENGTH);
      if (tocOffset<LOWER_IMMED || tocOffset>UPPER_IMMED)
         return(2*PPC_INSTRUCTION_LENGTH);
      return(PPC_INSTRUCTION_LENGTH);
      }

   if (self()->getUnresolvedSnippet() != NULL)
      {
      if (_indexRegister != NULL || self()->isUsingDelayedIndexedForm())
         {
         return(3*PPC_INSTRUCTION_LENGTH);
         }
      else
         {
         return(2*PPC_INSTRUCTION_LENGTH);
         }
      }

   if (_indexRegister != NULL)
      {
      if (self()->isUsingDelayedIndexedForm())
         {
         if (self()->getOffset(*comp)<LOWER_IMMED || self()->getOffset(*comp)>UPPER_IMMED)
            {
            return(3*PPC_INSTRUCTION_LENGTH);
            }
         else if (self()->getOffset(*comp) != 0)
            {
            return(2*PPC_INSTRUCTION_LENGTH);
            }
         }
      return(PPC_INSTRUCTION_LENGTH);
      }
   else
      {
      if (self()->isUsingDelayedIndexedForm())
         {
         // must be isBaseModifiable()
         if (self()->getOffset(*comp)<LOWER_IMMED || self()->getOffset(*comp)>UPPER_IMMED)
            {
            return(3*PPC_INSTRUCTION_LENGTH);
            }
         else if (self()->getOffset(*comp) != 0)
            {
            return(2*PPC_INSTRUCTION_LENGTH);
            }
         }
      if (self()->getOffset(*comp)<LOWER_IMMED || self()->getOffset(*comp)>UPPER_IMMED)
         {
         if (self()->isBaseModifiable())
            {
            return(2*PPC_INSTRUCTION_LENGTH);
            }
         else
            {
            return(4*PPC_INSTRUCTION_LENGTH);
            }
         }
      else
         {
         return(PPC_INSTRUCTION_LENGTH);
         }
      }
   }

extern "C" void voom(){}
void OMR::Power::MemoryReference::accessStaticItem(TR::Node *node, TR::SymbolReference *ref, bool isStore, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Symbol                       *symbol;
   symbol = ref->getSymbol();
   TR_ASSERT(symbol->isStatic() || symbol->isMethod(), "accessStaticItem:problem with new symbol hierarchy");

   TR_ASSERT(_baseRegister==NULL, "32Bit JIT has relocation problem for AOT");

   bool isStatic = symbol->isStatic() && !ref->isUnresolved();
   bool isStaticField = isStatic && (ref->getCPIndex() > 0) && !symbol->isClassObject();
   bool isClass = isStatic && symbol->isClassObject();
   bool isPicSite = isClass;
   if (isPicSite && !cg->comp()->compileRelocatableCode()
       && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)symbol->getStaticSymbol()->getStaticAddress(), node))
      {
      TR_ASSERT(!comp->getOption(TR_AOT), "HCR: AOT is currently no supported");
      TR::Register *reg = _baseRegister = cg->allocateRegister();
      intptrj_t address = (intptrj_t)symbol->getStaticSymbol()->getStaticAddress();
      loadAddressConstantInSnippet(cg, node ? node : cg->getCurrentEvaluationTreeTop()->getNode(), address, reg, NULL, isStore?TR::InstOpCode::Op_st :TR::InstOpCode::Op_load, false, NULL);
      return;
      }

   if (TR::Compiler->target.is64Bit())
      {
      TR::Node *topNode = cg->getCurrentEvaluationTreeTop()->getNode();
      TR::UnresolvedDataSnippet *snippet = NULL;
      int32_t  tocIndex = symbol->getStaticSymbol()->getTOCIndex();

      if (cg->comp()->compileRelocatableCode())
         {
         /* don't want to trash node prematurely by the code for handling guarded counting recompilations symbols */
         TR::Node *GCRnode = node;
         if (!GCRnode)
            GCRnode = cg->getCurrentEvaluationTreeTop()->getNode();

         if (symbol->isCountForRecompile())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, TR_CountForRecompile, reg, NULL, false, TR_GlobalValue);
            return;
            }
         else if (symbol->isRecompilationCounter())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_BodyInfoAddressLoad);
            return;
            }
         else if (symbol->isCompiledMethod())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_RamMethodSequence);
            return;
            }
         else if (symbol->isStartPC())
            {
            // use inSnippet, as the relocation mechanism is already set up there
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstantInSnippet(cg, GCRnode, 0, reg, NULL, TR::InstOpCode::addi, false, NULL);
            return;
            }
         else if (isStaticField && !ref->isUnresolved())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_DataAddress);
            return;
            }
         }

      // TODO: find a better default uninitialized value --- 0x80000000
      //       Deciding particular static references through TOC or not
      // startPC symbols should not be placed in the TOC as their value is not known at this time
      // and they are different from method to method
      if (tocIndex == 0 && !symbol->isStartPC())
         {
         tocIndex = TR_PPCTableOfConstants::lookUp(ref, cg);
         symbol->getStaticSymbol()->setTOCIndex(tocIndex);
         }

      // We need a snippet for unresolved or AOT if:
      //  1. the load hasn't been resolved yet
      //  (otherwise optimizer will remove the ResolveCHK and we can be sure pTOC will contain the resolved address),
      //  2. we don't have a PTOC slot, we must always take the slow path
      if ((ref->isUnresolved() || cg->comp()->compileRelocatableCode()) &&
          (topNode->getOpCodeValue() == TR::ResolveCHK || tocIndex == PTOC_FULL_INDEX))
         {
         snippet = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, ref, isStore, false);
         cg->addSnippet(snippet);
         }

      // TODO: Improve the code sequence for cases when we know pTOC is full.
      TR::MemoryReference *tocRef = new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), 0, sizeof(uintptrj_t), cg);
      tocRef->setSymbol(symbol, cg);
      tocRef->getSymbolReference()->copyFlags(ref);
      tocRef->setUsingStaticTOC();
      if (snippet != NULL)
         {
         tocRef->setUnresolvedSnippet(snippet);
         tocRef->adjustForResolution(cg);
         }

      TR::Register *addrReg = cg->allocateRegister();
      TR::InstOpCode::Mnemonic loadOp = TR::InstOpCode::ld;

      generateTrg1MemInstruction(cg, loadOp, node==NULL?topNode:node, addrReg, tocRef);
      if (snippet != NULL)
         cg->stopUsingRegister(tocRef->getModBase());
      _baseRegister = addrReg;
      self()->setBaseModifiable();
      }
   else
      {
      if (ref->isUnresolved()  || comp->compileRelocatableCode())
         {
         /* don't want to trash node prematurely by the code for handling guarded counting recompilations symbols */
         TR::Node *GCRnode = node;
         if (!GCRnode)
            GCRnode = cg->getCurrentEvaluationTreeTop()->getNode();

         if (symbol->isCountForRecompile())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, TR_CountForRecompile, reg, NULL, false, TR_GlobalValue);
            }
         else if (symbol->isRecompilationCounter())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, 0, reg, NULL, false, TR_BodyInfoAddressLoad);
            }
         else if (symbol->isCompiledMethod())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, 0, reg, NULL, false, TR_RamMethodSequence);
            }
         else if (symbol->isStartPC())
            {
            // use inSnippet, as the relocation mechanism is already set up there
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstantInSnippet(cg, GCRnode, 0, reg, NULL, TR::InstOpCode::addi, false, NULL);
            }
         else if (isStaticField && !ref->isUnresolved())
            {
            TR::Register *reg = _baseRegister = cg->allocateRegister();
            loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_DataAddress);
            }
         else
            {
            self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, ref, isStore, false));
            cg->addSnippet(self()->getUnresolvedSnippet());
            }
         }
      else
         {
         TR::Instruction                 *rel1, *rel2;
         uint8_t                        *relocationTarget;
         TR::PPCPairedRelocation         *staticRelocation;
         TR::Register                    *reg = _baseRegister = cg->allocateRegister();
         TR_ExternalRelocationTargetKind relocationKind;
         intptrj_t                        addr;
         bool                            specialCase;

         // Unless we get a new type of relocation, we have to totally load the address for
         // AOT purpose for long data type since its low half can reuse the memory reference
         // without appropriate relocation to fix it.
         specialCase = symbol->isStatic() &&
                       (symbol->getType().isInt64()) &&
                       comp->getOption(TR_AOT);

         addr = symbol->isStatic() ? (intptrj_t)symbol->getStaticSymbol()->getStaticAddress() :
                                     (intptrj_t)symbol->getMethodSymbol()->getMethodAddress();

         if (!node) node = cg->getCurrentEvaluationTreeTop()->getNode();

         if (symbol->isStartPC())
            {
            loadAddressConstantInSnippet(cg, node, 0, reg, NULL, TR::InstOpCode::addi, false, NULL);
            return;
            }

         self()->setBaseModifiable();
         rel1 = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, reg, cg->hiValue(addr)&0x0000ffff);
         if (specialCase)
            {
            rel2 = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, reg, reg, LO_VALUE(addr));
            }
         else
            {
            rel2 = NULL;
            self()->addToOffset(node, LO_VALUE(addr), cg);
            }

         if (symbol->isConst())
            {
            relocationTarget = (uint8_t *)ref->getOwningMethod(comp)->constantPool();
            relocationKind   = TR_ConstantPoolOrderedPair;
            }
         else if (symbol->isClassObject())
            {
            relocationKind   = TR_ClassAddress;
            }
         else if (symbol->isMethod())
            {
            relocationTarget = (uint8_t *)ref;
            relocationKind   = TR_MethodObject;
            }
         else
            {
            relocationTarget = (uint8_t *)ref;
            relocationKind   = TR_DataAddress;
            }

         if (comp->getOption(TR_AOT))
            {
             TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
            recordInfo->data1 = (uintptr_t)relocationTarget;
            recordInfo->data2 = node ? (uintptr_t)node->getInlinedSiteIndex() : (uintptr_t)-1;
            recordInfo->data3 = orderedPairSequence1;
            staticRelocation = new (cg->trHeapMemory()) TR::PPCPairedRelocation(rel1, rel2, (uint8_t *)recordInfo, relocationKind, node);
            cg->getAheadOfTimeCompile()->getRelocationList().push_front(staticRelocation);

            if (!specialCase)
               self()->setStaticRelocation(staticRelocation);
            }
         }
      }
   }


void
OMR::Power::MemoryReference::setOffsetRequiresWordAlignment(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Instruction *cursor)
   {
   _flag |=  TR_PPCMemoryReferenceControl_OffsetRequiresWordAlignment;
   if (self()->getOffset() & 0x3)
      {
      self()->forceIndexedForm(node, cg, cursor);
      }
   }

void
OMR::Power::MemoryReference::setSymbol(
      TR::Symbol *symbol,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(_symbolReference->getSymbol()==NULL || _symbolReference->getSymbol()==symbol, "should not write to existing symbolReference");
   _symbolReference->setSymbol(symbol);

   if (_baseRegister!=NULL && _indexRegister!=NULL &&
       (self()->hasDelayedOffset() || self()->getOffset(*cg->comp())!=0))
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   }

void
OMR::Power::MemoryReference::checkRegisters(TR::CodeGenerator *cg)
   {
   if (_baseRegister!=NULL && _indexRegister!=NULL &&
       (self()->hasDelayedOffset() || self()->getOffset(*cg->comp())!=0))
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   }
