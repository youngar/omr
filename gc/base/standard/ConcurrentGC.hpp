/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

#if !defined(CONCURRENTGC_HPP_)
#define CONCURRENTGC_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "omr.h"
#include "OMR_VM.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include "ConcurrentCardTable.hpp"
#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "ConcurrentGCStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentStandard.hpp"
#include "ParallelGlobalGC.hpp"

extern "C" {
int con_helper_thread_proc(void *info);
uintptr_t con_helper_thread_proc2(OMRPortLibrary* portLib, void *info);
void J9ConcurrentWriteBarrierStore (OMR_VMThread *vmThread, omrobjectptr_t destinationObject, omrobjectptr_t storedObject);
void J9ConcurrentWriteBarrierBatchStore (OMR_VMThread *vmThread, omrobjectptr_t destinationObject);
}

/**
 * @name Concurrent marking rate
 * @{
 */
#define MAX_ALLOC_2_TRACE_RATE_1 (float)4.0
#define MAX_ALLOC_2_TRACE_RATE_8 (float)2.0
#define MAX_ALLOC_2_TRACE_RATE_10 (float)1.8
 
#define MIN_ALLOC_2_TRACE_RATE_1 (float)1.0
#define MIN_ALLOC_2_TRACE_RATE_8 (float)2.0
#define MIN_ALLOC_2_TRACE_RATE_10 (float)2.0

#define OVER_TRACING_BOOST_FACTOR (float)2.0

/**
 * @}
 */

/**
 * @name Concurrent mark card cleaning factor
 * @{
 */
#define INITIAL_CARD_CLEANING_FACTOR_PASS1_1 (float)0.5
#define INITIAL_CARD_CLEANING_FACTOR_PASS1_8 (float)0.05
#define INITIAL_CARD_CLEANING_FACTOR_PASS1_10 (float)0.05

#define INITIAL_CARD_CLEANING_FACTOR_PASS2_1 (float)0.1
#define INITIAL_CARD_CLEANING_FACTOR_PASS2_8 (float)0.01
#define INITIAL_CARD_CLEANING_FACTOR_PASS2_10 (float)0.01
 
#define MAX_CARD_CLEANING_FACTOR_PASS1_1 (float)0.8
#define MAX_CARD_CLEANING_FACTOR_PASS1_8 (float)0.2
#define MAX_CARD_CLEANING_FACTOR_PASS1_10 (float)0.2

#define MAX_CARD_CLEANING_FACTOR_PASS2_1 (float)0.5
#define MAX_CARD_CLEANING_FACTOR_PASS2_8 (float)0.1
#define MAX_CARD_CLEANING_FACTOR_PASS2_10 (float)0.1

/**
 * @}
 */

/**
 * @name Concurrent mark card cleaning threshold
 * @{
 */
#define CARD_CLEANING_THRESHOLD_FACTOR_1 (float)4.0  
#define CARD_CLEANING_THRESHOLD_FACTOR_8 (float)3.0  
#define CARD_CLEANING_THRESHOLD_FACTOR_10 (float)1.5   
 
/**
 * @}
 */

/**
 * @name Concurrent mark misc definitions
 * @{
 */ 
#define INITIAL_OLD_AREA_LIVE_PART_FACTOR ((float)0.7)
#define LIVE_PART_HISTORY_WEIGHT ((float)0.8)
#define INITIAL_OLD_AREA_NON_LEAF_FACTOR ((float)0.4)
#define NON_LEAF_HISTORY_WEIGHT ((float)0.8)
#define CARD_CLEANING_HISTORY_WEIGHT ((float)0.7)
#define CONCURRENT_HELPER_HISTORY_WEIGHT ((float)0.6) 
#define BYTES_TRACED_IN_PASS_1_HISTORY_WEIGHT ((float)0.8)
	
#define TUNING_HEAP_SIZE_FACTOR ((float)0.05)
#define CONCURRENT_STOP_SAMPLE_GRAIN 0x0F

#define INIT_CHUNK_SIZE 8 * 1024
#define CONCURRENT_INIT_BOOST_FACTOR 8
#define CONCURRENT_KICKOFF_THRESHOLD_BOOST ((float)1.10)
#define LAST_FREE_SIZE_NEEDS_INITIALIZING ((uintptr_t)-1)
#define ALL_BYTES_TRACED_IN_PASS_1 ((float)1.0)

/**
 * @}
 */

#if defined(OMR_GC_CONCURRENT_SWEEP)
class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */

class MM_AllocateDescription;
class MM_ConcurrentSafepointCallback;
class MM_MemorySubSpace;
class MM_MemorySubSpaceConcurrent;
class MM_MemorySubSpaceGenerational;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentGC : public MM_ParallelGlobalGC
{
	/*
	 * Data members
	 */
private:
	typedef enum {
		CONCURRENT_HELPER_WAIT = 1,
		CONCURRENT_HELPER_MARK,
		CONCURRENT_HELPER_SHUTDOWN
	} ConHelperRequest;
	
	typedef enum {
		MARK_BITS=1,
		CARD_TABLE
	} InitType;
	
	typedef struct {
        void *base;
        void *top;
        void * volatile current;
        uintptr_t initBytes;
        InitType type;
        uintptr_t chunkSize;
        MM_MemorySubSpace *subspace;
	} InitWorkItem;

	typedef enum {
		PHASE1=1,
		PHASE2	
	} KickoffPhase;

	typedef enum {
		SOA = 1,
		LOA	
	} MeteringType;
	
	typedef enum {
		VOTE_UNDEFINED = 0,
		VOTE_SOA,
		VOTE_LOA	
	} MeteringVote;

	typedef struct {
		uintptr_t soaFreeBeforeGC;
		uintptr_t soaFreeAfterGC;
		uintptr_t loaFreeBeforeGC;
		uintptr_t loaFreeAfterGC;
		MeteringVote vote;
	} MeteringHistory;

	enum {
		_minTraceSize = 1000,
		_maxTraceSize = 0x20000000,
		_conHelperCleanSize= 0x10000,
		_meteringHistorySize = 5
	};

	MM_ConcurrentCardTable *_cardTable;	/**< pointer to Cards Table */
	void *_heapBase; 
	void *_heapAlloc;
	bool _rebuildInitWork;
	bool _retuneAfterHeapResize;
#if defined(OMR_GC_LARGE_OBJECT_AREA)	
	MeteringHistory *_meteringHistory;
	uint32_t _currentMeteringHistory;
	MeteringType _meteringType;
#endif /* OMR_GC_LARGE_OBJECT_AREA */	
		
	omrthread_t *_conHelpersTable;
	uint32_t _conHelperThreads;
	uint32_t _conHelpersStarted;
	volatile uint32_t _conHelpersShutdownCount;
	omrthread_monitor_t _conHelpersActivationMonitor;
	ConHelperRequest _conHelpersRequest;
	
	bool _globalCollectionInProgress;
	bool _initializeMarkMap;
	omrthread_monitor_t _initWorkMonitor;
	omrthread_monitor_t _initWorkCompleteMonitor;
	omrthread_monitor_t _concurrentTuningMonitor;
	
	/* Concurrent initialization */
	InitWorkItem *_initRanges;
	uint32_t _numInitRanges;
	uintptr_t _numPhysicalInitRanges;	/**< physical size of table */
	volatile uint32_t _nextInitRange;
	uintptr_t _initializers;
	bool _initSetupDone;
	
	/* Mutator tracing statistics */
	uintptr_t _allocToInitRate;
	uintptr_t _allocToTraceRate;
	uintptr_t _allocToTraceRateNormal;
	bool  _secondCardCleanPass;
	uintptr_t _allocToTraceRateCardCleanPass2Boost;
	float _allocToTraceRateMaxFactor;
	float _allocToTraceRateMinFactor;	
	float _bytesTracedInPass1Factor;
	uintptr_t _bytesToCleanPass1;
	uintptr_t _bytesToCleanPass2;
	uintptr_t _bytesToTracePass1;
	uintptr_t _bytesToTracePass2;
	uintptr_t _traceTargetPass1;
	uintptr_t _traceTargetPass2;
	uintptr_t _totalTracedAtPass2KO;
	uintptr_t _totalCleanedAtPass2KO;
	float _tenureLiveObjectFactor;
	float _tenureNonLeafObjectFactor;
	uintptr_t _kickoffThresholdBuffer;
	bool _pass2Started;
	
	/* Periodic tuning statistics */
	volatile uintptr_t _tuningUpdateInterval;
	volatile uintptr_t _lastFreeSize;
	float _lastAverageAlloc2TraceRate;
	float _maxAverageAlloc2TraceRate;
	uintptr_t _lastTotalTraced;

	/* Background helper thread statistics */	
	uintptr_t _lastConHelperTraceSizeCount;
	float _alloc2ConHelperTraceRate; 
	
	/* Concurrent card cleaning statistics */
	float _cardCleaningFactorPass1;
	float _cardCleaningFactorPass2;	
	float _maxCardCleaningFactorPass1;
	float _maxCardCleaningFactorPass2;
	float _cardCleaningThresholdFactor;

	bool _forcedKickoff;	/**< Kickoff forced externally flag */

	uintptr_t _languageKickoffReason;
	MM_CycleState _concurrentCycleState;

protected:
	MM_ConcurrentSafepointCallback *_callback;
	MM_ConcurrentGCStats *_stats;
public:
	
	/*
	 * Function members
	 */
private:
	MMINLINE float getAllocToTraceRateMin() { return (float)(_allocToTraceRate * _allocToTraceRateMinFactor);};
	MMINLINE float getAllocToTraceRateMax() { return (float)(_allocToTraceRate * _allocToTraceRateMaxFactor);};
	MMINLINE float getAllocToTraceRateNormal() { return (float)_allocToTraceRateNormal;};
	
	float interpolateInRange(float, float, float, uintptr_t);
	void determineInitWork(MM_EnvironmentStandard *env);
	void resetInitRangesForConcurrentKO(MM_EnvironmentStandard *env);
	void resetInitRangesForSTW(MM_EnvironmentStandard *env);
	bool getInitRange(MM_EnvironmentStandard *env, void **from, void **to, InitType *type, bool *concurrentCollectable);
	bool allInitRangesProcessed() { return (_nextInitRange == _numInitRanges ? true : false); };
	uintptr_t calculateInitSize(MM_EnvironmentStandard *env, uintptr_t allocationSize);
	uintptr_t calculateTraceSize(MM_EnvironmentStandard *env, MM_AllocateDescription *allocDescription);
	bool periodicalTuningNeeded(MM_EnvironmentStandard *env, uintptr_t freeSize);
	void periodicalTuning(MM_EnvironmentStandard *env, uintptr_t freeSize);
	void kickoffCardCleaning(MM_EnvironmentStandard *env, ConcurrentCardCleaningReason reason);
	
	void adjustTraceTarget(MM_EnvironmentStandard *env);
	void updateTuningStatistics(MM_EnvironmentBase *env);
	void tuneToHeap(MM_EnvironmentStandard *env);

	void conHelperEntryPoint(OMR_VMThread *omrThread, uintptr_t slaveID);
	void shutdownAndExitConHelperThread(OMR_VMThread *omrThread);
	
	bool initializeConcurrentHelpers(MM_GCExtensionsBase *extensions);
	void shutdownConHelperThreads(MM_GCExtensionsBase *extensions);
	void resumeConHelperThreads(MM_EnvironmentStandard *env);
	
	void signalThreadsToDirtyCards(MM_EnvironmentStandard *env);
	bool timeToKickoffConcurrent(MM_EnvironmentStandard *env, MM_AllocateDescription *allocDescription);
	uintptr_t doConcurrentInitialization(MM_EnvironmentStandard *env, uintptr_t initToDo);
	uintptr_t doConcurrentTrace(MM_EnvironmentStandard *env, MM_AllocateDescription *allocDescription, uintptr_t sizeToTrace, MM_MemorySubSpace *subspace, bool tlhAllocation);

	bool tracingRateDropped(MM_EnvironmentStandard *env);
#if defined(OMR_GC_MODRON_SCAVENGER)	
	uintptr_t potentialFreeSpace(MM_EnvironmentStandard *env, MM_AllocateDescription *allocDescription);
	static void hookOldToOldReferenceCreated(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#endif /*OMR_GC_MODRON_SCAVENGER */	
	static void hookCardCleanPass2Start(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);

	bool cleanCards(MM_EnvironmentStandard *env, bool isMutator, uintptr_t sizeToDo, uintptr_t  *sizeDone, bool threadAtSafePoint);

	void reportConcurrentKickoff(MM_EnvironmentStandard *env);
	void reportConcurrentAborted(MM_EnvironmentStandard *env, CollectionAbortReason reason);
	void reportConcurrentHalted(MM_EnvironmentStandard *env);
	void reportConcurrentFinalCardCleaningStart(MM_EnvironmentStandard *env);
	void reportConcurrentFinalCardCleaningEnd(MM_EnvironmentStandard *env, uint64_t duration);
	void reportConcurrentCollectionStart(MM_EnvironmentStandard *env);
	void reportConcurrentCollectionEnd(MM_EnvironmentStandard *env, uint64_t duration);
	
	void reportConcurrentBackgroundThreadActivated(MM_EnvironmentStandard *env);
	void reportConcurrentBackgroundThreadFinished(MM_EnvironmentStandard *env, uintptr_t traceTotal);
	
	void reportConcurrentCompleteTracingStart(MM_EnvironmentStandard *env);
	void reportConcurrentCompleteTracingEnd(MM_EnvironmentStandard *env, uint64_t duration);
	
	void reportConcurrentRememberedSetScanStart(MM_EnvironmentStandard *env);
	void reportConcurrentRememberedSetScanEnd(MM_EnvironmentStandard *env, uint64_t duration);

	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);

	/**
	 * Creates Concurrent Card Table
	 * @param env current thread environment
	 * @return true if table is created
	 */
	bool createCardTable(MM_EnvironmentBase *env);

	void clearConcurrentWorkStackOverflow(MM_EnvironmentStandard *env);

#if defined(OMR_GC_CONCURRENT_SWEEP)
	void concurrentSweep(MM_EnvironmentStandard *env, MM_MemorySubSpace *subspace, MM_AllocateDescription *allocDescription);
	void completeConcurrentSweep(MM_EnvironmentStandard *env);
	void completeConcurrentSweepForKickoff(MM_EnvironmentStandard *env);
#endif /* OMR_GC_CONCURRENT_SWEEP */

#if defined(OMR_GC_LARGE_OBJECT_AREA)		
	void updateMeteringHistoryBeforeGC(MM_EnvironmentStandard *env);
	void updateMeteringHistoryAfterGC(MM_EnvironmentStandard *env);
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	/**
	 * Switch the _conHelperRequest to appropriate state and return the new value.
	 * This will only switch the state if from == the current state.
	 *
	 * @param from - switch the state only if it currently equals this value
	 * @param to - the value to switch the state to
	 * @return the value of _conHelperRequest.
	 */
	ConHelperRequest switchConHelperRequest(ConHelperRequest from, ConHelperRequest to);
	/**
	 * Get the current value of _conHelperRequest
	 *
	 * @return the value of _conHelperRequest.
	 */
	ConHelperRequest getConHelperRequest(MM_EnvironmentBase *env);

protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	void concurrentMark(MM_EnvironmentStandard *env, MM_MemorySubSpace *subspace,  MM_AllocateDescription *allocDescription);
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

public:
	virtual uintptr_t getVMStateID() { return J9VMSTATE_GC_COLLECTOR_CONCURRENTGC; };

	static 	MM_ConcurrentGC *newInstance(MM_EnvironmentBase *env, MM_CollectorLanguageInterface *cli);
	virtual void kill(MM_EnvironmentBase *env);
	
	virtual void postMark(MM_EnvironmentBase *envModron);
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase *extensions);
	virtual void abortCollection(MM_EnvironmentBase *env, CollectionAbortReason reason);
	
	static void signalThreadsToDirtyCardsAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData);
	
	virtual void prepareHeapForWalk(MM_EnvironmentBase *envModron);

	virtual void scanThread(MM_EnvironmentBase *env) { _cli->concurrentGC_scanThread(env); }

	/**
	 * Force Kickoff event externally
	 * @return true if Kickoff can be forced
	 */
	virtual bool forceKickoff(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);

#if defined(OMR_GC_CONCURRENT_SWEEP)
	virtual bool replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, uintptr_t size);
#endif /* OMR_GC_CONCURRENT_SWEEP */
	virtual void payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription);
	bool concurrentFinalCollection(MM_EnvironmentStandard *env, MM_MemorySubSpace *subspace);
	uintptr_t localMark(MM_EnvironmentStandard *env, uintptr_t sizeToTrace);
	
	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	virtual void heapReconfigured(MM_EnvironmentBase *env);

	void finalCleanCards(MM_EnvironmentStandard *env);

	/**
	 * Flush local buffers
	 * (local reference object buffer, work packets)
	 * @param env Local thread environment
	 */
	void flushLocalBuffers(MM_EnvironmentStandard *env);
	
	void clearNewMarkBits(MM_EnvironmentStandard *env);
	void completeTracing(MM_EnvironmentStandard *env);
#if defined(OMR_GC_MODRON_SCAVENGER)
	void scanRememberedSet(MM_EnvironmentStandard *env);
	void oldToOldReferenceCreated(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr);
#endif /* OMR_GC_MODRON_SCAVENGER */
	
	void recordCardCleanPass2Start(MM_EnvironmentStandard *env);
	
	/**
	 * Check if exclusive access has been requested 
	 * Determine whether exclusive access has been requested by another thread 
	 * but only do so once for every CONCURRENT_STOP_SAMPLE_GRAIN call to this routine.
	 * @return  TRUE if concurrent collection is in progress; FALSE otherwise 
	*/
	MMINLINE bool isExclusiveAccessRequestWaitingSparseSample(MM_EnvironmentStandard *env, uintptr_t value)
	{
		/* Have we reached a sample point ? */ 
		if ((0 == (value & CONCURRENT_STOP_SAMPLE_GRAIN)) && env->isExclusiveAccessRequestWaiting()) {
			return true;
		} else {
			return false;
		}	
	}

	/*
	 * Return value of _globalCollectionInProgress flag
	 */
	MMINLINE bool isGlobalCollectionInProgress()
	{
		return _globalCollectionInProgress;
	}

	/**
	 * Return reference to Card Table
	 */
	MMINLINE MM_ConcurrentCardTable *getCardTable()
	{
		return _cardTable;
	}

	void concurrentWorkStackOverflow();
	
	MM_ConcurrentGC(MM_EnvironmentBase *env, MM_CollectorLanguageInterface *cli) :
		MM_ParallelGlobalGC(env, cli)
		,_cardTable(NULL)
		,_heapBase(NULL)
		,_heapAlloc(NULL)
		,_rebuildInitWork(false)
		,_retuneAfterHeapResize(false)
#if defined(OMR_GC_LARGE_OBJECT_AREA)		
		,_meteringHistory(NULL)
		,_currentMeteringHistory(0)
		,_meteringType(SOA)
#endif /* OMR_GC_LARGE_OBJECT_AREA */		
		,_conHelpersTable(NULL)
		,_conHelperThreads((uint32_t)_extensions->concurrentBackground)
		,_conHelpersStarted(0)
		,_conHelpersShutdownCount(0)
		,_conHelpersActivationMonitor(NULL)
		,_conHelpersRequest(CONCURRENT_HELPER_WAIT)
		,_globalCollectionInProgress(false)
		,_initializeMarkMap(false)
		,_initWorkMonitor(NULL)
		,_initWorkCompleteMonitor(NULL)
		,_concurrentTuningMonitor(NULL)
		,_initRanges(NULL)
		,_numInitRanges(0)
		,_numPhysicalInitRanges(0)
		,_nextInitRange(0)
		,_initializers(0)
		,_initSetupDone(false)
		,_bytesToCleanPass1(0)
		,_bytesToCleanPass2(0)
		,_bytesToTracePass1(0)
		,_bytesToTracePass2(0)
		,_tenureLiveObjectFactor(INITIAL_OLD_AREA_LIVE_PART_FACTOR)
		,_tenureNonLeafObjectFactor(INITIAL_OLD_AREA_NON_LEAF_FACTOR)
		,_tuningUpdateInterval(_minTraceSize)
		,_lastFreeSize(LAST_FREE_SIZE_NEEDS_INITIALIZING)
		,_lastAverageAlloc2TraceRate(0)
		,_maxAverageAlloc2TraceRate(0)
		,_lastTotalTraced(0)
		,_lastConHelperTraceSizeCount(0)
		,_alloc2ConHelperTraceRate(0)
		,_forcedKickoff(false)
		,_languageKickoffReason(NO_LANGUAGE_KICKOFF_REASON)
		,_concurrentCycleState()
		,_callback(NULL)
		,_stats(cli->concurrentGC_getConcurrentStats())
		{
			_typeId = __FUNCTION__;
		}
	
	/*
	 * Friends
	 */
	friend int con_helper_thread_proc(void *info);
	friend uintptr_t con_helper_thread_proc2(OMRPortLibrary* portLib, void *info);
	friend class MM_ConcurrentOverflow;
#if defined(OMR_GC_CONCURRENT_SWEEP)
	friend class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */
};

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#endif /* CONCURRENTGC_HPP_ */
