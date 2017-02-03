add_executable(algotest
	algoTest.cpp
	algorithm_test_internal.h
	avltest.c
	avltest.lst
	hashtabletest.c
	hooksample.h
	hooksample.hdf
	hooksample.sentinel
	hooksample_internal.h
	hooktest.c
	main.cpp
	pooltest.c
)

target_link_libraries(algotest
	gtest
	omrcore
	omrport
	omrtestutil
)

add_test(algotest algotest)