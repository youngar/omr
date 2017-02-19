
add_library(omrGtest STATIC
	omrGtest.cpp)
target_include_directories(omrGtest
	PRIVATE
		${PROJECT_SOURCE_DIR}/third_party/gtest-1.7.0/
		${PROJECT_SOURCE_DIR}/third_party/gtest-1.7.0/src
	PUBLIC
		${PROJECT_SOURCE_DIR}/third_party/gtest-1.7.0/include
)
add_library(omrGtestGlue INTERFACE)

target_include_directories(omrGtestGlue
	INTERFACE
		.
)

target_sources(omrGtestGlue INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/argmain.cpp
)

target_link_libraries(omrGtestGlue INTERFACE omrGtest)

#TODO there is platform specific stuff here
target_link_libraries(omrGtest pthread)
#target_link_libraries(omrGtest INTERFACE omrGtestGlue)
