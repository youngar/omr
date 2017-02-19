cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

set(OMR_VERSION_MAJOR 0)
set(OMR_VERSION_MINOR 0)
set(OMR_VERSION_PATCH 1)
set(OMR_VERSION ${OMR_VERSION_MAJOR}.${OMR_VERSION_MINOR}.${OMR_VERSION_PATCH})

project(omr VERSION ${OMR_VERSION} LANGUAGES CXX C)

###
### Major Feature Flags
###

set(OMR_GC On CACHE BOOL "Enable the GC")
set(OMR_COMPILER Off CACHE BOOL "Enable the compiler")
set(OMR_JITBUILDER Off CACHE BOOL "Enable building JitBuilder")
set(OMR_TEST_COMPILER Off CACHE BOOL "Enable building the test compiler")
set(OMR_PORT On CACHE BOOL "Enable portability library")
set(OMR_THREAD On CACHE BOOL "Enable thread library")
set(OMR_OMRSIG On CACHE BOOL "Enable the OMR signal compatibility library")
# set(OMR_TOOLS On CACHE BOOL "Enable the build tools")
# TODO: Support building only tools for cross-compilation build

###
### Getting the glue directory
###

set(OMR_GLUE "./example/glue" CACHE PATH "The glue directory")

# TODO: OMR_EXAMPLE flag
# TODO: OMR_RTTI flag

# TODO: Version things

set(OMR_VERSION_STRING "<Unknown>" CACHE STRING "") # TODO: Set the version
set(OMR_JIT_VERSION_STRING "<Unknown>" CACHE STRING "") # TODO: Set the version

###
### Versions and stuff
###

include(cmake/platform.cmake)

###
### Boolean Feature Flags
###

# TODO: This is a pretty crazy list, can we move it to their subprojects?

## Default on
set(OMR_GC_ALLOCATION_TAX On CACHE BOOL "TODO: Document")
set(OMR_GC_ARRAYLETS On CACHE BOOL "TODO: Document")
set(OMR_GC_BATCH_CLEAR_TLH On CACHE BOOL "TODO: Document")
set(OMR_GC_COMBINATION_SPEC On CACHE BOOL "TODO: Document")
set(OMR_GC_DEBUG_ASSERTS On CACHE BOOL "TODO: Document")
set(OMR_GC_HEAP_CARD_TABLE On CACHE BOOL "TODO: Document")
set(OMR_GC_LARGE_OBJECT_AREA On CACHE BOOL "TODO: Document")
set(OMR_GC_MINIMUM_OBJECT_SIZE On CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_STANDARD On CACHE BOOL "TODO: Document")
set(OMR_GC_NON_ZERO_TLH On CACHE BOOL "TODO: Document")
set(OMR_GC_THREAD_LOCAL_HEAP On CACHE BOOL "TODO: Document")
set(OMR_INTERP_HAS_SEMAPHORES On CACHE BOOL "TODO: Document")
set(OMR_RAS_TDF_TRACE On CACHE BOOL "TODO: Document")
set(OMR_THR_ADAPTIVE_SPIN On CACHE BOOL "TODO: Document")
set(OMR_THR_JLMOMR_THR_JLM_HOLD_TIMES On CACHE BOOL "TODO: Document")
set(OMR_THR_LOCK_NURSERY On CACHE BOOL "TODO: Document")

## Default off

set(OMR_GC_COMPRESSED_POINTERS Off CACHE BOOL "TODO: Document")
set(OMR_INTERP_COMPRESSED_OBJECT_HEADER Off CACHE BOOL "TODO: Document")
set(OMR_INTERP_SMALL_MONITOR_SLOT Off CACHE BOOL "TODO: Document")
set(OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS Off CACHE BOOL "TODO: Document")
set(OMR_THR_FORK_SUPPORT Off CACHE BOOL "TODO: Document")
set(OMR_THR_THREE_TIER_LOCKING Off CACHE BOOL "TODO: Document")
set(OMR_THR_CUSTOM_SPIN_OPTIONS Off CACHE BOOL "TODO: Document")
set(OMR_NOTIFY_POLICY_CONTROL Off CACHE BOOL "TODO: Document")
set(OMR_THR_SPIN_WAKE_CONTROL Off CACHE BOOL "TODO: Document")
set(OMR_THR_YIELD_ALG Off CACHE BOOL "TODO: Document")
set(OMR_GC_OBJECT_MAP Off CACHE BOOL "TODO: Document")
set(OMR_ENV_LITTLE_ENDIAN Off CACHE BOOL "TODO: Document")
set(OMR_GC_TLH_PREFETCH_FTA Off CACHE BOOL "TODO: Document")
set(OMR_ENV_GCC Off CACHE BOOL "TODO: Document")
set(OMR_PORT_NUMA_SUPPORT Off CACHE BOOL "TODO: Document")
set(OMR_OPT_CUDA Off CACHE BOOL "TODO: Document")
set(OMR_PORT_ALLOCATE_TOP_DOWN Off CACHE BOOL "TODO: Document")
set(OMR_PORT_ZOS_CEEHDLRSUPPORT Off CACHE BOOL "TODO: Document")
set(OMRTHREAD_LIB_AIX Off CACHE BOOL "TODO: Document")
set(OMRTHREAD_LIB_WIN32 Off CACHE BOOL "TODO: Document")
set(OMRTHREAD_LIB_ZOS Off CACHE BOOL "TODO: Document")
#TODO forcing this to get unix thread lib to compile
set(OMRTHREAD_LIB_UNIX On CACHE BOOL "TODO: Document")
#TODO set to disabled. Stuff fails to compile when its on
set(OMR_THR_TRACING Off CACHE BOOL "TODO: Document")
set(OMR_GC_DYNAMIC_CLASS_UNLOADING Off CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_COMPACTION Off CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_CONCURRENT_MARK Off CACHE BOOL "TODO: Document")
set(OMR_GC_MODRON_SCAVENGER Off CACHE BOOL "TODO: Document")
set(OMR_GC_CONCURRENT_SCAVENGER Off CACHE BOOL "TODO: Document")
set(OMR_GC_CONCURRENT_SWEEP Off CACHE BOOL "TODO: Document")
set(OMR_GC_HYBRID_ARRAYLETS Off CACHE BOOL "TODO: Document")
set(OMR_GC_OBJECT_ALLOCATION_NOTIFY Off CACHE BOOL "TODO: Document")
set(OMR_GC_REALTIME Off CACHE BOOL "TODO: Document")
set(OMR_GC_SEGREGATED_HEAP Off CACHE BOOL "TODO: Document")
set(OMR_GC_STACCATO Off CACHE BOOL "TODO: Document")
set(OMR_GC_VLHGC Off CACHE BOOL "TODO: Document")
set(OMR_PORT_ASYNC_HANDLER Off CACHE BOOL "TODO: Document")

enable_testing()

function(VPathResolve inlist dirlist outlist)
    set(output "")
  foreach(currentFile ${${inlist}})
    set(fileFound False)
    #message("Searching for ${currentFile}")
    foreach(currentDir ${${dirlist}})
        #message("looking in ${currentDir}")
        #message("${CMAKE_CURRENT_SOURCE_DIR}/${currentDir}/${currentFile}")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${currentDir}/${currentFile}")
            list(APPEND output "${currentDir}/${currentFile}")
            set(fileFound True)
            break()
        endif()
    endforeach()
    if(NOT fileFound)
        message(FATAL_ERROR "Could not find file ${currentFile}")
    endif()
  endforeach()
  set(${outlist} ${output} PARENT_SCOPE)
endfunction()

include_directories(
  ${PROJECT_BINARY_DIR}
  ./include/
  ./include_core/
  ./third_party/
)

add_definitions(
  -DUT_DIRECT_TRACE_REGISTRATION # TODO:  Deal with that stupid jni issue in tracegen
)

configure_file(./omrcfg.CMakeTemplate.h omrcfg.h)
configure_file(./omrversionstrings.CMakeTemplate.h omrversionstrings.h)

add_subdirectory(tools)
add_subdirectory(thread)
add_subdirectory(port)
add_subdirectory(util)
add_subdirectory(omr)
add_subdirectory(third_party)

#TODO should be wrapped in if()
add_subdirectory(fvtest)

if(OMR_GC)
  add_subdirectory(gc)
  add_subdirectory("${OMR_GLUE}")
endif(OMR_GC)

if(OMR_COMPILER)
  # TODO: Actually support the compiler
  add_subdirectory(compiler)
endif(OMR_COMPILER)
