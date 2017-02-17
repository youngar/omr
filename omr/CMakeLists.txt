add_tracegen(
	omrti.tdf

	${CMAKE_CURRENT_SOURCE_DIR}/ut_omrti.c
	${CMAKE_CURRENT_SOURCE_DIR}/ut_omrti.h
)

add_tracegen(
	omrvm.tdf

	${CMAKE_CURRENT_SOURCE_DIR}/ut_omrvm.c
	${CMAKE_CURRENT_SOURCE_DIR}/ut_omrvm.h
)

add_library(omrcore
	OMR_Agent.cpp
	OMR_MethodDictionary.cpp
	OMR_MethodDictionary.hpp
	OMR_Profiler.cpp
	OMR_Runtime.cpp
	OMR_TI.cpp
	OMR_TIMemorySize.cpp
	OMR_VM.cpp
	OMR_VMThread.cpp
	omrrasinit.c
	omrti.tdf
	omrtrcinit.c
	startup/omrvmstartup.cpp
	ut_omrti.c
	ut_omrti.h
	ut_omrvm.c
	ut_omrvm.h
)

target_include_directories(omrcore
	PUBLIC
		.
)

target_link_libraries(omrcore
	PUBLIC
		omrgc # mminitcore.h
)