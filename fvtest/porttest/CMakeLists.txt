include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

set(cudaSources "")

if(OMR_OPT_CUDA)
	set(cudaSources
		cudaBasic.cpp
		cudaError.cpp
		cudaEvent.cpp
		cudaInvalid.cpp
		cudaLaunch.cpp
		cudaLink.cpp
		cudaMemory.cpp
		cudaModule.cpp
		cudaPeer.cpp
		cudaPropery.cpp
		cudaPtx.cpp
		cudaStram.cpp
		cudaTests.cpp
	)
endif()


add_executable(omrporttest
	fileTest.cpp
	heapTest.cpp
	j9portTest.cpp
	main.cpp
	memTest.cpp
	omrdumpTest.cpp
	omrerrorTest.cpp
	omrfileTest.cpp
	omrfilestreamTest.cpp
	omrheapTest.cpp
	omrintrospectTest.cpp
	omrmemTest.cpp
	omrmmapTest.cpp
	omrsignalExtendedTest.cpp
	omrsignalTest.cpp
	omrslTest.cpp
	omrstrTest.cpp
	omrtimeTest.cpp
	omrttyExtendedTest.cpp
	omrttyTest.cpp
	omrvmemTest.cpp
	si.cpp
	si_numcpusTest.cpp
	testHelpers.cpp
	testProcessHelpers.cpp
	vmemTest.cpp
	
	${cudaSources}
)



target_link_libraries(omrporttest
	omrGtestGlue
	omrcore
	omrport
	omrtestutil
)

add_library(sltestlib SHARED
	sltestlib/sltest.c)

#Shared lib test broken on windows
if(OMR_HOST_OS STREQUAL "win")
	add_test(NAME porttest COMMAND omrporttest --gtest-filter="-PortSlTest.*")
else()
	add_test(NAME porttest COMMAND omrporttest)
endif()

if(OMR_OPT_CUDA)
	add_test(NAME cuda_porttest COMMAND omrporttest --gtest_filter="Cuda*" -earlyExit)
endif()

