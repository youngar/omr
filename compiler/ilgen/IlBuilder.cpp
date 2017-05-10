/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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

#include "ilgen/IlBuilder.hpp"

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/FrontEnd.hpp"
#include "il/Block.hpp"
#include "il/SymbolReference.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"

#define OPT_DETAILS "O^O ILBLD: "

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

//#define REPLAY(x)            { x; }
#define REPLAY(x)            { }
#define ILB_REPLAY(...)	     { REPLAY({sprintf(_rpLine, ##__VA_ARGS__); (*_rpILCpp) << "\t" << _rpLine << std::endl;}) }
#define REPLAY_VALUE(v)      ((v)->getSymbol()->getAutoSymbol()->getName())
#define REPLAY_TYPE(t)       ((t)->getName())
#define REPLAY_BUILDER(b)    ((b)->getName())
#define REPLAY_BOOL(b)       ( b? "true" : "false")


// IlBuilder is a class designed to help build Testarossa IL quickly without
// a lot of knowledge of the intricacies of commoned references, symbols,
// symbol references, or blocks. You can add operations to an IlBuilder via
// a set of services, for example: Add, Sub, Load, Store. These services
// operate on TR::IlValues, which currently correspond to SymbolReferences.
// However, IlBuilders also incorporate a notion of "symbols" that are
// identified by strings (i.e. arbitrary symbolic names).
//
// So Load("a") returns a TR::IlValue. Load("b") returns a different
// TR::IlValue. The value of a+b can be computed by Add(Load("a"),Load("b")),
// and that value can be stored into c by Store("c", Add(Load("a"),Load("b"))).
//
// More complicated services exist to construct control flow by linking
// together sets of IlBuilder objects. A simple if-then construct, for
// example, can be built with the following code:
//    TR::IlValue *condition = NotEqualToZero(Load("a"));
//    TR::IlBuilder *then = NULL;
//    IfThen(&then, condition);
//    then->Return(then->ConstInt32(0));
//
//
// An IlBuilder is really a sequence of Blocks and other IlBuilders, but an
// IlBuilder is also a sequence of TreeTops connecting together a set of
// Blocks that are arranged in the CFG. Another way to think of an IlBuilder
// is as representing the code needed to execute a particular code path
// (which may itself have sub control flow handled by embedded IlBuilders).
// All IlBuilders exist within the single control flow graph, but the full
// control flow graph will not be connected together until all IlBuilder
// objects have had injectIL() called.


namespace OMR
{

IlBuilder::IlBuilder(TR::IlBuilder *source)
   : TR::IlInjector(source),
   _methodBuilder(source->_methodBuilder),
   _sequence(0),
   _sequenceAppender(0),
   _entryBlock(0),
   _exitBlock(0),
   _count(-1),
   _partOfSequence(false),
   _connectedTrees(false),
   _comesBack(true),
   _haveReplayName(false),
   _rpILCpp(0)
   {
   }

bool
IlBuilder::TraceEnabled_log(){
    bool traceEnabled = _comp->getOption(TR_TraceILGen);
    return traceEnabled;
}

void
IlBuilder::TraceIL_log(const char* s, ...){
    va_list argp;
    va_start (argp, s);
    traceMsgVarArgs(_comp, s, argp);
    va_end(argp);
}

void
IlBuilder::initSequence()
   {
   _sequence = new (_comp->trMemory()->trHeapMemory()) List<SequenceEntry>(_comp->trMemory());
   _sequenceAppender = new (_comp->trMemory()->trHeapMemory()) ListAppender<SequenceEntry>(_sequence);
   }

bool
IlBuilder::injectIL()
   {
   TraceIL("Inside injectIL()\n");
   TraceIL("original entry %p\n", cfg()->getStart());
   TraceIL("original exit %p\n", cfg()->getEnd());

   setupForBuildIL();

   bool rc = buildIL();
   TraceIL("buildIL() returned %d\n", rc);
   if (!rc)
      return false;

   rc = connectTrees();
   if (TraceEnabled)
      comp()->dumpMethodTrees("after connectTrees");
   cfg()->removeUnreachableBlocks();
   if (TraceEnabled)
      comp()->dumpMethodTrees("after removing unreachable blocks");
   return rc;
   }

void
IlBuilder::setupForBuildIL()
   {
   initSequence();
   appendBlock(NULL, false);
   _entryBlock = _currentBlock;
   _exitBlock = emptyBlock();
   }


char *
IlBuilder::getName()
   {
   if (!_haveReplayName)
      {
      sprintf(_replayName, "B_%p", this);
      _haveReplayName = true;
      }
   return _replayName;
   }


void
IlBuilder::print(const char *title, bool recurse)
   {
   if (!TraceEnabled)
      return;

   if (title != NULL)
      TraceIL("[ %p ] %s\n", this, title);
   TraceIL("[ %p ] _methodBuilder %p\n", this, _methodBuilder);
   TraceIL("[ %p ] _entryBlock %p\n", this, _entryBlock);
   TraceIL("[ %p ] _exitBlock %p\n", this, _exitBlock);
   TraceIL("[ %p ] _connectedBlocks %d\n", this, _connectedTrees);
   TraceIL("[ %p ] _comesBack %d\n", this, _comesBack);
   TraceIL("[ %p ] Sequence:\n", this);
   ListIterator<SequenceEntry> iter(_sequence);
   for (SequenceEntry *entry = iter.getFirst(); !iter.atEnd(); entry = iter.getNext())
      {
      if (entry->_isBlock)
         {
         printBlock(entry->_block);
         }
      else
         {
         if (recurse)
            {
            TraceIL("[ %p ] Inner builder %p", this, entry->_builder);
            entry->_builder->print(NULL, recurse);
            }
         else
            {
            TraceIL("[ %p ] Builder %p\n", this, entry->_builder);
            }
         }
      }
   }

void
IlBuilder::printBlock(TR::Block *block)
   {
   if (!TraceEnabled)
      return;

   TraceIL("[ %p ] Block %p\n", this, block);
   TR::TreeTop *tt = block->getEntry();
   while (tt != block->getExit())
      {
      comp()->getDebug()->print(comp()->getOutFile(), tt);
      tt = tt->getNextTreeTop();
      }
   comp()->getDebug()->print(comp()->getOutFile(), tt);
   }

TR::IlValue *
IlBuilder::lookupSymbol(const char *name)
   {
   TR_ASSERT(_methodBuilder, "cannot look up symbols in an IlBuilder that has no MethodBuilder");
   return _methodBuilder->lookupSymbol(name);
   }

void
IlBuilder::defineSymbol(const char *name, TR::IlValue *v)
   {
   TR_ASSERT(_methodBuilder, "cannot define symbols in an IlBuilder that has no MethodBuilder");
   _methodBuilder->defineSymbol(name, v);
   }

void
IlBuilder::defineValue(const char *name, TR::IlType *type)
   {
   TR::DataType dt = type->getPrimitiveType();
   TR::SymbolReference *newSymRef = symRefTab()->createTemporary(methodSymbol(), dt);
   newSymRef->getSymbol()->setNotCollected();
   defineSymbol(name, newSymRef);
   }

TR::IlValue *
IlBuilder::newValue(TR::DataType dt)
   {
   TR::SymbolReference *newSymRef = symRefTab()->createTemporary(methodSymbol(), dt);
   newSymRef->getSymbol()->setNotCollected();
   char *name = (char *) _comp->trMemory()->allocateHeapMemory(5 * sizeof(char));
   sprintf(name, "_T%-3d", newSymRef->getCPIndex());
   newSymRef->getSymbol()->getAutoSymbol()->setName(name);
   defineSymbol(name, newSymRef);
   return newSymRef;
   }

TR::IlValue *
IlBuilder::NewValue(TR::IlType *dt)
   {
   return newValue(dt->getPrimitiveType());
   }

TR::TreeTop *
IlBuilder::getFirstTree()
   {
   TR_ASSERT(_blocks, "_blocks not created yet");
   TR_ASSERT(_blocks[0], "_blocks[0] not created yet");
   return _blocks[0]->getEntry();
   }

TR::TreeTop *
IlBuilder::getLastTree()
   {
   TR_ASSERT(_blocks, "_blocks not created yet");
   TR_ASSERT(_blocks[_numBlocks], "_blocks[0] not created yet");
   return _blocks[_numBlocks]->getExit();
   }

uint32_t
IlBuilder::countBlocks()
   {
   // only count each block once
   if (_count > -1)
      return _count;

   TraceIL("[ %p ] TR::IlBuilder::countBlocks 0 at entry\n", this);

   _count = 0; // prevent recursive counting; will be updated to real value before returning

   uint32_t count=0;
   ListIterator<SequenceEntry> iter(_sequence);
   for (SequenceEntry *entry = iter.getFirst(); !iter.atEnd(); entry = iter.getNext())
      {
      if (entry->_isBlock)
         count++;
      else
         count += entry->_builder->countBlocks();
      }

   if (this != _methodBuilder)
      {
      // exit block isn't in the sequence except for method builders
      //            gets tacked in late by connectTrees
      count++;
      }

   TraceIL("[ %p ] TR::IlBuilder::countBlocks %d\n", this, count);

   _count = count;
   return count;
   }

void
IlBuilder::pullInBuilderTrees(TR::IlBuilder *builder,
                              uint32_t *currentBlock,
                              TR::TreeTop **firstTree,
                              TR::TreeTop **newLastTree)
   {
   TraceIL("\n[ %p ] Calling connectTrees on inner builder %p\n", this, builder);
   builder->connectTrees();
   TraceIL("\n[ %p ] Returned from connectTrees on %p\n", this, builder);

   TR::Block **innerBlocks = builder->blocks();
   uint32_t innerNumBlocks = builder->numBlocks();

   uint32_t copyBlock = *currentBlock;
   for (uint32_t i=0;i < innerNumBlocks;i++, copyBlock++)
      {
      if (TraceEnabled)
         {
         TraceIL("[ %p ] copying inner block B%d to B%d\n", this, i, copyBlock);
         printBlock(innerBlocks[i]);
         }
      _blocks[copyBlock] = innerBlocks[i];
      }

   *currentBlock += innerNumBlocks;
   *firstTree = innerBlocks[0]->getEntry();
   *newLastTree = innerBlocks[innerNumBlocks-1]->getExit();
   }

bool
IlBuilder::connectTrees()
   {
   // don't do this more than once per builder object
   if (_connectedTrees)
      return true;

   _connectedTrees = true;

   TraceIL("[ %p ] TR::IlBuilder::connectTrees():\n", this);

   // figure out how many blocks we need
   uint32_t numBlocks = countBlocks();
   TraceIL("[ %p ] Total %d blocks\n", this, numBlocks);

   allocateBlocks(numBlocks);
   TraceIL("[ %p ] Allocated _blocks %p\n", this, _blocks);

   // now add all the blocks from the sequence into the _blocks array
   // and connect the trees into a single sequence
   uint32_t currentBlock = 0;
   TR::Block **blocks = _blocks;
   TR::TreeTop *lastTree = NULL;

   TraceIL("[ %p ] Connecting trees one entry at a time:\n", this);
   ListIterator<SequenceEntry> iter(_sequence);
   for (SequenceEntry *entry = iter.getFirst(); !iter.atEnd(); entry = iter.getNext())
      {
      TraceIL("[ %p ] currentBlock = %d\n", this, currentBlock);
      TraceIL("[ %p ] lastTree = %p\n", this, lastTree);
      TR::TreeTop *firstTree = NULL;
      TR::TreeTop *newLastTree = NULL;
      if (entry->_isBlock)
         {
         if (TraceEnabled)
            {
            TraceIL("[ %p ] Block entry %p becomes block B%d\n", this, entry->_block, currentBlock);
            printBlock(entry->_block);
            }
         blocks[currentBlock++] = entry->_block;
         firstTree = entry->_block->getEntry();
         newLastTree = entry->_block->getExit();
         }
      else
         {
         TR::IlBuilder *builder = entry->_builder;
         pullInBuilderTrees(builder, &currentBlock, &firstTree, &newLastTree);
         }

      TraceIL("[ %p ] First tree is %p [ node %p ]\n", this, firstTree, firstTree->getNode());
      TraceIL("[ %p ] Last tree will be %p [ node %p ]\n", this, newLastTree, newLastTree->getNode());

      // connect the trees
      if (lastTree)
         {
         TraceIL("[ %p ] Connecting tree %p [ node %p ] to new tree %p [ node %p ]\n", this, lastTree, lastTree->getNode(), firstTree, firstTree->getNode());
         lastTree->join(firstTree);
         }

      lastTree = newLastTree;
      }

   if (_methodBuilder != this)
      {
      // non-method builders need to append the "EXIT" block trees
      // (method builders have EXIT as the special block 1)
      TraceIL("[ %p ] Connecting last tree %p [ node %p ] to exit block entry %p [ node %p ]\n", this, lastTree, lastTree->getNode(), _exitBlock->getEntry(), _exitBlock->getEntry()->getNode());
      lastTree->join(_exitBlock->getEntry());

      // also add exit block to blocks array and add an edge from last block to EXIT if the builder comes back (i.e. doesn't return or branch off somewhere)
      if (comesBack())
         cfg()->addEdge(blocks[currentBlock-1], getExit());

      blocks[currentBlock++] = _exitBlock;
      TraceIL("[ %p ] exit block %d is %p (%p -> %p)\n", this, _exitBlock->getNumber(), _exitBlock->getEntry(), _exitBlock->getExit());
      lastTree = _exitBlock->getExit();

      _numBlocks = currentBlock;
      }

   TraceIL("[ %p ] last tree %p [ node %p ]\n", this, lastTree, lastTree->getNode());
   lastTree->setNextTreeTop(NULL);
   TraceIL("[ %p ] blocks[%d] exit tree is %p\n", this, currentBlock-1, blocks[currentBlock-1]->getExit());

   return true;
   }

TR::Node *
IlBuilder::zero(TR::DataType dt)
   {
   switch (dt)
      {
      case TR::Int8 :  return TR::Node::bconst(0);
      case TR::Int16 : return TR::Node::sconst(0);
      default :        return TR::Node::create(TR::ILOpCode::constOpCode(dt), 0, 0);
      }
   }

TR::Node *
IlBuilder::zero(TR::IlType *dt)
   {
   return zero(dt->getPrimitiveType());
   }

TR::Node *
IlBuilder::zeroNodeForValue(TR::IlValue *v)
   {
   return zero(v->getSymbol()->getDataType());
   }

TR::IlValue *
IlBuilder::zeroForValue(TR::IlValue *v)
   {
   TR::IlValue *returnValue = newValue(v->getSymbol()->getDataType());
   storeNode(returnValue, zeroNodeForValue(v));
   return returnValue;
   }


TR::IlBuilder *
IlBuilder::OrphanBuilder()
   {
   TR::IlBuilder *orphan = new (comp()->trHeapMemory()) TR::IlBuilder(_methodBuilder, _types);
   orphan->initialize(_details, _methodSymbol, _fe, _symRefTab);
   orphan->setupForBuildIL();
   ILB_REPLAY("%s = %s->OrphanBuilder();", REPLAY_BUILDER(orphan), REPLAY_BUILDER(this));
   return orphan;
   }

TR::Block *
IlBuilder::emptyBlock()
   {
   TR::Block *empty = TR::Block::createEmptyBlock(NULL, _comp);
   cfg()->addNode(empty);
   return empty;
   }


TR::IlBuilder *
IlBuilder::createBuilderIfNeeded(TR::IlBuilder *builder)
   {
   if (builder == NULL)
      builder = OrphanBuilder();
   return builder;
   }

IlBuilder::SequenceEntry *
IlBuilder::blockEntry(TR::Block *block)
   {
   return new (_comp->trMemory()->trHeapMemory()) IlBuilder::SequenceEntry(block);
   }

IlBuilder::SequenceEntry *
IlBuilder::builderEntry(TR::IlBuilder *builder)
   {
   return new (_comp->trMemory()->trHeapMemory()) IlBuilder::SequenceEntry(builder);
   }

void
IlBuilder::appendBlock(TR::Block *newBlock, bool addEdge)
   {
   if (newBlock == NULL)
      {
      newBlock = emptyBlock();
      }

   _sequenceAppender->add(blockEntry(newBlock));

   if (_currentBlock && addEdge)
      {
      // if current block does not fall through to new block, use appendNoFallThroughBlock() rather than appendBlock()
      cfg()->addEdge(_currentBlock, newBlock);
      }

   // subsequent IL should generate to appended block
   _currentBlock = newBlock;
   }

void
IlBuilder::AppendBuilder(TR::IlBuilder *builder)
   {
   ILB_REPLAY("%s->AppendBuilder(%s);", REPLAY_BUILDER(this), REPLAY_BUILDER(builder));
   TR_ASSERT(builder->_partOfSequence == false, "builder cannot be in two places");

   builder->_partOfSequence = true;
   _sequenceAppender->add(builderEntry(builder));
   if (_currentBlock != NULL)
      {
      cfg()->addEdge(_currentBlock, builder->getEntry());
      _currentBlock = NULL;
      }

   TraceIL("IlBuilder[ %p ]::AppendBuilder %p\n", this, builder);

   // when trees are connected, exit block will swing down to become last trees in builder, so it can fall through to this block we're about to create
   // need to add edge explicitly because of this exit block sleight of hand
   appendNoFallThroughBlock();
   cfg()->addEdge(builder->getExit(), _currentBlock);
   }

TR::Node *
IlBuilder::loadValue(TR::IlValue *v)
   {
   return TR::Node::createLoad(v);
   }

void
IlBuilder::storeNode(TR::IlValue *dest, TR::Node *v)
   {
   appendBlock();
   genTreeTop(TR::Node::createStore(dest, v));
   }

void
IlBuilder::indirectStoreNode(TR::Node *addr, TR::Node *v)
   {
   TR::DataType dt = v->getDataType();
   TR::SymbolReference *storeSymRef = symRefTab()->findOrCreateArrayShadowSymbolRef(dt, addr);
   TR::ILOpCodes storeOp = comp()->il.opCodeForIndirectArrayStore(dt);
   genTreeTop(TR::Node::createWithSymRef(storeOp, 2, addr, v, 0, storeSymRef));
   }

TR::IlValue *
IlBuilder::indirectLoadNode(TR::IlType *dt, TR::Node *addr, bool isVectorLoad)
   {
   TR_ASSERT(dt->isPointer(), "indirectLoadNode must apply to pointer type");
   TR::IlType * baseType = dt->baseType();
   TR::DataType primType = baseType->getPrimitiveType();
   TR_ASSERT(primType != TR::NoType, "Dereferencing an untyped pointer.");
   TR::DataType symRefType = primType;
   if (isVectorLoad)
      symRefType = symRefType.scalarToVector();

   TR::SymbolReference *storeSymRef = symRefTab()->findOrCreateArrayShadowSymbolRef(symRefType, addr);

   TR::ILOpCodes loadOp = comp()->il.opCodeForIndirectArrayLoad(primType);
   if (isVectorLoad)
      {
      loadOp = TR::ILOpCode::convertScalarToVector(loadOp);
      baseType = _types->PrimitiveType(symRefType);
      }

   TR::Node *loadNode = TR::Node::createWithSymRef(loadOp, 1, 1, addr, storeSymRef);

   TR::IlValue *loadValue = NewValue(baseType);
   storeNode(loadValue, loadNode);
   return loadValue;
   }

/**
 * @brief Store an IlValue into a named local variable
 * @param varName the name of the local variable to be stored into. If the name has not been used before, this local variable will
 *                take the same data type as the value being written to it.
 * @param value IlValue that should be written to the local variable, which should be the same data type
 */
void
IlBuilder::Store(const char *varName, TR::IlValue *value)
   {
   ILB_REPLAY("%s->Store(\"%s\", %s);", REPLAY_BUILDER(this), varName, REPLAY_VALUE(value));

   if (!_methodBuilder->symbolDefined(varName))
      _methodBuilder->defineValue(varName, _types->PrimitiveType(value->getSymbol()->getDataType()));
   TR::IlValue *sym = lookupSymbol(varName);

   TraceIL("IlBuilder[ %p ]::Store %s %d gets %d\n", this, varName, sym->getCPIndex(), value->getCPIndex());
   storeNode(sym, loadValue(value));
   }

/**
 * @brief Store an IlValue into the same local value as another IlValue
 * @param dest IlValue that should now hold the same value as "value"
 * @param value IlValue that should overwrite "dest"
 */
void
IlBuilder::StoreOver(TR::IlValue *dest, TR::IlValue *value)
   {
   ILB_REPLAY("%s->StoreOver(%s, %s);", REPLAY_BUILDER(this), REPLAY_VALUE(dest), REPLAY_VALUE(value));
   Store(dest->getSymbol()->getAutoSymbol()->getName(), value);
   }

/**
 * @brief Store a vector IlValue into a named local variable
 * @param varName the name of the local variable to be vector stored into. if the name has not been used before, this local variable will
 *                take the same data type as the value being written to it. The width of this data will be determined by the vector
 *                data type of IlValue.
 * @param value IlValue with the vector data that should be written to the local variable, and should have the same data type
 */
void
IlBuilder::VectorStore(const char *varName, TR::IlValue *value)
   {
   ILB_REPLAY("%s->VectorStore(\"%s\", %s);", REPLAY_BUILDER(this), varName, REPLAY_VALUE(value));

   TR::Node *valueNode = loadValue(value);
   TR::DataType dt = valueNode->getDataType();
   if (!dt.isVector())
      {
      valueNode = TR::Node::create(TR::vsplats, 1, valueNode);
      dt = dt.scalarToVector();
      }

   if (!_methodBuilder->symbolDefined(varName))
      _methodBuilder->defineValue(varName, _types->PrimitiveType(dt));
   TR::IlValue *sym = lookupSymbol(varName);

   TraceIL("IlBuilder[ %p ]::VectorStore %s %d gets %d\n", this, varName, sym->getCPIndex(), value->getCPIndex());
   storeNode(sym, valueNode);
   }

/**
 * @brief Store an IlValue through a pointer
 * @param address the pointer address through which the value will be written
 * @param value IlValue that should be written at "address"
 */
void
IlBuilder::StoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   ILB_REPLAY("%s->StoreAt(%s, %s);", REPLAY_BUILDER(this), REPLAY_VALUE(address), REPLAY_VALUE(value));

   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "StoreAt needs an address operand");

   TraceIL("IlBuilder[ %p ]::StoreAt address %d gets %d\n", this, address->getCPIndex(), value->getCPIndex());
   indirectStoreNode(loadValue(address), loadValue(value));
   }

/**
 * @brief Store a vector IlValue through a pointer
 * @param address the pointer address through which the vector value will be written. The width of the store will be determined
 *                by the vector data type of IlValue.
 * @param value IlValue with the vector data that should be written  at "address"
 */
void
IlBuilder::VectorStoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   ILB_REPLAY("%s->VectorStoreAt(%s, %s);", REPLAY_BUILDER(this), REPLAY_VALUE(address), REPLAY_VALUE(value));

   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "VectorStoreAt needs an address operand");

   TraceIL("IlBuilder[ %p ]::VectorStoreAt address %d gets %d\n", this, address->getCPIndex(), value->getCPIndex());

   TR::Node *valueNode = loadValue(value);

   if (!valueNode->getDataType().isVector())
      valueNode = TR::Node::create(TR::vsplats, 1, valueNode);

   indirectStoreNode(loadValue(address), valueNode);
   }

TR::IlValue *
IlBuilder::CreateLocalArray(int32_t numElements, TR::IlType *elementType)
   {
   uint32_t size = numElements * elementType->getSize();
   TR::SymbolReference *localArraySymRef = symRefTab()->createLocalPrimArray(size,
                                                                             methodSymbol(),
                                                                             8 /*FIXME: JVM-specific - byte*/);
   localArraySymRef->setStackAllocatedArrayAccess();

   TR::Node *arrayAddress = TR::Node::createWithSymRef(TR::loadaddr, 0, localArraySymRef);
   TR::IlValue *arrayAddressValue = newValue(TR::Address);
   storeNode(arrayAddressValue, arrayAddress);

   ILB_REPLAY("%s = %s->CreateLocalArray(%d, %s);", REPLAY_VALUE(arrayAddressValue), REPLAY_BUILDER(this), numElements, REPLAY_TYPE(elementType));

   TraceIL("IlBuilder[ %p ]::CreateLocalArray array allocated %d bytes, address in %d\n", this, size, arrayAddressValue->getCPIndex());
   return arrayAddressValue;

   }

TR::IlValue *
IlBuilder::CreateLocalStruct(TR::IlType *structType)
   {
   //similar to CreateLocalArray except writing a method in StructType to get the struct size
   uint32_t size = structType->getSize();
   TR::SymbolReference *localStructSymRef = symRefTab()->createLocalPrimArray(size,
                                                                             methodSymbol(),
                                                                             8 /*FIXME: JVM-specific - byte*/);
   localStructSymRef->setStackAllocatedArrayAccess();

   TR::Node *structAddress = TR::Node::createWithSymRef(TR::loadaddr, 0, localStructSymRef);
   TR::IlValue *structAddressValue = newValue(TR::Address);
   storeNode(structAddressValue, structAddress);

   ILB_REPLAY("%s = %s->CreateLocalStruct(%s);", REPLAY_VALUE(structAddressValue), REPLAY_BUILDER(this), REPLAY_TYPE(newStructType));

   TraceIL("IlBuilder[ %p ]::CreateLocalStruct struct allocated %d bytes, address in %d\n", this, size, structAddressValue->getCPIndex());
   return structAddressValue;
   }

void
IlBuilder::StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value)
  {
  ILB_REPLAY("%s->StoreIndirect(\"%s\", \"%s\", %s, %s);", REPLAY_BUILDER(this), type, field, REPLAY_VALUE(object), REPLAY_VALUE(value));

  TR::SymbolReference *symRef = (TR::SymbolReference*)_types->FieldReference(type, field);
  TR::DataType fieldType = symRef->getSymbol()->getDataType();
  TraceIL("IlBuilder[ %p ]::StoreIndirect %s.%s (%d) into (%d)\n", this, type, field, value->getCPIndex(), object->getCPIndex());
  TR::ILOpCodes storeOp = comp()->il.opCodeForIndirectStore(fieldType);
  genTreeTop(TR::Node::createWithSymRef(storeOp, 2, loadValue(object), loadValue(value), 0, symRef));
  }

TR::IlValue *
IlBuilder::Load(const char *name)
   {
   TR::IlValue *nameSym = lookupSymbol(name);
   // looks awful much of the time, but we need to anchor the load in case symbol's value is changed later
   // copy propagation should clean it up
   appendBlock();
   TR::IlValue *returnValue = newValue(nameSym->getSymbol()->getDataType());
   TraceIL("IlBuilder[ %p ]::%d is Load %s (%d)\n", this, returnValue->getCPIndex(), name, nameSym->getCPIndex());
   storeNode(returnValue, TR::Node::createLoad(nameSym));

   ILB_REPLAY("%s = %s->Load(\"%s\");", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), name)
   return returnValue;
   }

TR::IlValue *
IlBuilder::VectorLoad(const char *name)
   {
   TR::IlValue *nameSymRef = lookupSymbol(name);
   appendBlock();
   TR::DataType returnType = nameSymRef->getSymbol()->getDataType();
   TR_ASSERT(returnType.isVector(), "VectorLoad must load symbol with a vector type");
   TR::IlValue *returnValue = newValue(returnType);
   TraceIL("IlBuilder[ %p ]::%d is VectorLoad %s (%d)\n", this, returnValue->getCPIndex(), name, nameSymRef->getCPIndex());

   TR::Node *loadNode = TR::Node::createWithSymRef(0, TR::comp()->il.opCodeForDirectLoad(returnType), 0, nameSymRef);
   storeNode(returnValue, loadNode);

   ILB_REPLAY("%s = %s->Load(\"%s\");", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), name)
   return returnValue;
   }

TR::IlValue *
IlBuilder::LoadIndirect(const char *type, const char *field, TR::IlValue *object)
   {
   TR::SymbolReference *symRef = (TR::SymbolReference *)_types->FieldReference(type, field);
   TR::DataType fieldType = symRef->getSymbol()->getDataType();
   TR::IlValue *returnValue = newValue(fieldType);
   TraceIL("IlBuilder[ %p ]::%d is LoadIndirect %s.%s from (%d)\n", this, returnValue->getCPIndex(), type, field, object->getCPIndex());
   storeNode(returnValue, TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(fieldType), 1, loadValue(object), 0, symRef));
   ILB_REPLAY("%s = %s->LoadIndirect(%s, %s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), type, field, REPLAY_VALUE(object));
   return returnValue;
   }

TR::IlValue *
IlBuilder::LoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "LoadAt needs an address operand");
   TR::IlValue *returnValue = indirectLoadNode(dt, loadValue(address));
   TraceIL("IlBuilder[ %p ]::%d is LoadAt type %d address %d\n", this, returnValue->getCPIndex(), dt->getPrimitiveType(), address->getCPIndex());
   ILB_REPLAY("%s = %s->LoadAt(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_TYPE(dt), REPLAY_VALUE(address));
   return returnValue;
   }

TR::IlValue *
IlBuilder::VectorLoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "LoadAt needs an address operand");
   TR::IlValue *returnValue = indirectLoadNode(dt, loadValue(address), true);
   TraceIL("IlBuilder[ %p ]::%d is VectorLoadAt type %d address %d\n", this, returnValue->getCPIndex(), dt->getPrimitiveType(), address->getCPIndex());
   ILB_REPLAY("%s = %s->LoadAt(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_TYPE(dt), REPLAY_VALUE(address));
   return returnValue;
   }

TR::IlValue *
IlBuilder::IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index)
   {
   TR::IlType *elemType = dt->baseType();
   TR_ASSERT(elemType != NULL, "IndexAt should be called with pointer type");
   TR_ASSERT(elemType->getPrimitiveType() != TR::NoType, "Cannot use IndexAt with pointer to NoType.");
   TR::Node *baseNode = TR::Node::createLoad(base);
   TR::Node *indexNode = TR::Node::createLoad(index);
   TR::Node *elemSizeNode;
   TR::ILOpCodes addOp, mulOp;
   TR::DataType indexType = indexNode->getSymbol()->getDataType();
   if (TR::Compiler->target.is64Bit())
      {
      if (indexType != TR::Int64)
         {
         TR::ILOpCodes op = TR::DataType::getDataTypeConversion(indexType, TR::Int64);
         indexNode = TR::Node::create(op, 1, indexNode);
         }
      elemSizeNode = TR::Node::lconst(elemType->getSize());
      addOp = TR::aladd;
      mulOp = TR::lmul;
      }
   else
      {
      TR::DataType targetType = TR::Int32;
      if (indexType != targetType)
         {
         TR::ILOpCodes op = TR::DataType::getDataTypeConversion(indexType, targetType);
         indexNode = TR::Node::create(op, 1, indexNode);
         }
      elemSizeNode = TR::Node::iconst(elemType->getSize());
      addOp = TR::aiadd;
      mulOp = TR::imul;
      }

   TR::Node *offsetNode = TR::Node::create(mulOp, 2, indexNode, elemSizeNode);
   TR::Node *addrNode = TR::Node::create(addOp, 2, baseNode, offsetNode);

   TR::IlValue *address = NewValue(Address);
   storeNode(address, addrNode);

   ILB_REPLAY("%s = %s->IndexAt(%s, %s, %s);", REPLAY_VALUE(address), REPLAY_BUILDER(this), REPLAY_TYPE(dt), REPLAY_VALUE(base), REPLAY_VALUE(index));
   TraceIL("IlBuilder[ %p ]::%d is IndexAt(%s) base %d index %d\n", this, address->getCPIndex(), dt->getName(), base->getCPIndex(), index->getCPIndex());

   return address;
   }

/**
 * @brief Generate IL to load the address of a struct field.
 *
 * The address of the field is calculated by adding the field's offset to the
 * base address of the struct. The address is also converted (type casted) to
 * a pointer to the type of the field.
 */
TR::IlValue *
IlBuilder::StructFieldInstanceAddress(const char* structName, const char* fieldName, TR::IlValue* obj) {
   auto offset = typeDictionary()->OffsetOf(structName, fieldName);
   auto ptype = typeDictionary()->PointerTo(typeDictionary()->GetFieldType(structName, fieldName));
   auto addr = Add(obj, ConstInt64(offset));
   return ConvertTo(ptype, addr);
}

/**
 * @brief Generate IL to load the address of a union field.
 *
 * The address of the field is simply the base address of the union, since the
 * offset of all union fields is zero.
 */
TR::IlValue *
IlBuilder::UnionFieldInstanceAddress(const char* unionName, const char* fieldName, TR::IlValue* obj) {
   auto ptype = typeDictionary()->PointerTo(typeDictionary()->UnionFieldType(unionName, fieldName));
   return ConvertTo(ptype, obj);
}

TR::IlValue *
IlBuilder::NullAddress()
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Address);
   storeNode(returnValue, TR::Node::aconst(0));
   ILB_REPLAY("%s = %s->NullAddress();", REPLAY_VALUE(address), REPLAY_BUILDER(this));
   TraceIL("IlBuilder[ %p ]::%d is NullAddress\n", this, returnValue->getCPIndex());
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt8(int8_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Int8);
   storeNode(returnValue, TR::Node::bconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt8 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt8(%d);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt16(int16_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Int16);
   storeNode(returnValue, TR::Node::sconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt16 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt16(%d);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt32(int32_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Int32);
   storeNode(returnValue, TR::Node::iconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt32 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt32(%d);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt64(int64_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Int64);
   storeNode(returnValue, TR::Node::lconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt64 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt64(%ld);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstFloat(float value)
   {
   appendBlock();
   TR::Node *fconstNode = TR::Node::create(0, TR::fconst, 0);
   fconstNode->setFloat(value);
   TR::IlValue *returnValue = NewValue(Float);
   storeNode(returnValue, fconstNode);
   TraceIL("IlBuilder[ %p ]::%d is ConstFloat %f\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstFloat(%f);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstDouble(double value)
   {
   appendBlock();
   TR::Node *dconstNode = TR::Node::create(0, TR::dconst, 0);
   dconstNode->setDouble(value);
   TR::IlValue *returnValue = NewValue(Double);
   storeNode(returnValue, dconstNode);
   TraceIL("IlBuilder[ %p ]::%d is ConstDouble %lf\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstDouble(%lf);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstString(const char * const value)
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Address);
   storeNode(returnValue, TR::Node::aconst((uintptrj_t)value));
   TraceIL("IlBuilder[ %p ]::%d is ConstString %p\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstString(%p);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstAddress(const void * const value)
   {
   appendBlock();
   TR::IlValue *returnValue = NewValue(Address);
   storeNode(returnValue, TR::Node::aconst((uintptrj_t)value));
   TraceIL("IlBuilder[ %p ]::%d is ConstAddress %p\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstAddress(%p);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInteger(TR::IlType *intType, int64_t value)
   {
   if      (intType == Int8)  return ConstInt8 ((int8_t)  value);
   else if (intType == Int16) return ConstInt16((int16_t) value);
   else if (intType == Int32) return ConstInt32((int32_t) value);
   else if (intType == Int64) return ConstInt64(          value);

   TR_ASSERT(0, "unknown integer type");
   return NULL;
   }

TR::IlValue *
IlBuilder::ConvertTo(TR::IlType *t, TR::IlValue *v)
   {
   TR::DataType typeFrom = v->getSymbol()->getDataType();
   TR::DataType typeTo = t->getPrimitiveType();
   if (typeFrom == typeTo)
      {
      TraceIL("IlBuilder[ %p ]::%d is ConvertTo (already has type %s) %d\n", this, v->getCPIndex(), t->getName(), v->getCPIndex());
      return v;
      }
   TR::IlValue *convertedValue = convertTo(t, v, false);
   TraceIL("IlBuilder[ %p ]::%d is ConvertTo(%s) %d\n", this, convertedValue->getCPIndex(), t->getName(), v->getCPIndex());
   ILB_REPLAY("%s = %s->ConvertTo(%s, %s);", REPLAY_VALUE(convertedValue), REPLAY_BUILDER(this), REPLAY_TYPE(t), REPLAY_VALUE(value));
   return convertedValue;
   }

TR::IlValue *
IlBuilder::UnsignedConvertTo(TR::IlType *t, TR::IlValue *v)
   {
   TR::DataType typeFrom = v->getSymbol()->getDataType();
   TR::DataType typeTo = t->getPrimitiveType();
   if (typeFrom == typeTo)
      {
      TraceIL("IlBuilder[ %p ]::%d is UnsignedConvertTo (already has type %s) %d\n", this, v->getCPIndex(), t->getName(), v->getCPIndex());
      return v;
      }
   TR::IlValue *convertedValue = convertTo(t, v, true);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedConvertTo(%s) %d\n", this, convertedValue->getCPIndex(), t->getName(), v->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedConvertTo(%s, %s);", REPLAY_VALUE(convertedValue), REPLAY_BUILDER(this), REPLAY_TYPE(t), REPLAY_VALUE(value));
   return convertedValue;
   }

TR::IlValue *
IlBuilder::convertTo(TR::IlType *t, TR::IlValue *v, bool needUnsigned)
   {
   TR::DataType typeFrom = v->getSymbol()->getDataType();
   TR::DataType typeTo = t->getPrimitiveType();

   appendBlock();
   TR::ILOpCodes convertOp = ILOpCode::getProperConversion(typeFrom, typeTo, needUnsigned);
   TR_ASSERT(convertOp != TR::BadILOp, "Builder [ %p ] unknown conversion requested for value %d (TR::DataType %d) to type %s", this, v->getCPIndex(), (int)typeFrom, t->getName());

   TR::Node *result = TR::Node::create(convertOp, 1, loadValue(v));
   TR::IlValue *convertedValue = NewValue(t);
   storeNode(convertedValue, result);
   return convertedValue;
   }

TR::IlValue *
IlBuilder::unaryOp(TR::ILOpCodes op, TR::IlValue *v)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(v->getSymbol()->getDataType());
   TR::Node *valueNode = loadValue(v);
   TR::Node *result = TR::Node::create(op, 1, valueNode);
   storeNode(returnValue, result);
   return returnValue;
   }

void
IlBuilder::doVectorConversions(TR::Node **leftPtr, TR::Node **rightPtr)
   {
   TR::Node *    left  = *leftPtr;
   TR::DataType lType = left->getDataType();

   TR::Node *    right = *rightPtr;
   TR::DataType rType = right->getDataType();

   if (lType.isVector() && !rType.isVector())
      *rightPtr = TR::Node::create(TR::vsplats, 1, right);

   if (!lType.isVector() && rType.isVector())
      *leftPtr = TR::Node::create(TR::vsplats, 1, left);
   }

TR::Node*
IlBuilder::binaryOpNodeFromNodes(TR::ILOpCodes op,
                             TR::Node *leftNode,
                             TR::Node *rightNode) 
   {
   TR::DataType leftType = leftNode->getDataType();
   TR::DataType rightType = rightNode->getDataType();
   bool isAddressBump = ((leftType == TR::Address) &&
                            (rightType == TR::Int32 || rightType == TR::Int64));
   bool isRevAddressBump = ((rightType == TR::Address) &&
                               (leftType == TR::Int32 || leftType == TR::Int64));
   TR_ASSERT(leftType == rightType || isAddressBump || isRevAddressBump, "binaryOp requires both left and right operands to have same type or one is address and other is Int32/64");

   if (isRevAddressBump) // swap them
      {
      TR::Node *save = leftNode;
      leftNode = rightNode;
      rightNode = save;
      }
   
   return TR::Node::create(op, 2, leftNode, rightNode);
   }

TR::IlValue *
IlBuilder::binaryOpFromNodes(TR::ILOpCodes op,
                             TR::Node *leftNode,
                             TR::Node *rightNode) 
   {
   TR::Node *result = binaryOpNodeFromNodes(op, leftNode, rightNode);
   TR::IlValue *returnValue = newValue(result->getDataType());
   storeNode(returnValue, result);
   return returnValue;
   } 

TR::IlValue *
IlBuilder::binaryOpFromOpMap(OpCodeMapper mapOp,
                             TR::IlValue *left,
                             TR::IlValue *right)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);

   doVectorConversions(&leftNode, &rightNode);

   TR::DataType leftType = leftNode->getDataType();
   return binaryOpFromNodes(mapOp(leftType), leftNode, rightNode);
   }

TR::IlValue *
IlBuilder::binaryOpFromOpCode(TR::ILOpCodes op,
                              TR::IlValue *left,
                              TR::IlValue *right)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   //doVectorConversions(&left, &right);
   return binaryOpFromNodes(op, leftNode, rightNode);
   }

TR::IlValue *
IlBuilder::compareOp(TR_ComparisonTypes ct,
                     bool needUnsigned,
                     TR::IlValue *left,
                     TR::IlValue *right)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCodes op = TR::ILOpCode::compareOpCode(leftNode->getDataType(), ct, needUnsigned);
   return binaryOpFromOpCode(op, left, right);
   }

TR::IlValue *
IlBuilder::NotEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=compareOp(TR_cmpNE, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is NotEqualTo %d != %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->NotEqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

void
IlBuilder::Goto(TR::IlBuilder **dest)
   {
   *dest = createBuilderIfNeeded(*dest);
   Goto(*dest);
   }

void
IlBuilder::Goto(TR::IlBuilder *dest)
   {
   TR_ASSERT(dest != NULL, "This goto implementation requires a non-NULL builder object");
   TraceIL("IlBuilder[ %p ]::Goto %p\n", this, dest);
   appendGoto(dest->getEntry());
   setDoesNotComeBack();

   ILB_REPLAY("%s->Goto(%s);", REPLAY_BUILDER(this), REPLAY_BUILDER(dest));
   }

void
IlBuilder::Return()
   {
   ILB_REPLAY("%s->Return();", REPLAY_BUILDER(this));
   TraceIL("IlBuilder[ %p ]::Return\n", this);
   appendBlock();
   TR::Node *returnNode = TR::Node::create(TR::ILOpCode::returnOpCode(TR::NoType));
   genTreeTop(returnNode);
   cfg()->addEdge(_currentBlock, cfg()->getEnd());
   setDoesNotComeBack();
   }

void
IlBuilder::Return(TR::IlValue *value)
   {
   ILB_REPLAY("%s->Return(%s);", REPLAY_BUILDER(this), REPLAY_VALUE(value));
   TraceIL("IlBuilder[ %p ]::Return %d\n", this, value->getCPIndex());
   appendBlock();
   TR::Node *returnNode = TR::Node::create(TR::ILOpCode::returnOpCode(value->getSymbol()->getDataType()), 1, loadValue(value));
   genTreeTop(returnNode);
   cfg()->addEdge(_currentBlock, cfg()->getEnd());
   setDoesNotComeBack();
   }

TR::IlValue *
IlBuilder::Sub(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = NULL;
   if (left->getSymbol()->getDataType() == TR::Address)
      {
      if (right->getSymbol()->getDataType() == TR::Int32)
         returnValue = binaryOpFromNodes(TR::aiadd, loadValue(left), loadValue(Sub(ConstInt32(0), right)));
      else if (right->getSymbol()->getDataType() == TR::Int64)
         returnValue = binaryOpFromNodes(TR::aladd, loadValue(left), loadValue(Sub(ConstInt64(0), right)));
      }
   if (returnValue == NULL)
      returnValue=binaryOpFromOpMap(TR::ILOpCode::subtractOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Sub %d - %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Sub(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

static TR::ILOpCodes addOpCode(TR::DataType type)
   {
   return TR::ILOpCode::addOpCode(type, TR::Compiler->target.is64Bit());
   }

static TR::ILOpCodes unsignedAddOpCode(TR::DataType type)
   {
   return TR::ILOpCode::unsignedAddOpCode(type, TR::Compiler->target.is64Bit());
   }

TR::IlValue *
IlBuilder::Add(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = NULL;
   if (left->getSymbol()->getDataType() == TR::Address)
      {
      if (right->getSymbol()->getDataType() == TR::Int32)
         returnValue = binaryOpFromNodes(TR::aiadd, loadValue(left), loadValue(right));
      else if (right->getSymbol()->getDataType() == TR::Int64)
         returnValue = binaryOpFromNodes(TR::aladd, loadValue(left), loadValue(right));
      }
   if (returnValue == NULL)
      returnValue = binaryOpFromOpMap(addOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Add %d + %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Add(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

/*
 * blockThrowsException:
 * ....
 * goto blockAfterExceptionHandler 
 * Handler:
 * ....
 * goto blockAfterExceptionHandler 
 * blockAfterExceptionHandler:
 */
void
IlBuilder::appendExceptionHandler(TR::Block *blockThrowsException, TR::IlBuilder **handler, uint32_t catchType)
   {
   //split block after overflowCHK, and add an goto to blockAfterExceptionHandler
   appendBlock();
   TR::Block *blockWithGoto = _currentBlock;
   TR::Node *gotoNode = TR::Node::create(NULL, TR::Goto);
   genTreeTop(gotoNode);
   _currentBlock = NULL;
   _currentBlockNumber = -1;
 
   //append handler, add exception edge and merge edge
   *handler = createBuilderIfNeeded(*handler);
   TR_ASSERT(*handler != NULL, "exception handler cannot be NULL\n");
   (*handler)->_isHandler = true;
   cfg()->addExceptionEdge(blockThrowsException, (*handler)->getEntry());
   AppendBuilder(*handler);
   (*handler)->setHandlerInfo(catchType);

   TR::Block *blockAfterExceptionHandler = _currentBlock;
   TraceIL("blockAfterExceptionHandler block_%d, blockWithGoto block_%d \n", blockAfterExceptionHandler->getNumber(), blockWithGoto->getNumber());
   gotoNode->setBranchDestination(blockAfterExceptionHandler->getEntry());
   cfg()->addEdge(blockWithGoto, blockAfterExceptionHandler);
   }

void
IlBuilder::setHandlerInfo(uint32_t catchType)
   {
   TR::Block *catchBlock = getEntry();
   catchBlock->setIsCold();
   catchBlock->setHandlerInfoWithOutBCInfo(catchType, comp()->getInlineDepth(), -1, _methodSymbol->getResolvedMethod(), comp());
   }

TR::Node*
IlBuilder::genOverflowCHKTreeTop(TR::Node *operationNode, TR::ILOpCodes overflow)
   {
   TR::Node *overflowChkNode = TR::Node::createWithRoomForOneMore(overflow, 3, symRefTab()->findOrCreateOverflowCheckSymbolRef(_methodSymbol), operationNode, operationNode->getFirstChild(), operationNode->getSecondChild());
   overflowChkNode->setOverflowCheckOperation(operationNode->getOpCodeValue());
   genTreeTop(overflowChkNode);
   return overflowChkNode;
   }

TR::IlValue *
IlBuilder::genOperationWithOverflowCHK(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode, TR::IlBuilder **handler, TR::ILOpCodes overflow)
   {
   /*
    * BB1:
    *    overflowCHK
    *       operation(add/sub/mul)
    *          child1
    *          child2
    *       =>child1
    *       =>child2
    *    store
    *       => operation
    * BB2:
    *    goto BB3 
    * Handler:
    *    ...
    * BB3:
    *    continue
    */
   TR::Node *operationNode = binaryOpNodeFromNodes(op, leftNode, rightNode);
   TR::Node *overflowChkNode = genOverflowCHKTreeTop(operationNode, overflow);

   TR::Block *blockWithOverflowCHK = _currentBlock;
   TR::IlValue *resultValue = newValue(operationNode->getDataType());
   genTreeTop(TR::Node::createStore(resultValue, operationNode));

   appendExceptionHandler(blockWithOverflowCHK, handler, TR::Block::CanCatchOverflowCheck);
   return resultValue;
   }

// This function takes 4 arguments and generate the addValue.
// This function is called by AddWithOverflow and AddWithUnsignedOverflow.
TR::ILOpCodes 
IlBuilder::getOpCode(TR::IlValue *leftValue, TR::IlValue *rightValue)
   {
   appendBlock();

   TR::ILOpCodes op;
   if (leftValue->getSymbol()->getDataType() == TR::Address)
      {
      if (rightValue->getSymbol()->getDataType() == TR::Int32)
         op = TR::aiadd;
      else if (rightValue->getSymbol()->getDataType() == TR::Int64)
         op = TR::aladd;
      else 
         TR_ASSERT(0, "the right child type must be either TR::Int32 or TR::Int64 when the left child of Add is TR::Address\n");
      }    
   else 
      {
      op = addOpCode(leftValue->getSymbol()->getDataType());
      }
   return op; 
   }

TR::IlValue *
IlBuilder::AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCodes opcode = getOpCode(left, right);
   TR::IlValue *addValue = genOperationWithOverflowCHK(opcode, leftNode, rightNode, handler, TR::OverflowCHK);
   TraceIL("IlBuilder[ %p ]::%d is AddWithOverflow %d + %d\n", this, addValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   //ILB_REPLAY("%s = %s->AddWithOverflow(%s, %s);", REPLAY_VALUE(addValue), REPLAY_BUILDER(this), REPLAY_BUILDER(*handler), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return addValue;
   }

TR::IlValue *
IlBuilder::AddWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCodes opcode = getOpCode(left, right);
   TR::IlValue *addValue = genOperationWithOverflowCHK(opcode, leftNode, rightNode, handler, TR::UnsignedOverflowCHK);
   TraceIL("IlBuilder[ %p ]::%d is AddWithUnsignedOverflow %d + %d\n", this, addValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   //ILB_REPLAY("%s = %s->AddWithUnsignedOverflow(%s, %s, %s);", REPLAY_VALUE(addValue), REPLAY_BUILDER(this), REPLAY_PTRTOBUILDER(handler), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return addValue;
   }

TR::IlValue *
IlBuilder::SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::IlValue *subValue = genOperationWithOverflowCHK(TR::ILOpCode::subtractOpCode(leftNode->getDataType()), leftNode, rightNode, handler, TR::OverflowCHK);
   TraceIL("IlBuilder[ %p ]::%d is SubWithOverflow %d + %d\n", this, subValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   return subValue;
   }

TR::IlValue *
IlBuilder::SubWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::IlValue *unsignedSubValue = genOperationWithOverflowCHK(TR::ILOpCode::subtractOpCode(leftNode->getDataType()), leftNode, rightNode, handler, TR::UnsignedOverflowCHK);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedSubWithOverflow %d + %d\n", this, unsignedSubValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   //ILB_REPLAY("%s = %s->UnsignedSubWithOverflow(%s, %s, %s);", REPLAY_VALUE(unsignedSubValue), REPLAY_BUILDER(this), REPLAY_PTRTOBUILDER(handler), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return unsignedSubValue;
   }

TR::IlValue *
IlBuilder::MulWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::IlValue *mulValue = genOperationWithOverflowCHK(TR::ILOpCode::multiplyOpCode(leftNode->getDataType()), leftNode, rightNode, handler, TR::OverflowCHK);
   TraceIL("IlBuilder[ %p ]::%d is MulWithOverflow %d + %d\n", this, mulValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   return mulValue;
   }

TR::IlValue *
IlBuilder::Mul(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::multiplyOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Mul %d * %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Mul(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Div(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::divideOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Div %d / %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Div(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::And(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::andOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is And %d & %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->And(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Or(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::orOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Or %d | %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Or(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Xor(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::xorOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Xor %d ^ %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Xor(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::ShiftL(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::shiftLeftOpCode, v, amount);
   TraceIL("IlBuilder[ %p ]::%d is shr %d << %d\n", this, returnValue->getCPIndex(), v->getCPIndex(), amount->getCPIndex());
   ILB_REPLAY("%s = %s->ShiftL(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(v), REPLAY_VALUE(amount));
   return returnValue;
   }

TR::IlValue *
IlBuilder::ShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::shiftRightOpCode, v, amount);
   TraceIL("IlBuilder[ %p ]::%d is shr %d >> %d\n", this, returnValue->getCPIndex(), v->getCPIndex(), amount->getCPIndex());
   ILB_REPLAY("%s = %s->ShiftR(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(v), REPLAY_VALUE(amount));
   return returnValue;
   }

TR::IlValue *
IlBuilder::UnsignedShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::unsignedShiftRightOpCode, v, amount);
   TraceIL("IlBuilder[ %p ]::%d is unsigned shr %d >> %d\n", this, returnValue->getCPIndex(), v->getCPIndex(), amount->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedShiftR(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(v), REPLAY_VALUE(amount));
   return returnValue;
   }

/*
 * @brief IfAnd service for constructing short circuit AND conditional nests (like the && operator)
 * @param allTrueBuilder builder containing operations to execute if all conditional tests evaluate to true
 * @param anyFalseBuilder builder containing operations to execute if any conditional test is false
 * @param numTerms the number of conditional terms
 * @param ... for each term, provide a TR::IlBuilder object and a TR::IlValue object that evaluates a condition (builder is where all the operations to evaluate the condition go, the value is the final result of the condition)
 *
 * Example:
 * TR::IlBuilder *cond1Builder = OrphanBuilder();
 * TR::IlValue *cond1 = cond1Builder->GreaterOrEqual(
 *                      cond1Builder->   Load("x"),
 *                      cond1Builder->   Load("lower"));
 * TR::IlBuilder *cond2Builder = OrphanBuilder();
 * TR::IlValue *cond2 = cond2Builder->LessThan(
 *                      cond2Builder->   Load("x"),
 *                      cond2Builder->   Load("upper"));
 * TR::IlBuilder *inRange = NULL, *outOfRange = NULL;
 * IfAnd(&inRange, &outOfRange, 2, cond1Builder, cond1, cond2Builder, cond2);
 */
void
IlBuilder::IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, ...)
   {
   TR::IlBuilder *mergePoint = OrphanBuilder();
   *allTrueBuilder = createBuilderIfNeeded(*allTrueBuilder);
   *anyFalseBuilder = createBuilderIfNeeded(*anyFalseBuilder);

   va_list terms;
   va_start(terms, numTerms);
   for (int32_t t=0;t < numTerms;t++)
      {
      TR::IlBuilder *condBuilder = va_arg(terms, TR::IlBuilder*);
      TR::IlValue *condValue = va_arg(terms, TR::IlValue*);
      AppendBuilder(condBuilder);
      condBuilder->IfCmpEqualZero(anyFalseBuilder, condValue);
      // otherwise fall through to test next term
      }
   va_end(terms);

   // if control gets here, all the provided terms were true
   AppendBuilder(*allTrueBuilder);
   Goto(mergePoint);

   // also need to handle the false case
   AppendBuilder(*anyFalseBuilder);
   Goto(mergePoint);

   AppendBuilder(mergePoint);

   // return state for "this" can get confused by the Goto's in this service
   setComesBack();
   }

/*
 * @brief IfOr service for constructing short circuit OR conditional nests (like the || operator)
 * @param anyTrueBuilder builder containing operations to execute if any conditional test evaluates to true
 * @param allFalseBuilder builder containing operations to execute if all conditional tests are false
 * @param numTerms the number of conditional terms
 * @param ... for each term, provide a TR::IlBuilder object and a TR::IlValue object that evaluates a condition (builder is where all the operations to evaluate the condition go, the value is the final result of the condition)
 *
 * Example:
 * TR::IlBuilder *cond1Builder = OrphanBuilder();
 * TR::IlValue *cond1 = cond1Builder->LessThan(
 *                      cond1Builder->   Load("x"),
 *                      cond1Builder->   Load("lower"));
 * TR::IlBuilder *cond2Builder = OrphanBuilder();
 * TR::IlValue *cond2 = cond2Builder->GreaterOrEqual(
 *                      cond2Builder->   Load("x"),
 *                      cond2Builder->   Load("upper"));
 * TR::IlBuilder *inRange = NULL, *outOfRange = NULL;
 * IfOr(&outOfRange, &inRange, 2, cond1Builder, cond1, cond2Builder, cond2);
 */
void
IlBuilder::IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, ...)
   {
   TR::IlBuilder *mergePoint = OrphanBuilder();
   *anyTrueBuilder = createBuilderIfNeeded(*anyTrueBuilder);
   *allFalseBuilder = createBuilderIfNeeded(*allFalseBuilder);

   va_list terms;
   va_start(terms, numTerms);
   for (int32_t t=0;t < numTerms-1;t++)
      {
      TR::IlBuilder *condBuilder = va_arg(terms, TR::IlBuilder*);
      TR::IlValue *condValue = va_arg(terms, TR::IlValue*);
      AppendBuilder(condBuilder);
      condBuilder->IfCmpNotEqualZero(anyTrueBuilder, condValue);
      // otherwise fall through to test next term
      }

   // reverse condition on last term so that it can fall through to anyTrueBuilder
   TR::IlBuilder *condBuilder = va_arg(terms, TR::IlBuilder*);
   TR::IlValue *condValue = va_arg(terms, TR::IlValue*);
   AppendBuilder(condBuilder);
   condBuilder->IfCmpEqualZero(allFalseBuilder, condValue);
   va_end(terms);

   // any true term will end up here
   AppendBuilder(*anyTrueBuilder);
   Goto(mergePoint);

   // if control gets here, all the provided terms were false
   AppendBuilder(*allFalseBuilder);
   Goto(mergePoint);

   AppendBuilder(mergePoint);

   // return state for "this" can get confused by the Goto's in this service
   setComesBack();
   }

TR::IlValue *
IlBuilder::EqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=compareOp(TR_cmpEQ, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is EqualTo %d == %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->EqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

void
IlBuilder::integerizeAddresses(TR::IlValue **leftPtr, TR::IlValue **rightPtr)
   {
   TR::IlValue *left = *leftPtr;
   if (left->getSymbol()->getDataType() == TR::Address)
      *leftPtr = ConvertTo(Word, left);

   TR::IlValue *right = *rightPtr;
   if (right->getSymbol()->getDataType() == TR::Address)
      *rightPtr = ConvertTo(Word, right);
   }

TR::IlValue *
IlBuilder::LessThan(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpLT, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is LessThan %d < %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->LessThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::UnsignedLessThan(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpLT, true, left, right);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedLessThan %d < %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedLessThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::LessOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpLE, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is LessOrEqualTo %d <= %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->LessOrEqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::UnsignedLessOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpLE, true, left, right);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedLessOrEqualTo %d <= %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedLessOrEqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::GreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpGT, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is GreaterThan %d > %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->GreaterThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::UnsignedGreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpGT, true, left, right);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedGreaterThan %d > %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedGreaterThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::GreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpGE, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is GreaterOrEqualTo %d >= %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->GreaterOrEqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::UnsignedGreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   integerizeAddresses(&left, &right);
   TR::IlValue *returnValue=compareOp(TR_cmpGE, true, left, right);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedGreaterOrEqualTo %d >= %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedGreaterOrEqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue** 
IlBuilder::processCallArgs(TR::Compilation *comp, int numArgs, va_list args)
   {
   TR::IlValue ** argValues = (TR::IlValue **) comp->trMemory()->allocateHeapMemory(numArgs * sizeof(TR::IlValue *));
   for (int32_t a=0;a < numArgs;a++)
      {
      argValues[a] = va_arg(args, TR::IlValue*);
      }
   return argValues;
   }

/*
 * \param numArgs 
 *    Number of actual arguments for the method  plus 1 
 * \param ... 
 *    The list is a computed address followed by the actual arguments
 */
TR::IlValue *
IlBuilder::ComputedCall(const char *functionName, int32_t numArgs, ...)
   {
   // TODO: figure out Call REPLAY
   TraceIL("IlBuilder[ %p ]::ComputedCall %s\n", this, functionName);
   va_list args;
   va_start(args, numArgs);
   TR::IlValue **argValues = processCallArgs(_comp, numArgs, args);
   va_end(args);
   TR::ResolvedMethod *resolvedMethod = _methodBuilder->lookupFunction(functionName);
   if (resolvedMethod == NULL && _methodBuilder->RequestFunction(functionName))
      resolvedMethod = _methodBuilder->lookupFunction(functionName);
   TR_ASSERT(resolvedMethod, "Could not identify function %s\n", functionName);

   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateComputedStaticMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod);
   return genCall(methodSymRef, numArgs, argValues, false /*isDirectCall*/);
   }

/*
 * \param numArgs 
 *    Number of actual arguments for the method  plus 1 
 * \param argValues
 *    the computed address followed by the actual arguments
 */
TR::IlValue *
IlBuilder::ComputedCall(const char *functionName, int32_t numArgs, TR::IlValue **argValues)
   {
   // TODO: figure out Call REPLAY
   TraceIL("IlBuilder[ %p ]::ComputedCall %s\n", this, functionName);
   TR::ResolvedMethod *resolvedMethod = _methodBuilder->lookupFunction(functionName);
   if (resolvedMethod == NULL && _methodBuilder->RequestFunction(functionName))
      resolvedMethod = _methodBuilder->lookupFunction(functionName);
   TR_ASSERT(resolvedMethod, "Could not identify function %s\n", functionName);

   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateComputedStaticMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod);
   return genCall(methodSymRef, numArgs, argValues, false /*isDirectCall*/);
   }

TR::IlValue *
IlBuilder::Call(const char *functionName, int32_t numArgs, ...)
   {
   // TODO: figure out Call REPLAY
   TraceIL("IlBuilder[ %p ]::Call %s\n", this, functionName);
   va_list args;
   va_start(args, numArgs);
   TR::IlValue **argValues = processCallArgs(_comp, numArgs, args);
   va_end(args);
   TR::ResolvedMethod *resolvedMethod = _methodBuilder->lookupFunction(functionName);
   if (resolvedMethod == NULL && _methodBuilder->RequestFunction(functionName))
      resolvedMethod = _methodBuilder->lookupFunction(functionName);
   TR_ASSERT(resolvedMethod, "Could not identify function %s\n", functionName);

   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateStaticMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod);
   return genCall(methodSymRef, numArgs, argValues);
   }

TR::IlValue *
IlBuilder::Call(const char *functionName, int32_t numArgs, TR::IlValue ** argValues)
   {
   // TODO: figure out Call REPLAY
   TraceIL("IlBuilder[ %p ]::Call %s\n", this, functionName);
   TR::ResolvedMethod *resolvedMethod = _methodBuilder->lookupFunction(functionName);
   if (resolvedMethod == NULL && _methodBuilder->RequestFunction(functionName))
      resolvedMethod = _methodBuilder->lookupFunction(functionName);
   TR_ASSERT(resolvedMethod, "Could not identify function %s\n", functionName);

   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateStaticMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod);
   return genCall(methodSymRef, numArgs, argValues);
   }

TR::IlValue *
IlBuilder::genCall(TR::SymbolReference *methodSymRef, int32_t numArgs, TR::IlValue ** argValues, bool isDirectCall /* true by default*/)
   {
   appendBlock();

   TR::DataType returnType = methodSymRef->getSymbol()->castToMethodSymbol()->getMethod()->returnType();
   TR::Node *callNode = TR::Node::createWithSymRef(isDirectCall? TR::ILOpCode::getDirectCall(returnType): TR::ILOpCode::getIndirectCall(returnType), numArgs, methodSymRef);

   // TODO: should really verify argument types here
   int32_t childIndex = 0;
   for (int32_t a=0;a < numArgs;a++)
      {
      TR::IlValue *arg = argValues[a];
      callNode->setAndIncChild(childIndex++, loadValue(arg));
      }

   // call has side effect and needs to be anchored under a treetop
   genTreeTop(callNode);

   if (returnType != TR::NoType)
      {
      TR::IlValue *returnValue = newValue(callNode->getDataType());
      genTreeTop(TR::Node::createStore(returnValue, callNode));
      return returnValue;
      }

   return NULL;
   }

/** \brief
 *     The service is used to atomically increase memory location [\p baseAddress + \p offset] by amount of \p value. 
 *
 *  \param value
 *     The amount to increase for the memory location.
 * 
 *  \note
 *	   This service currently only supports Int32/Int64.
 *
 *  \return
 *     The old value at the location [\p baseAddress + \p offset].
 */
TR::IlValue *
IlBuilder::AtomicAddWithOffset(TR::IlValue * baseAddress, TR::IlValue * offset, TR::IlValue * value)
   {
   TR_ASSERT(comp()->cg()->supportsAtomicAdd(), "this platform doesn't support AtomicAdd() yet");
   TR_ASSERT(baseAddress->getSymbol()->getDataType() == TR::Address, "baseAddress must be TR::Address");
   TR_ASSERT(offset == NULL || offset->getSymbol()->getDataType() == TR::Int32 || offset->getSymbol()->getDataType() == TR::Int64, "offset must be TR::Int32/64 or NULL");

   //Determine the implementation type and returnType by detecting "value"'s type
   TR::DataType returnType = value->getSymbol()->getDataType();
   TR_ASSERT(returnType == TR::Int32 || (returnType == TR::Int64 && TR::Compiler->target.is64Bit()), "AtomicAdd currently only supports Int32/64 values");
   TraceIL("IlBuilder[ %p ]::AtomicAddWithOffset (%d, %d, %d)\n", this, baseAddress->getCPIndex(), offset == NULL ? 0 : offset->getCPIndex(), value->getCPIndex());

   appendBlock();

   OMR::SymbolReferenceTable::CommonNonhelperSymbol atomicBitSymbol = returnType == TR::Int32 ? TR::SymbolReferenceTable::atomicAdd32BitSymbol : TR::SymbolReferenceTable::atomicAdd64BitSymbol;//lock add
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateCodeGenInlinedHelper(atomicBitSymbol); 
   TR::Node *callNode;
   //Evaluator will handle if it's (baseAddress+offset) or baseAddress based on the number of children the call node have 
   callNode = TR::Node::createWithSymRef(TR::ILOpCode::getDirectCall(returnType), offset == NULL ? 2 : 3, methodSymRef);
   callNode->setAndIncChild(0, loadValue(baseAddress));
   if (offset == NULL)
      {
      callNode->setAndIncChild(1, loadValue(value));
      }
   else
      {
      callNode->setAndIncChild(1, loadValue(offset));
      callNode->setAndIncChild(2, loadValue(value));
      }

   genTreeTop(callNode); 
   TR::IlValue *returnValue = newValue(callNode->getDataType());
   genTreeTop(TR::Node::createStore(returnValue, callNode)); 
   
   return returnValue; 
   }

/** \brief
 *     The service is used to atomically increase memory location \p baseAddress by amount of \p value. 
 *
 *  \param value
 *     The amount to increase for the memory location.
 *
 *  \note
 *     This service currently only supports Int32/Int64. 
 *
 *  \return
 *     The old value at the location \p baseAddress.
 */
TR::IlValue *
IlBuilder::AtomicAdd(TR::IlValue * baseAddress, TR::IlValue * value)
   {
   return AtomicAddWithOffset(baseAddress, NULL, value);    
   }


/**
 * \brief
 *  The service is for generating a treetop for transaction begin when the user needs to use transactional memory.
 *  At the end of transaction path, service will automatically generate transaction end instruction.
 *
 * \verbatim
 *   
 *    +---------------------------------+
 *    |<block_$tstart>                  |
 *    |==============                   |
 *    | TSTART                          |
 *    |    |                            |
 *    |    |_ <block_$persistentFailure>|
 *    |    |_ <block_$transientFailure> |
 *    |    |_ <block_$transaction>      |+------------------------+----------------------------------+
 *    |                                 |                         |                                  |
 *    +---------------------------------+                         |                                  |
 *                       |                                        |                                  |
 *                       v                                        V                                  V
 *    +-----------------------------------------+   +-------------------------------+    +-------------------------+     
 *    |<block_$transaction>                     |   |<block_$persistentFailure>     |    |<block_$transientFailure>|      
 *    |====================                     |   |===========================    |    |=========================|
 *    |     add (For illustration, we take add  |   |AtomicAdd (For illustration, we|    |   ...                   |
 *    |     ... as an example. User could       |   |...       take atomicAdd as an |    |   ...                   |
 *    |     ... replace it with other non-atomic|   |...       example. User could  |    |   ...                   |
 *    |     ... operations.)                    |   |...       replace it with other|    |   ...                   |
 *    |     ...                                 |   |...       atomic operations.)  |    |   ...                   |
 *    |     TEND                                |   |...                            |    |   ...                   |
 *    |goto --> block_$merge                    |   |goto->block_$merge             |    |goto->block_$merge       |
 *    +----------------------------------------+    +-------------------------------+    +-------------------------+
 *                      |                                          |                                 |
 *                      |                                          |                                 |
 *                      |------------------------------------------+---------------------------------+
 *                      |
 *                      v             
 *              +----------------+     
 *              | block_$merge   |     
 *              | ============== |   
 *              |      ...       |
 *              +----------------+   
 *
 * \endverbatim 
 *
 * \structure & \param value
 *     tstart
 *       |
 *       ----persistentFailure // This is a permanent failure, try atomic way to do it instead
 *       |
 *       ----transientFailure // Temporary failure, user can retry 
 *       |
 *       ----transaction // Success, use general(non-atomic) way to do it
 *                |
 *                ---- non-atomic operations
 *                |
 *                ---- ...
 *                |
 *                ---- tend
 *
 * \note
 *      If user's platform doesn't support TM, go to persistentFailure path directly/
 *      In this case, current IlBuilder walks around transientFailureBuilder and transactionBuilder
 *      and goes to persistentFailureBuilder.
 *      
 *      _currentBuilder
 *          |
 *          ->Goto()
 *              |   transientFailureBuilder 
 *              |   transactionBuilder
 *              |-->persistentFailurie
 */
void
IlBuilder::Transaction(TR::IlBuilder **persistentFailureBuilder, TR::IlBuilder **transientFailureBuilder, TR::IlBuilder **transactionBuilder)
    {   
    //This assertion is to rule out platforms which don't have tstart evaluator yet. 
    TR_ASSERT(comp()->cg()->hasTMEvaluator(), "this platform doesn't support tstart or tfinish evaluator yet");   
    
    //ILB_REPLAY("%s->TransactionBegin(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(persistentFailureBuilder), REPLAY_BUILDER(transientFailureBuilder), REPLAY_BUILDER(transactionBuilder)); 
    TraceIL("IlBuilder[ %p ]::transactionBegin %p, %p, %p, %p)\n", this, *persistentFailureBuilder, *transientFailureBuilder, *transactionBuilder);

    appendBlock();

    TR::Block *mergeBlock = emptyBlock();
    *persistentFailureBuilder = createBuilderIfNeeded(*persistentFailureBuilder);
    *transientFailureBuilder = createBuilderIfNeeded(*transientFailureBuilder);
    *transactionBuilder = createBuilderIfNeeded(*transactionBuilder);

    if (!comp()->cg()->getSupportsTM())
       {
       //if user's processor doesn't support TM.
       //we will walk around transaction and transientFailure paths

       Goto(persistentFailureBuilder);

       AppendBuilder(*transactionBuilder);
       AppendBuilder(*transientFailureBuilder);

       AppendBuilder(*persistentFailureBuilder);
       appendBlock(mergeBlock);
       return;
       }

    TR::Node *persistentFailureNode = TR::Node::create(TR::branch, 0, (*persistentFailureBuilder)->getEntry()->getEntry());
    TR::Node *transientFailureNode = TR::Node::create(TR::branch, 0, (*transientFailureBuilder)->getEntry()->getEntry());
    TR::Node *transactionNode = TR::Node::create(TR::branch, 0, (*transactionBuilder)->getEntry()->getEntry());

    TR::Node *tStartNode = TR::Node::create(TR::tstart, 3, persistentFailureNode, transientFailureNode, transactionNode);   
    tStartNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateTransactionEntrySymbolRef(comp()->getMethodSymbol()));
    
    genTreeTop(tStartNode);

    //connecting the block having tstart with persistentFailure's and transaction's blocks 
    cfg()->addEdge(_currentBlock, (*persistentFailureBuilder)->getEntry());
    cfg()->addEdge(_currentBlock, (*transientFailureBuilder)->getEntry());
    cfg()->addEdge(_currentBlock, (*transactionBuilder)->getEntry());

    appendNoFallThroughBlock();
    AppendBuilder(*transientFailureBuilder);
    gotoBlock(mergeBlock);

    AppendBuilder(*persistentFailureBuilder);
    gotoBlock(mergeBlock);

    AppendBuilder(*transactionBuilder);
    
    //ending the transaction at the end of transactionBuilder
    appendBlock();
    TR::Node *tEndNode=TR::Node::create(TR::tfinish,0);
    tEndNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateTransactionExitSymbolRef(comp()->getMethodSymbol()));
    genTreeTop(tEndNode);

	//Three IlBuilders above merged here
    appendBlock(mergeBlock);

	}  

/**
 * Generate XABORT instruction to abort transaction
 */
void
IlBuilder::TransactionAbort()
    {
    TraceIL("IlBuilder[ %p ]::transactionAbort", this);
    TR::Node *tAbortNode = TR::Node::create(TR::tabort, 0);
    tAbortNode->setSymbolReference(comp()->getSymRefTab()->findOrCreateTransactionAbortSymbolRef(comp()->getMethodSymbol()));
    genTreeTop(tAbortNode);
    }

void
IlBuilder::IfCmpNotEqualZero(TR::IlBuilder **target, TR::IlValue *condition)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpNotEqualZero(*target, condition);
   }

void
IlBuilder::IfCmpNotEqualZero(TR::IlBuilder *target, TR::IlValue *condition)
   {
   TR_ASSERT(target != NULL, "This IfCmpNotEqualZero requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpNotEqualZero(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(condition));
   TraceIL("IlBuilder[ %p ]::IfCmpNotEqualZero %d? -> [ %p ] B%d\n", this, condition->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpNotEqualZero(condition, target->getEntry());
   }

void
IlBuilder::IfCmpNotEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpNotEqual(*target, left, right);
   }

void
IlBuilder::IfCmpNotEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR_ASSERT(target != NULL, "This IfCmpNotEqual requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpNotEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpNotEqual %d == %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpNE, false, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpEqualZero(TR::IlBuilder **target, TR::IlValue *condition)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpEqualZero(*target, condition);
   }

void
IlBuilder::IfCmpEqualZero(TR::IlBuilder *target, TR::IlValue *condition)
   {
   TR_ASSERT(target != NULL, "This IfCmpEqualZero requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpEqualZero(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(condition));
   TraceIL("IlBuilder[ %p ]::IfCmpEqualZero %d == 0? -> [ %p ] B%d\n", this, condition->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpEqualZero(condition, target->getEntry());
   }

void
IlBuilder::IfCmpEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpEqual(*target, left, right);
   }

void
IlBuilder::IfCmpEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR_ASSERT(target != NULL, "This IfCmpEqual requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpEqual %d == %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpEQ, false, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpLessThan(*target, left, right);
   }

void
IlBuilder::IfCmpLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR_ASSERT(target != NULL, "This IfCmpLessThan requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpLessThan(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpLessThan %d < %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpLT, false, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpUnsignedLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpUnsignedLessThan(*target, left, right);
   }

void
IlBuilder::IfCmpUnsignedLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR_ASSERT(target != NULL, "This IfCmpUnsignedLessThan requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpUnsignedLessThan(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpUnsignedLessThan %d < %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpLT, true, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpLessOrEqual(*target, left, right);
   }

void
IlBuilder::IfCmpLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR_ASSERT(target != NULL, "This IfCmpLessOrEqual requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpLessOrEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpLessOrEqual %d <= %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpLE, false, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpUnsignedLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpUnsignedLessOrEqual(*target, left, right);
   }

void
IlBuilder::IfCmpUnsignedLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   TR_ASSERT(target != NULL, "This IfCmpUnsignedLessOrEqual requires a non-NULL builder object");
   ILB_REPLAY("%s->IfCmpUnsignedLessOrEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpUnsignedLessOrEqual %d <= %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpLE, true, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpGreaterThan(*target, left, right);
   }

void
IlBuilder::IfCmpGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   ILB_REPLAY("%s->IfCmpGreaterThan(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpGreaterThan %d > %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpGT, false, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpUnsignedGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpUnsignedGreaterThan(*target, left, right);
   }

void
IlBuilder::IfCmpUnsignedGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   ILB_REPLAY("%s->IfCmpUnsignedGreaterThan(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpUnsignedGreaterThan %d > %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpGT, true, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpGreaterOrEqual(*target, left, right);
   }

void
IlBuilder::IfCmpGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   ILB_REPLAY("%s->IfCmpGreaterOrEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpGreaterOrEqual %d >= %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpGE, false, left, right, target->getEntry());
   }

void
IlBuilder::IfCmpUnsignedGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   IfCmpUnsignedGreaterOrEqual(*target, left, right);
   }

void
IlBuilder::IfCmpUnsignedGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   ILB_REPLAY("%s->IfCmpUnsignedGreaterOrEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpUnsignedGreaterOrEqual %d >= %d? -> [ %p ] B%d\n", this, left->getCPIndex(), right->getCPIndex(), target, target->getEntry()->getNumber());
   ifCmpCondition(TR_cmpGE, true, left, right, target->getEntry());
   }

void
IlBuilder::ifCmpCondition(TR_ComparisonTypes ct, bool isUnsignedCmp, TR::IlValue *left, TR::IlValue *right, TR::Block *target)
   {
   integerizeAddresses(&left, &right);
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCode cmpOpCode(TR::ILOpCode::compareOpCode(leftNode->getDataType(), ct, isUnsignedCmp));
   ifjump(cmpOpCode.convertCmpToIfCmp(),
          leftNode,
          rightNode,
          target);
   appendBlock();
   }

void
IlBuilder::ifCmpNotEqualZero(TR::IlValue *condition, TR::Block *target)
   {
   ifCmpCondition(TR_cmpNE, false, condition, zeroForValue(condition), target);
   }

void
IlBuilder::ifCmpEqualZero(TR::IlValue *condition, TR::Block *target)
   {
   ifCmpCondition(TR_cmpEQ, false, condition, zeroForValue(condition), target);
   }

void
IlBuilder::appendGoto(TR::Block *destBlock)
   {
   appendBlock();
   gotoBlock(destBlock);
   }

/* Flexible builder for if...then...else structures
 * Basically 3 ways to call it:
 *    1) with then and else paths
 *    2) only a then path
 *    3) only an else path
 * #2 and #3 are similar: the existing path is appended and the proper condition causes branch around to the merge point
 * #1: then path exists "out of line", else path falls through from If and then jumps to merge, followed by then path which
 *     falls through to the merge point
 */
void
IlBuilder::IfThenElse(TR::IlBuilder **thenPath, TR::IlBuilder **elsePath, TR::IlValue *condition)
   {
   //ILB_REPLAY_BEGIN();
   TR_ASSERT(thenPath != NULL || elsePath != NULL, "IfThenElse needs at least one conditional path");

   TR::Block *thenEntry = NULL;
   TR::Block *elseEntry = NULL;

   if (thenPath)
      {
      *thenPath = createBuilderIfNeeded(*thenPath);
      thenEntry = (*thenPath)->getEntry();
      }

   if (elsePath)
      {
      *elsePath = createBuilderIfNeeded(*elsePath);
      elseEntry = (*elsePath)->getEntry();
      }

   TR::Block *mergeBlock = emptyBlock();

   TraceIL("IlBuilder[ %p ]::IfThenElse %d", this, condition->getCPIndex());
   if (thenEntry)
      TraceIL(" then B%d", thenEntry->getNumber());
   if (elseEntry)
      TraceIL(" else B%d", elseEntry->getNumber());
   TraceIL(" merge B%d\n", mergeBlock->getNumber());

   if (thenPath == NULL) // case #3
      {
      ifCmpNotEqualZero(condition, mergeBlock);
      if ((*elsePath)->_partOfSequence)
         gotoBlock(elseEntry);
      else
         AppendBuilder(*elsePath);
      }
   else if (elsePath == NULL) // case #2
      {
      ifCmpEqualZero(condition, mergeBlock);
      if ((*thenPath)->_partOfSequence)
         gotoBlock(thenEntry);
      else
         AppendBuilder(*thenPath);
      }
   else // case #1
      {
      ifCmpNotEqualZero(condition, thenEntry);
      if ((*elsePath)->_partOfSequence)
         {
         gotoBlock(elseEntry);
         }
      else
         {
         AppendBuilder(*elsePath);
         appendGoto(mergeBlock);
         }
      if (!(*thenPath)->_partOfSequence)
         AppendBuilder(*thenPath);
      // if then path exists elsewhere already,
      //  then IfCmpNotEqual above already brances to it
      }

   // all paths possibly merge back here
   appendBlock(mergeBlock);
   //ILB_REPLAY_END();
   //ILB_REPLAY("IfThenElse(%s, %s, %s);", REPLAY_PTRTOBUILDER(thenPath), REPLAY_PTRTOBUILDER(elsePath), REPLAY_VALUE(condition));
   }

void
IlBuilder::Switch(const char *selectionVar,
                  TR::IlBuilder **defaultBuilder,
                  uint32_t numCases,
                  int32_t *caseValues,
                  TR::IlBuilder **caseBuilders,
                  bool *caseFallsThrough)
   {
   //ILB_REPLAY_BEGIN();
   TR::IlValue *selectorValue = Load(selectionVar);
   TR_ASSERT(selectorValue->getSymbol()->getDataType() == TR::Int32, "Switch only supports selector having type Int32");
   *defaultBuilder = createBuilderIfNeeded(*defaultBuilder);

   appendBlock();

   TR::Node *defaultNode = TR::Node::createCase(0, (*defaultBuilder)->getEntry()->getEntry());
   TR::Node *lookupNode = TR::Node::create(TR::lookup, numCases + 2, loadValue(selectorValue), defaultNode);

   // get the lookup tree into this builder, even though we haven't completely filled it in yet
   genTreeTop(lookupNode);
   TR::Block *switchBlock = _currentBlock;

   // make sure no fall through edge created from the lookup
   appendNoFallThroughBlock();

   TR::IlBuilder *breakBuilder = OrphanBuilder();

   // each case handler is a sequence of two builder objects: first the one passed in via caseBuilder (or will be passed
   //   back via caseBuilders, and second a builder that branches to the breakBuilder (unless this case falls through)
   for (int32_t c=0;c < numCases;c++)
      {
      int32_t value = caseValues[c];
      TR::IlBuilder *handler = NULL;
      if (!caseFallsThrough[c])
         {
         handler = OrphanBuilder();

         caseBuilders[c] = createBuilderIfNeeded(caseBuilders[c]);
         handler->AppendBuilder(caseBuilders[c]);

         // handle "break" with a separate builder so user can add whatever they want into caseBuilders[c]
         TR::IlBuilder *branchToBreak = OrphanBuilder();
         branchToBreak->Goto(&breakBuilder);
         handler->AppendBuilder(branchToBreak);
         }
      else
         {
         caseBuilders[c] = createBuilderIfNeeded(caseBuilders[c]);
         handler = caseBuilders[c];
         }

      TR::Block *caseBlock = handler->getEntry();
      cfg()->addEdge(switchBlock, caseBlock);
      AppendBuilder(handler);

      TR::Node *caseNode = TR::Node::createCase(0, caseBlock->getEntry(), value);
      lookupNode->setAndIncChild(c+2, caseNode);
      }

   cfg()->addEdge(switchBlock, (*defaultBuilder)->getEntry());
   AppendBuilder(*defaultBuilder);

   AppendBuilder(breakBuilder);
   //ILB_REPLAY_END();
   }

void
IlBuilder::Switch(const char *selectionVar,
                  TR::IlBuilder **defaultBuilder,
                  uint32_t numCases,
                  ...)
   {
   //TODO figure out switch replay
   // ILB_REPLAY("%s->IfCmpGreaterThan(%s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   int32_t *caseValues = (int32_t *) _comp->trMemory()->allocateHeapMemory(numCases * sizeof(int32_t));
   TR_ASSERT(0 != caseValues, "out of memory");

   TR::IlBuilder **caseBuilders = (TR::IlBuilder **) _comp->trMemory()->allocateHeapMemory(numCases * sizeof(TR::IlBuilder *));
   TR_ASSERT(0 != caseBuilders, "out of memory");

   bool *caseFallsThrough = (bool *) _comp->trMemory()->allocateHeapMemory(numCases * sizeof(bool));
   TR_ASSERT(0 != caseFallsThrough, "out of memory");

   va_list cases;
   va_start(cases, numCases);
   for (int32_t c=0;c < numCases;c++)
      {
      caseValues[c] = (int32_t) va_arg(cases, int);
      caseBuilders[c] = *(TR::IlBuilder **) va_arg(cases, TR::IlBuilder **);
      caseFallsThrough[c] = (bool) va_arg(cases, int);
      }
   va_end(cases);

   Switch(selectionVar, defaultBuilder, numCases, caseValues, caseBuilders, caseFallsThrough);

   // if Switch created any new builders, we need to put those back into the arguments passed into this Switch call
   va_start(cases, numCases);
   for (int32_t c=0;c < numCases;c++)
      {
      int throwawayValue = va_arg(cases, int);
      TR::IlBuilder **caseBuilder = va_arg(cases, TR::IlBuilder **);
      (*caseBuilder) = caseBuilders[c];
      int throwAwayFallsThrough = va_arg(cases, int);
      }
   va_end(cases);
   }


void
IlBuilder::ForLoop(bool countsUp,
                   const char *indVar,
                   TR::IlBuilder **loopCode,
                   TR::IlBuilder **breakBuilder,
                   TR::IlBuilder **continueBuilder,
                   TR::IlValue *initial,
                   TR::IlValue *end,
                   TR::IlValue *increment)
   {
   //ILB_REPLAY_BEGIN();
   
   methodSymbol()->setMayHaveLoops(true);
   TR_ASSERT(loopCode != NULL, "ForLoop needs to have loopCode builder");
   *loopCode = createBuilderIfNeeded(*loopCode);

   TraceIL("IlBuilder[ %p ]::ForLoop ind %s initial %d end %d increment %d loopCode %p countsUp %d\n", this, indVar, initial->getCPIndex(), end->getCPIndex(), increment->getCPIndex(), *loopCode, countsUp);

   Store(indVar, initial);

   TR::IlValue *loopCondition;
   TR::IlBuilder *loopBody = OrphanBuilder();
   loopCondition = countsUp ? LessThan(Load(indVar), end) : GreaterThan(Load(indVar), end);
   IfThen(&loopBody, loopCondition);
   loopBody->AppendBuilder(*loopCode);
   TR::IlBuilder *loopContinue = OrphanBuilder();
   loopBody->AppendBuilder(loopContinue);

   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "ForLoop returns breakBuilder, cannot provide breakBuilder as input");
      *breakBuilder = OrphanBuilder();
      AppendBuilder(*breakBuilder);
      }

   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "ForLoop returns continueBuilder, cannot provide continueBuilder as input");
      *continueBuilder = loopContinue;
      }

   if (countsUp)
      {
      loopContinue->Store(indVar,
      loopContinue->   Add(
      loopContinue->      Load(indVar),
                          increment));
      loopContinue->IfCmpLessThan(&loopBody,
      loopContinue->   Load(indVar),
                       end);
      }
   else
      {
      loopContinue->Store(indVar,
      loopContinue->   Sub(
      loopContinue->      Load(indVar),
                          increment));
      loopContinue->IfCmpGreaterThan(&loopBody,
      loopContinue->   Load(indVar),
                       end);
      }
   //ILB_REPLAY_END();
#if 0
   ILB_REPLAY("%s->ForLoopUp(%s, \"%s\", %s, %s, %s, %s, %s, %s, %s);",
          REPLAY_BUILDER(this),
          REPLAY_BOOL(countsUp),
          indVar,
          REPLAY_PTRTOBUILDER(loopCode),
          REPLAY_PTRTOBUILDER(breakBuilder),
          REPLAY_PTRTOBUILDER(continueBuilder),
          REPLAY_VALUE(initial),
          REPLAY_VALUE(end),
          REPLAY_VALUE(increment));
#endif
   }

void
IlBuilder::DoWhileLoop(const char *whileCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder, TR::IlBuilder **continueBuilder)
   {
   //ILB_REPLAY_BEGIN();

   methodSymbol()->setMayHaveLoops(true);
   TR_ASSERT(body != NULL, "doWhileLoop needs to have a body");

   if (!_methodBuilder->symbolDefined(whileCondition))
      _methodBuilder->defineValue(whileCondition, Int32);

   *body = createBuilderIfNeeded(*body);
   TraceIL("IlBuilder[ %p ]::DoWhileLoop do body B%d while %s\n", this, (*body)->getEntry()->getNumber(), whileCondition);

   AppendBuilder(*body);
   TR::IlBuilder *loopContinue = NULL;

   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "DoWhileLoop returns continueBuilder, cannot provide continueBuilder as input");
      loopContinue = *continueBuilder = OrphanBuilder();
      }
   else
      loopContinue = OrphanBuilder();

   AppendBuilder(loopContinue);
   loopContinue->IfCmpNotEqualZero(body,
   loopContinue->   Load(whileCondition));

   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "DoWhileLoop returns breakBuilder, cannot provide breakBuilder as input");
      *breakBuilder = OrphanBuilder();
      AppendBuilder(*breakBuilder);
      }

   //ILB_REPLAY_END();
   //ILB_REPLAY("%s->DoWhileLoop(%s, %s, %s, %s);",
          //REPLAY_BUILDER(this),
          //whileCondition,
          //REPLAY_PTRTOBUILDER(body),
          //REPLAY_PTRTOBUILDER(breakBuilder),
          //REPLAY_PTRTOBUILDER(continueBuilder));
   }

void
IlBuilder::WhileDoLoop(const char *whileCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder, TR::IlBuilder **continueBuilder)
   {
   //ILB_REPLAY_BEGIN();

   methodSymbol()->setMayHaveLoops(true);
   TR_ASSERT(body != NULL, "WhileDo needs to have a body");
   TraceIL("IlBuilder[ %p ]::WhileDoLoop while %s do body %p\n", this, whileCondition, *body);

   TR::IlBuilder *done = OrphanBuilder();
   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "WhileDoLoop returns breakBuilder, cannot provide breakBuilder as input");
      *breakBuilder = done;
      }

   TR::IlBuilder *loopContinue = OrphanBuilder();
   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "WhileDoLoop returns continueBuilder, cannot provide continueBuilder as input");
      *continueBuilder = loopContinue;
      }

   AppendBuilder(loopContinue);
   loopContinue->IfCmpEqualZero(&done,
   loopContinue->   Load(whileCondition));

   *body = createBuilderIfNeeded(*body);
   AppendBuilder(*body);

   Goto(&loopContinue);
   setComesBack(); // this goto is on one particular flow path, doesn't mean every path does a goto

   AppendBuilder(done);

   //ILB_REPLAY_END();
   //ILB_REPLAY("%s->WhileDoLoop(%s, %s, %s, %s);",
          //REPLAY_BUILDER(this),
          //whileCondition,
          //REPLAY_PTRTOBUILDER(body),
          //REPLAY_PTRTOBUILDER(breakBuilder),
          //REPLAY_PTRTOBUILDER(continueBuilder));
   }

} // namespace OMR
