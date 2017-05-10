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

#ifndef OMR_Z_CODEGENERATOR_INCL
#define OMR_Z_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace Z { class CodeGenerator; } }
namespace OMR { typedef OMR::Z::CodeGenerator CodeGeneratorConnector; }
#else
#error OMR::Z::CodeGenerator expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include <stddef.h>                                 // for size_t, NULL
#include <stdint.h>                                 // for int32_t, etc
#include <string.h>                                 // for memcmp
#include "codegen/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"                   // for InstOpCode, etc
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/Machine.hpp"                      // for Machine, etc
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"            // for RecognizedMethod
#include "codegen/RegisterConstants.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "codegen/Snippet.hpp"                      // for Snippet
#include "codegen/TreeEvaluator.hpp"                // for TreeEvaluator
#include "compile/Compilation.hpp"                  // for Compilation, etc
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"              // for TR::Options, etc
#include "cs2/arrayof.h"                            // for ArrayOf, etc
#include "cs2/hashtab.h"                            // for HashTable, etc
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"                              // for Cpu
#include "env/jittypes.h"                           // for uintptrj_t
#include "env/PersistentInfo.hpp"                   // for PersistentInfo
#include "env/Processors.hpp"                       // for TR_Processor
#include "env/TRMemory.hpp"                         // for Allocator, etc
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for DataTypes, etc
#include "il/ILOpCodes.hpp"                         // for ILOpCodes, etc
#include "il/ILOps.hpp"                             // for TR::ILOpCode, etc
#include "il/Node.hpp"                              // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"                   // for TreeTop::getNode
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"                          // for TR_Array
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector
#include "infra/Flags.hpp"                          // for flags32_t
#include "infra/List.hpp"                           // for List
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase
#include "runtime/Runtime.hpp"
#include "env/IO.hpp"


#include "il/symbol/LabelSymbol.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/Relocation.hpp"

#include "infra/TRlist.hpp"
class TR_BackingStore;
class TR_GCStackMap;
class TR_OpaquePseudoRegister;
class TR_PseudoRegister;
class TR_RegisterCandidate;
namespace TR { class S390ConstantDataSnippet; }
namespace TR { class S390ConstantInstructionSnippet; }
namespace TR { class S390DeclTrampSnippet; }
namespace TR { class S390EyeCatcherDataSnippet; }
namespace TR { class S390ImmInstruction; }
namespace TR { class S390LabelTableSnippet; }
namespace TR { class S390LookupSwitchSnippet; }
class TR_S390OutOfLineCodeSection;
namespace TR { class S390PrivateLinkage; }
class TR_S390ScratchRegisterManager;
namespace TR { class S390SortJumpTrampSnippet; }
namespace TR { class S390TargetAddressSnippet; }
namespace TR { class S390WritableDataSnippet; }
class TR_StorageReference;
namespace OMR { class Linkage; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Optimizer; }
namespace TR { class RegStarRef; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterIterator; }
namespace TR { class RegisterPair; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class SystemLinkage; }

extern int64_t getIntegralValue(TR::Node* node);

#if defined(TR_TARGET_64BIT)
#define UPPER_4_BYTES(x) ((x) >> 32)
#define LOWER_4_BYTES(x) ((x) & 0xffffffff)
#else
// fake it to allow warning-free compilation on 32 bit systems
#define UPPER_4_BYTES(x) (x)
#define LOWER_4_BYTES(x) (x)
#endif

// Multi Code Cache Routines for checking whether an entry point is within reach of a BASRL.
#define NEEDS_TRAMPOLINE(target, rip, cg) (cg->alwaysUseTrampolines() || !CHECK_32BIT_TRAMPOLINE_RANGE(target,rip))

#define TR_MAX_MVO_PRECISION 31
#define TR_MAX_MVO_SIZE 16
#define TR_MAX_SRP_SIZE 16
#define TR_MAX_SRP_PRECISION 31
#define TR_MAX_SRP_SHIFT 31
#define TR_MAX_MVC_SIZE 256
#define TR_MAX_OC_NC_XC_SIZE 256
#define TR_MAX_CLC_SIZE 256
#define TR_MAX_SS1_SIZE 256
#define TR_MIN_SS_DISP 0
#define TR_MAX_SS_DISP 4095
#define TR_MIN_RX_DISP 0
#define TR_MAX_RX_DISP 4095

#define TR_MEMCPY_PAD_DST_LEN_INDEX 1
#define TR_MEMCPY_PAD_SRC_LEN_INDEX 3
#define TR_MEMCPY_PAD_MAX_LEN_INDEX 5
#define TR_MEMCPY_PAD_EQU_LEN_INDEX 6
#define MVCL_THRESHOLD 16777216

// functional thresholds
#define TR_MAX_FROM_TO_LOOP_STRING_SIZE (TR_MAX_SS1_SIZE)
#define TR_MAX_FROM_TO_TABLE_STRING_SIZE 1      // i.e. only TR one byte to one byte translations supported
#define TR_MAX_SIMD_LOOP_OPERAND_STRING_SIZE 8
#define TR_MAX_NUM_TALLY_TRIPLES_BY_LOOP 2
#define TR_MAX_NUM_TALLY_TRIPLES_BY_SIMD_LOOP 1
#define TR_MAX_NUM_TRANSLATE_QUADS_BY_LOOP 1    // note: for isInspectConvertingOp operations the length of the from/to strings is the # of 'all' replaces
#define TR_MAX_NUM_TRANSLATE_QUADS_BY_SIMD_LOOP 1
#define TR_MAX_SEARCH_STRING_SIZE (TR_MAX_SS1_SIZE)
#define TR_MAX_REPLACE_CHAR_STRING_SIZE 1
#define TR_MAX_BEFORE_AFTER_SIZE 1


// heuristic codegen register pressure threshold (restricted registers)
#define CODEGEN_REGPRESSURE_THRESHOLD 14

// performance thresholds -- until SRST and similar instructions are used inline
#define TR_MAX_REPLACE_ALL_LOOP_PERF 150              // search > 1 byte (otherwise it is table lookup)
#define TR_MAX_TALLY_ALL_WIDE_LOOP_PERF 150           // search > 1 byte
#define TR_MIN_BM_SEARCH_PATTERN_SIZE 4
#define TR_MIN_BM_SEARCH_TEXT_SIZE 16

#define USE_CURRENT_DFP_ROUNDING_MODE (uint8_t)0x0


enum TR_MemCpyPadTypes
   {
   OneByte,
   TwoByte,
   ND_OneByte,
   ND_TwoByte,
   InvalidType
   };

#define TR_INVALID_REGISTER -1

class TR_S390ProcessorInfo
   {
   public:
   enum TR_S390ProcessorArchitectures
      {
      TR_UnknownArchitecture          = 0,
      TR_ESA390                       = 1,
      TR_z900                         = 2,
      TR_z990                         = 3,
      TR_z9                           = 4, ///< arch7
      TR_z10                          = 5, ///< arch8
      TR_z196                         = 6, ///< arch9
      TR_zEC12                        = 7, ///< arch10
      TR_z13                          = 8, ///< arch11
      TR_zNext                        = 9, ///< arch12

      TR_LatestArchitecture           = TR_zNext
      };

   bool crossCompile()                   {return _crossCompile; }

   bool supportsArch(TR_S390ProcessorArchitectures arch)
      {
      TR_ASSERT(arch >= TR_UnknownArchitecture && arch <= TR_LatestArchitecture, "Invalid Processor Architecture.");
      return _processorArchitecture >= arch;
      }

   void disableArch(TR_S390ProcessorArchitectures arch)
      {
      TR_ASSERT(arch > TR_UnknownArchitecture && arch <= TR_LatestArchitecture, "Invalid Processor Architecture.");
      _processorArchitecture = _processorArchitecture < arch ? _processorArchitecture : (TR_S390ProcessorArchitectures)((uint32_t)arch - 1);
      }

   void enableArch(TR_S390ProcessorArchitectures arch)
      {
      TR_ASSERT(arch >= TR_UnknownArchitecture && arch <= TR_LatestArchitecture, "Invalid Processor Architecture.");
      _processorArchitecture = _processorArchitecture > arch ? _processorArchitecture : arch;
      }

   TR_Processor getProcessor();

   private:
   TR_S390ProcessorArchitectures _processorArchitecture;

   bool _crossCompile;

   friend class OMR::Z::CodeGenerator;

   TR_S390ProcessorInfo()
      :
        _processorArchitecture(TR_UnknownArchitecture),
        _crossCompile(false)
      {
#ifndef TR_HOST_S390
      _crossCompile = true;
#endif
      initialize();
      }

   bool checkz900();
   bool checkz10();
   bool checkz990();
   bool checkz9();
   bool checkz196();
   bool checkzEC12();
   bool checkZ13();
   bool checkZNext();

   void initialize()
      {
      if(checkZNext())
         _processorArchitecture = TR_zNext;
      else if (checkZ13())
         _processorArchitecture = TR_z13;
      else if (checkzEC12())
         _processorArchitecture = TR_zEC12;
      else if (checkz196())
         _processorArchitecture = TR_z196;
      else if (checkz10())
         _processorArchitecture = TR_z10;
      else if (checkz9())
         _processorArchitecture = TR_z9;
      else if (checkz990())
         _processorArchitecture = TR_z990;
      else if (checkz900())
         _processorArchitecture = TR_z900;
      }
   };


struct TR_S390BinaryEncodingData : public TR_BinaryEncodingData
   {
   int32_t estimate;
   TR::Instruction *cursorInstruction;
   TR::Instruction *jitTojitStart;
   TR::Instruction *preProcInstruction;
   int32_t loadArgSize;
   };


namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

public:

   void preLowerTrees();

   struct TR_BranchPreloadCallData
      {
      TR_ALLOC(TR_Memory::BranchPreloadCallData)
      TR::Instruction *_callInstr;
      TR::LabelSymbol *_callLabel;
      TR::Block       *_callBlock;
      TR::SymbolReference * _callSymRef;
      int32_t        _frequency;

      TR_BranchPreloadCallData() { _frequency = -1;}
      TR_BranchPreloadCallData(TR::Instruction *i,TR::LabelSymbol *l,TR::Block *b,TR::SymbolReference *s, int32_t f)
      :_callInstr(i),_callLabel(l),_callBlock(b),_callSymRef(s),_frequency(f){}
      };

   struct TR_BranchPreloadData
      {
      TR::Instruction *_returnInstr;
      TR::LabelSymbol *_returnLabel;
      TR::Block       *_returnBlock;
      int32_t        _frequency;
      bool           _insertBPPInEpilogue;

      TR_BranchPreloadData() { _frequency = -1; _insertBPPInEpilogue = false; }
      };

   TR_BranchPreloadData _hottestReturn;
   TR_BranchPreloadCallData _outlineCall;
   TR_BranchPreloadCallData _outlineArrayCall;

   TR::list<TR_BranchPreloadCallData*> *_callsForPreloadList;

   TR::list<TR_BranchPreloadCallData*> * getCallsForPreloadList() { return _callsForPreloadList; }
   void createBranchPreloadCallData(TR::LabelSymbol *callLabel, TR::SymbolReference * callSymRef, TR::Instruction * instr);

   void insertInstructionPrefetches();
   void insertInstructionPrefetchesForCalls(TR_BranchPreloadCallData * data);
   bool hasWarmCallsBeforeReturn();

   /**
    * \brief Tells the optimzers and codegen whether a load constant node should be rematerialized.
    *
    * \details Large constants should be materialized (constant node should be commoned up)
    * because loading them as immediate values is expensive.
    *
    * A constant is large if it's outside of the range specified by the largest negative
    * const and smallest positive const. And the ranges are hardware platform dependent.
    */
   bool materializesLargeConstants() { return true; }
   bool shouldValueBeInACommonedNode(int64_t value);
   int64_t getLargestNegConstThatMustBeMaterialized() {return ((-1ll) << 31) - 1;}   // min 32bit signed integer minus 1
   int64_t getSmallestPosConstThatMustBeMaterialized() {return ((int64_t)0x000000007FFFFFFF) + 1;}   // max 32bit signed integer plus 1

   void setNodeAddressOfCachedStaticTree(TR::Node *n) { _nodeAddressOfCachedStatic=n; }
   TR::Node *getNodeAddressOfCachedStatic() { return _nodeAddressOfCachedStatic; }

   bool supportsNamedVirtualRegisters(); // no virt, default
   TR::SparseBitVector & getBucketPlusIndexRegisters()  { return _bucketPlusIndexRegisters; }

   // For hanging multiple loads from register symbols onto one common DEPEND
   TR::Instruction *getCurrentDEPEND() {return _currentDEPEND; }
   void setCurrentDEPEND(TR::Instruction *instr) { _currentDEPEND=instr; }

   uintptr_t getOutgoingArgLevelDuringTreeEvaluation() { return _outgoingArgLevelDuringTreeEvaluation; }
   void changeRegisterKind(TR::Register * temp, TR_RegisterKinds rk);


   TR::SymbolReference * _ARSaveAreaForTM;
   void setARSaveAreaForTM(TR::SymbolReference * symRef) {_ARSaveAreaForTM = symRef;}
   TR::SymbolReference * getARSaveAreaForTM() {return _ARSaveAreaForTM;}

   void beginInstructionSelection();
   void endInstructionSelection();

   void checkIsUnneededIALoad(TR::Node *parent, TR::Node* node, TR::TreeTop *tt);
   void lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);


   void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   void lowerTreesPropagateBlockToNode(TR::Node *node);

   typedef enum
      {
      Backward = 0,
      Forward  = 1
      } RegisterAssignmentDirection;

   CodeGenerator();
   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   bool anyNonConstantSnippets();
   bool anyLitPoolSnippets();

   bool getSupportsEncodeUtf16BigWithSurrogateTest();

   TR_S390ScratchRegisterManager* generateScratchRegisterManager(int32_t capacity = 8);

   bool canTransformUnsafeCopyToArrayCopy();
   bool supportsInliningOfIsInstance();
   bool supports32bitAiadd() {return TR::Compiler->target.is64Bit();}

   void setLabelHashTable(TR_HashTab *notPrintLabelHashTab) {_notPrintLabelHashTab = notPrintLabelHashTab;}
   TR_HashTab * getLabelHashTable() {return _notPrintLabelHashTab;}

   void addPICsListForInterfaceSnippet(TR::S390ConstantDataSnippet * ifcSnippet, TR::list<TR_OpaqueClassBlock*> * PICSlist);
   TR::list<TR_OpaqueClassBlock*> * getPICsListForInterfaceSnippet(TR::S390ConstantDataSnippet * ifcSnippet);

   void doInstructionSelection();
   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);

   bool loadOrStoreAddressesMatch(TR::Node *node1, TR::Node *node2);

   bool reliesOnAParticularSignEncoding(TR::Node *node);

   void recordRegisterAssignment(TR::Register *assignedReg, TR::Register *virtualReg);

   void doBinaryEncoding();
   void doPeephole();
   void setUseDefRegisters(bool resetRegs);

   void AddFoldedMemRefToStack(TR::MemoryReference * mr);
   void RemoveMemRefFromStack(TR::MemoryReference * mr);
   void StopUsingEscapedMemRefsRegisters(int32_t topOfMemRefStackBeforeEvaluation);

   bool supportsMergingGuards();

   bool supportsDirectJNICallsForAOT() { return true;}

   bool shouldYankCompressedRefs() { return true; }

   TR::RegisterIterator *getARegisterIterator()                             {return  _aRegisterIterator;          }
   TR::RegisterIterator *setARegisterIterator(TR::RegisterIterator *iter)    {return _aRegisterIterator = iter;  }

   TR::RegisterIterator *getHPRegisterIterator()                            {return  _hpRegisterIterator;         }
   TR::RegisterIterator *setHPRegisterIterator(TR::RegisterIterator *iter)   {return _hpRegisterIterator = iter; }

   bool supportsJITFreeSystemStackPointer() { return false; }
   TR::RegisterIterator *getVRFRegisterIterator()                           {return  _vrfRegisterIterator;        }
   TR::RegisterIterator *setVRFRegisterIterator(TR::RegisterIterator *iter)  {return _vrfRegisterIterator = iter;}

   bool supportsLengthMinusOneForMemoryOpts() {return true;}

   bool supportsTrapsInTMRegion()
      {
      if(!TR::Compiler->target.isZOS())
         return false;
      return true;
      }

   bool inlineNDmemcpyWithPad(TR::Node * node, int64_t * maxLengthPtr = NULL);
   bool codegenSupportsLoadlessBNDCheck() {return _processorInfo.supportsArch(TR_S390ProcessorInfo::TR_zEC12);}
   TR::Register *evaluateLengthMinusOneForMemoryOps(TR::Node *,  bool , bool &lenMinusOne);

   virtual TR_GlobalRegisterNumber getGlobalRegisterNumber(uint32_t realRegNum);

   RegisterAssignmentDirection getAssignmentDirection() {return assignmentDirection;}
   RegisterAssignmentDirection setAssignmentDirection(RegisterAssignmentDirection d)
      {
      return assignmentDirection = d;
      }

   TR::RegisterPair* allocateArGprPair(TR::Register* lowRegister, TR::Register* highRegister);
   void splitBaseRegisterPairsForRRMemoryInstructions(TR::Node *node, TR::RegisterPair * sourceReg, TR::RegisterPair * targetReg);
   TR::RegisterDependencyConditions* createDepsForRRMemoryInstructions(TR::Node *node, TR::RegisterPair * sourceReg, TR::RegisterPair * targetReg, uint8_t extraDeps=0);

   // Allocate a pair with inherent consecutive association
   virtual TR::RegisterPair* allocateConsecutiveRegisterPair();
   virtual TR::RegisterPair* allocateConsecutiveRegisterPair(TR::Register* lowRegister, TR::Register* highRegister);
   virtual TR::RegisterPair* allocateFPRegisterPair();
   virtual TR::RegisterPair* allocateFPRegisterPair(TR::Register* lowRegister, TR::Register* highRegister);

   void processUnusedNodeDuringEvaluation(TR::Node *node);

   void apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *, bool isCheckDisp = true);
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *, int8_t addressDifferenceDivisor, bool isInstrOffset = false);
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   void apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   void apply32BitLabelTableRelocation(int32_t * cursor, TR::LabelSymbol *);

   void setUnavailableRegisters(TR::Block *b, TR_BitVector &unavailableRegisters);

   void resetGlobalRegister(TR::RealRegister::RegNum reg, TR_BitVector &globalRegisters);
   void setGlobalRegister(TR::RealRegister::RegNum reg, TR_BitVector &globalRegisters);
   void setRegister(TR::RealRegister::RegNum reg, TR_BitVector &registers);
   void removeUnavailableRegisters(TR_RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters);
   void setUnavailableRegistersUsage(TR_Array<TR_BitVector>  &_liveOnEntryUsage, TR_Array<TR_BitVector>   &_liveOnExitUsage);

   void genMemCpy(TR::MemoryReference *targetMR, TR::Node *targetNode, TR::MemoryReference *sourceMR, TR::Node *sourceNode, int64_t copySize);
   void genMemClear(TR::MemoryReference *targetMR, TR::Node *targetNode, int64_t clearSize);

   void genCopyFromLiteralPool(TR::Node *node, int32_t bytesToCopy, TR::MemoryReference *targetMR, size_t litPoolOffset, TR::InstOpCode::Mnemonic op = TR::InstOpCode::MVC);
   int32_t biasDecimalFloatFrac(TR::DataType dt, int32_t frac);


   bool isMemcpyWithPadIfFoldable(TR::Node *node, TR_MemCpyPadTypes type);
   bool useMVCLForMemcpyWithPad(TR::Node *node, TR_MemCpyPadTypes type);
   bool isValidCompareConst(int64_t compareConst);
   bool isIfFoldable(TR::Node *node, int64_t compareConst);

   bool mayImmedInstructionCauseOverFlow(TR::Node * node);

   TR::Instruction *genLoadAddressToRegister(TR::Register *reg, TR::MemoryReference *origMR, TR::Node *node, TR::Instruction *preced=NULL);


   bool useRippleCopyOrXC(size_t size, char *lit);

   bool isCompressedClassPointerOfObjectHeader(TR::Node * node);



   bool usesImplicit64BitGPRs(TR::Node *node);
   bool nodeMayCauseException(TR::Node *node);

   bool nodeRequiresATemporary(TR::Node *node);
   bool endHintOnOperation(TR::Node *node);
   bool endAccumulatorSearchOnOperation(TR::Node *node);
   bool isStorageReferenceType(TR::Node *node);

   TR::Register *allocateClobberableRegister(TR::Register *srcRegister);
   /** Different from evaluateNode in that it returns a clobberable register */
   TR::Register *gprClobberEvaluate(TR::Node *node, bool force_copy=false, bool ignoreRefCount = false);
   TR::Register *fprClobberEvaluate(TR::Node *node);
   TR_OpaquePseudoRegister *ssrClobberEvaluate(TR::Node *node, TR::MemoryReference *sourceMR);

//Convenience accessor methods
   TR::Linkage *getS390Linkage();
   TR::S390PrivateLinkage *getS390PrivateLinkage();
   TR::SystemLinkage * getS390SystemLinkage();

   TR::RealRegister *getStackPointerRealRegister(TR::Symbol *symbol = NULL);
   TR::RealRegister *getEntryPointRealRegister();
   TR::RealRegister *getReturnAddressRealRegister();

   TR::RealRegister::RegNum getEntryPointRegister();
   TR::RealRegister::RegNum getReturnAddressRegister();

   TR::RealRegister *getExtCodeBaseRealRegister();
   TR::RealRegister *getMethodMetaDataRealRegister();
   TR::RealRegister *getLitPoolRealRegister();


   void buildRegisterMapForInstruction(TR_GCStackMap *map);

   // BCDCHK node
   TR::Node * _currentCheckNode;
   void setCurrentCheckNodeBeingEvaluated(TR::Node * n) { _currentCheckNode = n; }
   TR::Node * getCurrentCheckNodeBeingEvaluated(){ return _currentCheckNode; }

   // BCDCHK node exception handler label
   TR::LabelSymbol* _currentBCDCHKHandlerLabel;
   void setCurrentBCDCHKHandlerLabel(TR::LabelSymbol * l) { _currentBCDCHKHandlerLabel = l; }
   TR::LabelSymbol* getCurrentBCDCHKHandlerLabel(){ return _currentBCDCHKHandlerLabel; }

   TR::RegisterDependencyConditions * _currentBCDRegDeps;
   void setCurrentCheckNodeRegDeps(TR::RegisterDependencyConditions * daaDeps) {_currentBCDRegDeps = daaDeps;}
   TR::RegisterDependencyConditions * getCurrentCheckNodeRegDeps() {return _currentBCDRegDeps;}

   // TODO : Do we need these? And if so there has to be a better way of tracking this this than what we're doing here.
   /** Active counter used to track control flow basic blocks generated at instruction selection. */
   int32_t _nextAvailableBlockIndex;
   /** The index of the current basic block (used and updated during instruction selection). */
   int32_t _currentBlockIndex;

   void setNextAvailableBlockIndex(int32_t blockIndex) { _nextAvailableBlockIndex = blockIndex; }
   int32_t getNextAvailableBlockIndex(){ return _nextAvailableBlockIndex; }
   void incNextAvailableBlockIndex() { _nextAvailableBlockIndex++; }

   void setCurrentBlockIndex(int32_t blockIndex) { _currentBlockIndex = blockIndex; }
   int32_t getCurrentBlockIndex() { return _currentBlockIndex; }
   int32_t arrayInitMinimumNumberOfBytes() {return 16;}

   bool isStackBased(TR::MemoryReference *mr);

   TR::Register* copyRestrictedVirtual(TR::Register * virtReg, TR::Node *node, TR::Instruction ** preced=NULL);

   bool directLoadAddressMatch(TR::Node *load1, TR::Node *load2, bool trace);
   bool isOutOf32BitPositiveRange(int64_t value, bool trace);
   int32_t getMaskSize(int32_t leftMostNibble, int32_t nibbleCount);

   void setAccumulatorNodeUsage(int32_t n) { _accumulatorNodeUsage = n; }
   int32_t getAccumulatorNodeUsage()       { return _accumulatorNodeUsage; }
   void incAccumulatorNodeUsage()          { _accumulatorNodeUsage++; }

   bool possiblyConflictingNode(TR::Node *node);
   bool foundConflictingNode(TR::Node *node, TR::list<TR::Node*> *conflictingAddressNodes);
   void collectConflictingAddressNodes(TR::Node *parent, TR::Node *node, TR::list<TR::Node*> *conflictingAddressNodes);


   template <class TR_AliasSetInterface> bool loadAndStoreMayOverlap(TR::Node *store, size_t storeSize, TR::Node *load, size_t loadSize, TR_AliasSetInterface &storeAliases);
   bool loadAndStoreMayOverlap(TR::Node *store, size_t storeSize, TR::Node *load, size_t loadSize);

   bool checkIfcmpxx(TR::Node *node);
   bool checkSimpleLoadStore(TR::Node *loadNode, TR::Node *storeNode, TR::Block *block);
   bool checkBitWiseChild(TR::Node *node);

   TR_StorageDestructiveOverlapInfo getStorageDestructiveOverlapInfo(TR::Node *src, size_t srcLength, TR::Node *dst, size_t dstLength);

   void setBranchOnCountFlag(TR::Node *node, vcount_t visitCount);

   bool needs64bitPrecision(TR::Node *node);

   virtual bool isUsing32BitEvaluator(TR::Node *node);
   int32_t getEstimatedExtentOfLitLoop()  {return _extentOfLitPool;}

   int64_t setAvailableHPRSpillMask(int64_t i)  {return _availableHPRSpillMask = i;}
   int64_t maskAvailableHPRSpillMask(int64_t i) {return _availableHPRSpillMask &= i;}
   int64_t getAvailableHPRSpillMask()           {return _availableHPRSpillMask;}
   int32_t getPreprologueOffset()               { return _preprologueOffset; }
   int32_t setPreprologueOffset(int32_t offset) { return _preprologueOffset = offset; }

   bool supportsBranchPreload()          {return _cgFlags.testAny(S390CG_enableBranchPreload);}
   void setEnableBranchPreload()          {_cgFlags.set(S390CG_enableBranchPreload);}
   void setDisableBranchPreload()          {_cgFlags.reset(S390CG_enableBranchPreload);}

   bool supportsBranchPreloadForCalls()          {return _cgFlags.testAny(S390CG_enableBranchPreloadForCalls);}
   void setEnableBranchPreloadForCalls()          {_cgFlags.set(S390CG_enableBranchPreloadForCalls);}
   void setDisableBranchPreloadForCalls()          {_cgFlags.reset(S390CG_enableBranchPreloadForCalls);}

   bool getExtCodeBaseRegisterIsFree()         {return _cgFlags.testAny(S390CG_extCodeBaseRegisterIsFree);}
   void setExtCodeBaseRegisterIsFree(bool val) {return _cgFlags.set(S390CG_extCodeBaseRegisterIsFree, val);}

   bool isOutOfLineHotPath() {return _cgFlags.testAny(S390CG_isOutOfLineHotPath);}
   void setIsOutOfLineHotPath(bool val) {_cgFlags.set(S390CG_isOutOfLineHotPath, val);}

   bool getExitPointsInMethod() {return _cgFlags.testAny(S390CG_doesExit);}
   void setExitPointsInMethod(bool val) {return _cgFlags.set(S390CG_doesExit, val);}

   bool isPrefetchNextStackCacheLine() {return _cgFlags.testAny(S390CG_prefetchNextStackCacheLine);}
   void setPrefetchNextStackCacheLine(bool val) {return _cgFlags.set(S390CG_prefetchNextStackCacheLine, val);}

   // Heap object prefetching
   bool enableTLHPrefetching()    {return _cgFlags.testAny(S390CG_enableTLHPrefetching);}
   void setEnableTLHPrefetching() { _cgFlags.set(S390CG_enableTLHPrefetching);}

   // Query to codegen to know if regs are available or not
   //
   bool isExtCodeBaseFreeForAssignment();
   bool isLitPoolFreeForAssignment();

   // zGryphon HPR
   TR::Instruction * upgradeToHPRInstruction(TR::Instruction * inst);
   // REG ASSOCIATION
   //
   bool enableRegisterAssociations();
   bool enableRegisterPairAssociation();

   bool canBeAffectedByStoreTagStalls() { return true; }

   // Local RA/GRA
   TR_RegisterKinds prepareRegistersForAssignment();

   // GRA
   //
   bool prepareForGRA();
   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type);

   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];
   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return &_globalRegisterBitVectors[kind]; }
   virtual void simulateNodeEvaluation(TR::Node * node, TR_RegisterPressureState * state, TR_RegisterPressureSummary * summary);

   TR_BitVector *getGlobalGPRsPreservedAcrossCalls(){ return &_globalGPRsPreservedAcrossCalls; }
   TR_BitVector *getGlobalFPRsPreservedAcrossCalls(){ return &_globalFPRsPreservedAcrossCalls; }

   TR_GlobalRegisterNumber getGlobalHPRFromGPR (TR_GlobalRegisterNumber n);
   TR_GlobalRegisterNumber getGlobalGPRFromHPR (TR_GlobalRegisterNumber n);

   bool considerTypeForGRA(TR::Node *node);
   bool considerTypeForGRA(TR::DataType dt);
   bool considerTypeForGRA(TR::SymbolReference *symRef);

   // Number of assignable GPRs
   int32_t getMaximumNumberOfAssignableGPRs();

   using OMR::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge;
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfVRFsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Block *);

   int32_t getMaximumNumbersOfAssignableGPRs();
   int32_t getMaximumNumbersOfAssignableFPRs();
   int32_t getMaximumNumbersOfAssignableVRs();
   bool allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *, TR::Node *);
   void setRealRegisterAssociation(TR::Register     *reg,
                                   TR::RealRegister::RegNum realNum);
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt);
   void registerSymbolSetup();

   // Used to model register liveness without Future Use Count.
   virtual bool isInternalControlFlowReg(TR::Register *reg);
   virtual void startInternalControlFlow(TR::Instruction *instr);
   virtual void endInternalControlFlow(TR::Instruction *instr) { _internalControlFlowRegisters.clear(); }

   void setEvaluateNodeInRegPair(TR::Node * node, bool b = true)
   {
   if( !(node->getOpCodeValue()== TR::lload || node->getOpCodeValue() == TR::lloadi || node->getOpCodeValue() == TR::lconst))
      return;

   CS2::HashIndex hashIndex = 0;
   if (_nodesToBeEvaluatedInRegPairs.Locate(node->getGlobalIndex(), hashIndex))
      {
      _nodesToBeEvaluatedInRegPairs.SetDataAt(hashIndex,b);
      }
   else
      {

      _nodesToBeEvaluatedInRegPairs.Add(node->getGlobalIndex(), b);
      }
   }

   bool evaluateNodeInRegPair(TR::Node * node)
   {
   CS2::HashIndex hashIndex = 0;
   if (!_nodesToBeEvaluatedInRegPairs.Locate(node->getGlobalIndex(), hashIndex))
      return false;
   else
      return _nodesToBeEvaluatedInRegPairs.DataAt(hashIndex);
   }

   bool doRematerialization() {return true;}

   /**
    * The number of nodes we want between a monexit and the next monent before transforming a monitored region with
    * transactional lock elision.  On Z, we require 25-30 cycles between transactions, or else the latter transaction will
    * be aborted (with significant penalty).  The 45 is an estimate based on CPI of 1.5-2, and average of 1 instruction per node.
    */
   int32_t getMinimumNumberOfNodesBetweenMonitorsForTLE() { return 45; }

   void dumpDataSnippets(TR::FILE *outFile);
   void dumpTargetAddressSnippets(TR::FILE *outFile);

   bool specializedEpilogues() { return _cgFlags.testAny(S390CG_specializedEpilogues); }
   void setSpecializedEpilogues(bool b) { _cgFlags.set(S390CG_specializedEpilogues, b); }

   bool getSupportsBitOpCodes() { return true;}

   bool getSupportsImplicitNullChecks() { return _cgFlags.testAny(S390CG_implicitNullChecks); }
   void setSupportsImplicitNullChecks(bool b) { _cgFlags.set(S390CG_implicitNullChecks, b); }

   bool getConditionalMovesEvaluationMode() { return _cgFlags.testAny(S390CG_conditionalMovesEvaluation); }
   void setConditionalMovesEvaluationMode(bool b) { _cgFlags.set(S390CG_conditionalMovesEvaluation, b); }

   bool getEnableRIOverPrivateLinkage() { return _cgFlags.testAny(S390CG_enableRIOverPrivateLinkage); }
   void setEnableRIOverPrivateLinkage(bool b) { _cgFlags.set(S390CG_enableRIOverPrivateLinkage, b); }

   bool getCondCodeShouldBePreserved() { return _cgFlags.testAny(S390CG_condCodeShouldBePreserved); }
   void setCondCodeShouldBePreserved(bool b) { _cgFlags.set(S390CG_condCodeShouldBePreserved, b); }

   uint8_t getFCondMoveBranchOpCond() { return fCondMoveBranchOpCond; }
   void setFCondMoveBranchOpCond(TR::InstOpCode::S390BranchCondition b) { fCondMoveBranchOpCond = (getMaskForBranchCondition(b) >> 4) & 0xF; }

   uint8_t getRCondMoveBranchOpCond() { return 0xF - fCondMoveBranchOpCond; }

   void markBlockThatModifiesRegister(TR::RealRegister::RegNum reg, int32_t blockNum);

   /** Checks if reg has to be restored in block blockNumber during epilogue */
   bool restoreRegister(TR::RealRegister::RegNum reg, int32_t blockNumber);
   void performReachingBlocks();

   /** Support for shrinkwrapping */
   bool processInstruction(TR::Instruction *instr, TR_BitVector **registerUsageInfo, int32_t &blockNum, int32_t &isFence, bool traceIt); // virt
   uint32_t isPreservedRegister(int32_t regIndex);
   bool isReturnInstruction(TR::Instruction *instr);
   bool isBranchInstruction(TR::Instruction *instr);
   bool isLabelInstruction(TR::Instruction *instr);
   int32_t isFenceInstruction(TR::Instruction *instr);
   bool isAlignmentInstruction(TR::Instruction *instr);
   TR::Instruction *splitEdge(TR::Instruction *cursor, bool isFallThrough, bool needsJump, TR::Instruction *newSplitLabel, TR::list<TR::Instruction*> *jmpInstrs, bool firstJump = false);
   TR::Instruction *splitBlockEntry(TR::Instruction *instr);
   int32_t computeRegisterSaveDescription(TR_BitVector *regs, bool populateInfo = false);
   void processIncomingParameterUsage(TR_BitVector **registerUsageInfo, int32_t blockNum);
   void updateSnippetMapWithRSD(TR::Instruction *cur, int32_t rsd);
   bool isTargetSnippetOrOutOfLine(TR::Instruction *instr, TR::Instruction **start, TR::Instruction **end);
   bool canUseImmedInstruction(int64_t v);
   void ensure64BitRegister(TR::Register *reg);

   virtual bool isAddMemoryUpdate(TR::Node * node, TR::Node * valueChild);
   bool inlinePackedLongConversion();

   bool globalAccessRegistersSupported();

   // AR mode
   bool getRAPassAR() {return false;}
   void setRAPassAR();
   void resetRAPassAR();

#ifdef DEBUG
   void dumpPreGPRegisterAssignment(TR::Instruction *);
   void dumpPostGPRegisterAssignment(TR::Instruction *, TR::Instruction *);

   // Internal local RA counters for self evaluation
   void clearTotalSpills()        {_totalColdSpills=0;        _totalHotSpills=0;       }
   void clearTotalRegisterXfers() {_totalColdRegisterXfers=0; _totalHotRegisterXfers=0;}
   void clearTotalRegisterMoves() {_totalColdRegisterMoves=0; _totalHotRegisterMoves=0;}

   // current RA block is only valid during register allocation pass and is only used for debug
   TR::Block * getCurrentRABlock()           { return _curRABlock; }
   void setCurrentRABlock(TR::Block * block) { _curRABlock = block; }

   void incTotalSpills();
   void incTotalRegisterXfers();
   void incTotalRegisterMoves();
   void printStats(int32_t);
#endif

   TR_S390ProcessorInfo *getS390ProcessorInfo() {return &_processorInfo;}

   TR_S390OutOfLineCodeSection *findS390OutOfLineCodeSectionFromLabel(TR::LabelSymbol *label);

   TR::Instruction *generateNop(TR::Node *node, TR::Instruction *preced=0, TR_NOPKind nopKind=TR_NOPStandard);
   void insertPad(TR::Node *theNode,TR::Instruction *insertionPoint,int32_t padSize,bool);

   void setCurrentlyClobberedRestrictedRegister(int32_t regNum)
      {
      _currentlyClobberedRestrictedRegister = regNum;
      }
   void clearCurrentlyClobberedRestrictedRegister()
      {
      _currentlyClobberedRestrictedRegister = TR_INVALID_REGISTER;
      }
   int32_t getCurrentlyClobberedRestrictedRegister(int32_t regNum)
      {
      return _currentlyClobberedRestrictedRegister;
      }

   TR_BitVector * setKilledRestrictedRegisters(TR_BitVector *killRestrictedRegsBV)
      {
      return _killedRestrictedRegisters = killRestrictedRegsBV;
      }
   TR_BitVector * getKilledRestrictedRegisters()
      {
      return _killedRestrictedRegisters;
      }

   TR_BitVector * setCurrentlyRestrictedRegisters(TR_BitVector *currentRestrictedRegsBV)
      {
      return _currentlyRestrictedRegisters = currentRestrictedRegsBV;
      }

   TR_BitVector * getCurrentlyRestrictedRegisters()
      {
      return _currentlyRestrictedRegisters;
      }

   /** Heuristicly get the register pressure for doing codegen */
   int8_t getCurrentRegisterPressure(TR_RegisterKinds rk = TR_GPR);

    struct TR_S390ConstantDataSnippetKey
        {
           void * c;
           uint32_t size;
           TR::Node *node;
        };

    struct constantHashInfo
        {
          static CS2::HashValue Hash(const TR_S390ConstantDataSnippetKey & key, const CS2::HashValue hv = CS2::CS2_FNV_OFFSETBASIS)
              {
              return CS2::Hash_FNV((const unsigned char*)key.c,key.size, hv);
              }
          static bool Equal(const TR_S390ConstantDataSnippetKey & key1,const TR_S390ConstantDataSnippetKey & key2)
              {
              if(key1.size != key2.size)
                  return false;
              if (key1.node != key2.node)
                  return false;
              switch (key1.size)
                 {
                 case 4:
                    return *((int32_t *) key2.c) == *((int32_t *) key1.c);
                 case 8:
                    return *((int64_t *)key2.c) == *((int64_t *) key1.c);
                 case 2:
                    return *((int16_t *)key2.c) == *((int16_t *)key1.c);
                 case 256:
                    return memcmp(key1.c, key2.c, 256) == 0;
                 }
              return false;
              }
        };

    typedef CS2::HashTable<TR_S390ConstantDataSnippetKey,TR::S390ConstantDataSnippet *, TR::Allocator, constantHashInfo> TR_ConstantSnippetHash;
    typedef TR_ConstantSnippetHash::Cursor TR_ConstHashCursor;


   /** First Snippet (can be AddressSnippet or ConstantSnippet) */
   TR::Snippet *getFirstSnippet();
   // Constant Data List functions
   int32_t setEstimatedOffsetForConstantDataSnippets(int32_t targetAddressSnippetSize);
   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart);
   void emitDataSnippets();
   bool hasDataSnippets() { return (_constantList.empty() && _writableList.empty() && _snippetDataList.empty() && _constantHash.IsEmpty()) ? false : true; }
   TR::list<TR::S390ConstantDataSnippet*> &getConstantInstructionSnippets() { return _constantList; }
   TR::list<TR::S390ConstantDataSnippet*> &getConstantDataStringSnippets() { return _constantList; }
   TR_ConstHashCursor getConstantDataSnippets() { return _constantHashCur;}
   TR::S390ConstantDataSnippet * getConstantDataSnippet(CS2::HashIndex hi) { return _constantHash.DataAt(hi);}


   TR::S390ConstantDataSnippet * create64BitLiteralPoolSnippet(TR::DataType dt, int64_t value);
   TR::S390ConstantDataSnippet * createLiteralPoolSnippet(TR::Node * node);
   TR::S390ConstantInstructionSnippet *createConstantInstruction(TR::CodeGenerator * cg, TR::Node *node, TR::Instruction * instr);
   TR::S390ConstantDataSnippet *findOrCreateConstant(TR::Node *, void *c, uint16_t size);
   TR::S390ConstantDataSnippet *findOrCreate2ByteConstant(TR::Node *, int16_t c, bool isWarm = 0);
   TR::S390ConstantDataSnippet *findOrCreate4ByteConstant(TR::Node *, int32_t c, bool isWarm = 0);
   TR::S390ConstantDataSnippet *findOrCreate8ByteConstant(TR::Node *, int64_t c, bool isWarm = 0);
   TR::S390ConstantDataSnippet *Create4ByteConstant(TR::Node *, int32_t c, bool writable);
   TR::S390ConstantDataSnippet *Create8ByteConstant(TR::Node *, int64_t c, bool writable);
   TR::S390ConstantDataSnippet *CreateConstant(TR::Node *, void *c, uint16_t size, bool writable);
   TR::S390ConstantDataSnippet *getFirstConstantData();

   TR::S390LabelTableSnippet *createLabelTable(TR::Node *, int32_t);

   // Writable Data List functions
   bool hasWritableDataSnippets() { return _writableList.empty() ? false : true; }
   TR::S390WritableDataSnippet *CreateWritableConstant(TR::Node *);

   // OutOfLineCodeSection List functions
   TR::list<TR_S390OutOfLineCodeSection*> &getS390OutOfLineCodeSectionList() {return _outOfLineCodeSectionList;}
   TR_S390OutOfLineCodeSection *findOutLinedInstructionsFromLabel(TR::LabelSymbol *label);



   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm);


   // Snippet Data functions
   void addDataConstantSnippet(TR::S390ConstantDataSnippet * snippet);

   // Identify the Inst selection phase
   bool getDoingInstructionSelection() { return _cgFlags.testAny(S390CG_doingInstructionSelection); }
   void setDoingInstructionSelection(bool b) { _cgFlags.set(S390CG_doingInstructionSelection, b); }

   // Target Address List functions
   int32_t setEstimatedOffsetForTargetAddressSnippets();
   int32_t setEstimatedLocationsForTargetAddressSnippetLabels(int32_t estimatedSnippetStart);
   void emitTargetAddressSnippets();
   bool hasTargetAddressSnippets() { return _targetList.empty() ? false : true; }
   TR::S390LookupSwitchSnippet  *CreateLookupSwitchSnippet(TR::Node *,  TR::Snippet* s);
   TR::S390TargetAddressSnippet *CreateTargetAddressSnippet(TR::Node *, TR::Snippet* s);
   TR::S390TargetAddressSnippet *CreateTargetAddressSnippet(TR::Node *, TR::LabelSymbol * s);
   TR::S390TargetAddressSnippet *CreateTargetAddressSnippet(TR::Node *, TR::Symbol* s);
   TR::S390TargetAddressSnippet *findOrCreateTargetAddressSnippet(TR::Node *, uintptrj_t s);
   TR::S390TargetAddressSnippet *getFirstTargetAddress();

   // Transient Long Registers

   TR_Array<TR::Register *> &getTransientLongRegisters() {return _transientLongRegisters;}
   void freeAndResetTransientLongs();
   uint32_t registerBitMask(int32_t reg)
      {
      return 1 << (reg-1);
      }
   bool fixedPointOverflowExceptionEnabled() { return false; }

   bool opCodeIsNoOpOnThisPlatform(TR::ILOpCode &opCode);

   bool internalPointerSupportImplemented();
   // list of branch unconditional relative instructions used for patching entry point
   // need to be updated with correct offset to sampling preprologue
   TR::list<TR::Instruction*> _recompPatchInsnList;

   bool supportsFusedMultiplyAdd() {return true;}
   bool supportsSinglePrecisionSQRT() {return true;}
   bool supportsLongRegAllocation()
      {
      return TR::Compiler->target.isZOS() && TR::Compiler->target.is32Bit();
      }

   bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod method);

   bool isAddressScaleIndexSupported(int32_t scale) { if (scale <= 2) return true; return false; }
   using OMR::CodeGenerator::getSupportsConstantOffsetInAddressing;
   bool getSupportsConstantOffsetInAddressing(int64_t value);

   void enableLiteralPoolRegisterForGRA ();
   void setOnDemandLiteralPoolRun(bool val)     { _cgFlags.set(S390CG_literalPoolOnDemandOnRun, val); }
   bool isLiteralPoolOnDemandOn () { return _cgFlags.testAny(S390CG_literalPoolOnDemandOnRun); }
   virtual void setGlobalStaticBaseRegisterOn(bool val)     { _cgFlags.set(S390CG_globalStaticBaseRegisterOn, val); }
   virtual bool isGlobalStaticBaseRegisterOn () { return _cgFlags.testAny(S390CG_globalStaticBaseRegisterOn); }
   virtual void setGlobalStaticBaseRegisterOnFlag();
   virtual void setGlobalPrivateStaticBaseRegisterOn(bool val)     { _cgFlags.set(S390CG_globalPrivateStaticBaseRegisterOn, val); }
   virtual bool isGlobalPrivateStaticBaseRegisterOn () { return _cgFlags.testAny(S390CG_globalPrivateStaticBaseRegisterOn); }

   bool isAddressOfStaticSymRefWithLockedReg(TR::SymbolReference *symRef);
   bool isAddressOfPrivateStaticSymRefWithLockedReg(TR::SymbolReference *symRef);

   bool canUseRelativeLongInstructions(int64_t value);
   bool supportsOnDemandLiteralPool();

   bool supportsDirectIntegralLoadStoresFromLiteralPool();

   void setSupportsHighWordFacility(bool val)  { _cgFlags.set(S390CG_supportsHighWordFacility, val); }
   bool supportsHighWordFacility()     { return _cgFlags.testAny(S390CG_supportsHighWordFacility); }

   void setCanExceptByTrap(bool val) { _cgFlags.set(S390CG_canExceptByTrap, val); }
   virtual bool canExceptByTrap()    { return _cgFlags.testAny(S390CG_canExceptByTrap); }

   TR_BackingStore *getLocalF2ISpill();

   TR_BackingStore *getLocalD2LSpill();

   // The reusable temp slot is an 8-byte scalar temp that gets mapped to the stack
   // the first time allocateReusableTempSlot() is called.  This temp is meant to
   // be used for short periods of time.
   TR::SymbolReference* allocateReusableTempSlot();
   void freeReusableTempSlot();

   bool isActiveCompareCC(TR::InstOpCode::Mnemonic opcd, TR::Register* tReg, TR::Register* sReg);
   bool isActiveArithmeticCC(TR::Register* tstReg);
   bool isActiveLogicalCC(TR::Node* ccNode, TR::Register* tstReg);

   bool mulDecompositionCostIsJustified(int32_t numOfOperations, char bitPosition[], char operationType[], int64_t value);

   bool disableCommoningOfVolatiles() { return false; } // TODO : Identitiy needs folding
   bool allowDSEOfVolatiles() { return true; } // TODO : Identitiy needs folding

   bool canUseGoldenEagleImmediateInstruction( int32_t value )
     {
     return true;
     };

   bool canUseGoldenEagleImmediateInstruction( int64_t value )
     {
     return value >= GE_MIN_IMMEDIATE_VAL && value <= GE_MAX_IMMEDIATE_VAL;
     };

   /** Checks if the immediate value can fit in 32-bit immediate field that is unsigned. */
   bool canUseGoldenEagleUnsignedImmediateInstruction(int64_t value)
     {
     return value == (value & GE_MAX_UNSIGNED_IMMEDIATE_VAL);
     }

   /** LL: Sign extended the specified number of high order bits in the register. */
   bool signExtendedHighOrderBits( TR::Node * node, TR::Register * targetRegister, uint32_t numberOfBits );

   /** LL: Sign extended the specified number of high order bits in the register, use for 64 bits. */
   bool signExtendedHighOrderBits( TR::Node * node, TR::Register * targetRegister, TR::Register * srcRegister, uint32_t numberOfBits );

   /** LL: Zero filled the specified number of high order bits in the register. */
   bool clearHighOrderBits( TR::Node * node, TR::Register * targetRegister, uint32_t numberOfBits );

   /** LL: Zero filled the specified number of high order bits in the register, use for 64 bits. */
   bool clearHighOrderBits( TR::Node * node, TR::Register * targetRegister, TR::Register * srcRegister, uint32_t numberOfBits );

   void replaceInst(TR::Instruction* old, TR::Instruction* curr);

   void deleteInst(TR::Instruction* old);

   /** For using VM Thread Register as assignable register */
   bool needsVMThreadDependency();

   TR::RegisterDependencyConditions *addVMThreadPreCondition(TR::RegisterDependencyConditions *deps, TR::Register *reg);
   TR::RegisterDependencyConditions *addVMThreadPostCondition(TR::RegisterDependencyConditions *deps, TR::Register *reg);
   TR::RegisterDependencyConditions *addVMThreadDependencies(TR::RegisterDependencyConditions *deps, TR::Register *reg);
   virtual void setVMThreadRequired(bool v); //override TR::CodeGenerator::setVMThreadRequired

   bool ilOpCodeIsSupported(TR::ILOpCodes);

   void setupSpecializedEpilogues();

   void setUsesZeroBasePtr( bool v = true );
   bool getUsesZeroBasePtr();

   int16_t getMinShortForLongCompareNarrower() { return 0; }
   int8_t getMinByteForLongCompareNarrower() { return 0; }

   bool IsInMemoryType(TR::DataType type);

   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget) { if (isByteSource) return 25; else return 5; }

   int32_t arrayTranslateAndTestMinimumNumberOfIterations() { return 4; }

   /** Yank the scaling opp up a tree in lowerTrees */
   bool yankIndexScalingOp() {return true;}

   bool excludeInvariantsFromGRAEnabled();

   /**
    * array translate on TRex and below requires that the translate table be page-aligned
    * for TRTO/TRTT and double-word aligned for TROT/TROO
    * array translate takes awhile to initialize - for very short translations, the original
    * loop of instructions should be driven instead of the translate instruction
    * LL: For Golden Eagle, TRTO/TRTT is also double-word aligned.
    */
   int32_t arrayTranslateTableRequiresAlignment(bool isByteSource, bool isByteTarget)
      {
      return 7;
      }

   int64_t getCurrentTimeMillisDivisor() { return 8000; }
   int64_t getNanoTimeMultiplier() { return 125; }

   // LL: move to .cpp
   bool arithmeticNeedsLiteralFromPool(TR::Node *node);

   // LL: move to .cpp
   bool bitwiseOpNeedsLiteralFromPool(TR::Node *parent, TR::Node *child);

   bool bndsChkNeedsLiteralFromPool(TR::Node *child);

   bool constLoadNeedsLiteralFromPool(TR::Node *node);
   virtual bool isDispInRange(int64_t disp);

   bool getSupportsOpCodeForAutoSIMD(TR::ILOpCode, TR::DataType);

   bool canOptimizeEdit(uint8_t *parm, int32_t length);
   bool parseEditParm(uint8_t *parm,
                      int32_t length,
                      bool &leadingSigs,
                      bool &trailingStr,
                      uint8_t &fill,
                      int32_t &numDigits,
                      int32_t &numSigs,
                      int32_t &sigOff,
                      int32_t &lastDigit,
                      int32_t &digitsAfterSig);

   TR::Instruction *_ccInstruction;
   TR::Instruction* ccInstruction() { return _ccInstruction; }
   void setCCInstruction(TR::Instruction* cc) { _ccInstruction = cc; }

   #define TR_DEFAULT_DATA_SNIPPET_EXPONENT 4
   int32_t constantDataSnippetExponent() { return TR_DEFAULT_DATA_SNIPPET_EXPONENT; } // 1 << 4 = 16 byte max size for each constantDataSnippet


 private:
   TR_BitVector _globalGPRsPreservedAcrossCalls;
   TR_BitVector _globalFPRsPreservedAcrossCalls;

   TR_BitVector *getBlocksThatModifyRegister(TR::RealRegister::RegNum reg)
      {
      return _blocksThatModifyRegister[reg];
      }


   /**
    * _processorInfo contains the targetted hardware level for the compilation.
    * This may be different than the real hardware the JVM is currently running
    * on, due to user specified options.
    */
   TR_S390ProcessorInfo            _processorInfo;
   TR::S390ImmInstruction          *_returnTypeInfoInstruction;
   RegisterAssignmentDirection     assignmentDirection;
   int32_t                        _extentOfLitPool;  // excludes snippets
   uint64_t                       _availableHPRSpillMask;

   TR::list<TR::S390TargetAddressSnippet*> _targetList;

protected:
   TR::list<TR::S390ConstantDataSnippet*>  _constantList;
   TR::list<TR::S390ConstantDataSnippet*>  _snippetDataList;


private:
   TR::list<TR::S390WritableDataSnippet*>  _writableList;
   TR::list<TR_S390OutOfLineCodeSection*> _outOfLineCodeSectionList;

   CS2::HashTable<TR::Register *, TR::RealRegister::RegNum, TR::Allocator> _previouslyAssignedTo;

   TR_ConstantSnippetHash _constantHash;
   TR_ConstHashCursor     _constantHashCur;

   TR_HashTab * _notPrintLabelHashTab;
   TR_HashTab * _interfaceSnippetToPICsListHashTab;

   TR::RegisterIterator            *_aRegisterIterator;
   TR::RegisterIterator            *_hpRegisterIterator;
   TR::RegisterIterator            *_vrfRegisterIterator;

   TR_Array<TR::Register *>        _transientLongRegisters;

   /** For aggregate type GRA */
   bool considerAggregateSizeForGRA(int32_t size);

#ifdef DEBUG
   uint32_t _totalColdSpills;
   uint32_t _totalColdRegisterXfers;
   uint32_t _totalColdRegisterMoves;
   uint32_t _totalHotSpills;
   uint32_t _totalHotRegisterXfers;
   uint32_t _totalHotRegisterMoves;
   TR::Block * _curRABlock;
#endif

   bool  TR_LiteralPoolOnDemandOnRun;


   /** Saves the preprologue offset to allow JIT entry point alignment padding. */
   int32_t _preprologueOffset;

   TR_BackingStore* _localF2ISpill;

   TR_BackingStore* _localD2LSpill;

   uint8_t fCondMoveBranchOpCond;

   TR::SymbolReference* _reusableTempSlot;

   TR_ReachingBlocks                *_reachingBlocks;
   TR_BitVector                     **_blocksThatModifyRegister;

   TR_BitVector  *_currentlyRestrictedRegisters;
   TR_BitVector  *_killedRestrictedRegisters;

   TR::list<TR::Register *> _internalControlFlowRegisters;

   CS2::HashTable<ncount_t, bool, TR::Allocator> _nodesToBeEvaluatedInRegPairs;

protected:
   flags32_t  _cgFlags;

   /** Miscellaneous S390CG boolean flags. */
   typedef enum
      {
      // Available                       = 0x00000001,
      S390CG_extCodeBaseRegisterIsFree   = 0x00000002,
      S390CG_doingInstructionSelection   = 0x00000004,
      S390CG_addStorageReferenceHints    = 0x00000008,
      S390CG_isOutOfLineHotPath          = 0x00000010,
      S390CG_literalPoolOnDemandOnRun    = 0x00000020,
      S390CG_prefetchNextStackCacheLine  = 0x00000040,
      S390CG_doesExit                    = 0x00000080,
      S390CG_specializedEpilogues        = 0x00000100,
      S390CG_implicitNullChecks          = 0x00000200,
      S390CG_reusableSlotIsFree          = 0x00000400,
      S390CG_conditionalMovesEvaluation  = 0x00000800,
      S390CG_usesZeroBasePtr             = 0x00001000,
      S390CG_enableRIOverPrivateLinkage  = 0x00002000,
      S390CG_condCodeShouldBePreserved   = 0x00004000,
      S390CG_enableBranchPreload         = 0x00008000,
      S390CG_globalStaticBaseRegisterOn  = 0x00010000,
      S390CG_supportsHighWordFacility    = 0x00020000,
      S390CG_canExceptByTrap             = 0x00040000,
      S390CG_enableTLHPrefetching        = 0x00080000,
      S390CG_enableBranchPreloadForCalls = 0x00100000,
      S390CG_globalPrivateStaticBaseRegisterOn = 0x00200000
      } TR_S390CGFlags;
private:

   TR::Node *_nodeAddressOfCachedStatic;
   protected:

   int32_t _currentlyClobberedRestrictedRegister;
   TR::SparseBitVector _bucketPlusIndexRegisters;
   TR::Instruction *_currentDEPEND;
   uintptr_t _outgoingArgLevelDuringTreeEvaluation;
   };

}

}


class TR_S390Peephole
   {
public:
   TR_S390Peephole(TR::Compilation* comp, TR::CodeGenerator *cg)
      : _fe(comp->fe()),
        _outFile(comp->getOutFile()),
        _cursor(comp->getFirstInstruction()),
        _cg(cg)
      {
      }

   void perform();

private:
   void printInfo(const char* info)
      {
      if (_outFile)
         {
         if ( !( !comp()->getOption(TR_TraceCG) && comp()->getOptions()->getTraceCGOption(TR_TraceCGPostBinaryEncoding) && comp()->getOptions()->getTraceCGOption(TR_TraceCGMixedModeDisassembly) )  )
            {
            trfprintf(_outFile, info);
            }
         }
      }

   void printInst()
      {
      if (_outFile)
         {
         if ( !( !comp()->getOption(TR_TraceCG) && comp()->getOptions()->getTraceCGOption(TR_TraceCGPostBinaryEncoding) && comp()->getOptions()->getTraceCGOption(TR_TraceCGMixedModeDisassembly) )  )
            {
            comp()->getDebug()->print(_outFile, _cursor);
            }
         }
      }

   bool LLCReduction();
   bool LGFRReduction();
   bool AGIReduction();
   bool ICMReduction();
   bool LAReduction();
   bool NILHReduction();
   bool duplicateNILHReduction();
   bool unnecessaryNILHReduction();
   bool clearsHighBitOfAddressInReg(TR::Instruction *inst, TR::Register *reg);
   bool branchReduction();
   bool forwardBranchTarget();
   bool seekRegInFutureMemRef(int32_t ,TR::Register *);
   bool LRReduction();
   bool ConditionalBranchReduction(TR::InstOpCode::Mnemonic branchOPReplacement);
   bool CompareAndBranchReduction();
   bool LoadAndMaskReduction(TR::InstOpCode::Mnemonic LZOpCode);
   bool removeMergedNullCHK();
   bool trueCompEliminationForCompareAndBranch();
   bool trueCompEliminationForCompare();
   bool trueCompEliminationForLoadComp();
   bool attemptZ7distinctOperants();
   bool DeadStoreToSpillReduction();
   bool tryMoveImmediate();
   bool isBarrierToPeepHoleLookback(TR::Instruction *current);

   /** \brief
    *     Attempts to reduce LHI R,0 instructions to XR R,R instruction to save 2 bytes of icache.
    *
    *  \return
    *     true if the reduction was successful; false otherwise.
    */
   bool ReduceLHIToXR();

   // DAA related Peephole optimizations
   bool DAARemoveOutlinedLabelNop   (bool hasPadding);
   bool DAARemoveOutlinedLabelNopCVB(bool hasPadding);

   bool DAAHandleMemoryReferenceSpill(bool hasPadding);

   bool revertTo32BitShift();
   bool inlineEXtargetHelper(TR::Instruction *, TR::Instruction *);
   bool inlineEXtarget();
   void markBlockThatModifiesRegister(TR::Instruction *, TR::Register *, int32_t);
   void reloadLiteralPoolRegisterForCatchBlock();

   TR::Compilation * comp() { return TR::comp(); }

private:
   void setupSpecializedEpilogues();

private:
   TR_FrontEnd * _fe;
   TR::FILE *_outFile;
   TR::Instruction *_cursor;
   TR::CodeGenerator *_cg;
};

class TR_S390ScratchRegisterManager : public TR_ScratchRegisterManager
   {
   public:

   TR_S390ScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg) : TR_ScratchRegisterManager(capacity, cg) {}
   };

#endif
