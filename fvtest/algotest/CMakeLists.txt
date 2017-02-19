add_hookgen(hooksample.hdf
	${CMAKE_CURRENT_SOURCE_DIR}/hooksample.h
	${CMAKE_CURRENT_SOURCE_DIR}/hooksample_internal.h
)

add_executable(omralgotest
	algoTest.cpp
	algorithm_test_internal.h
	avltest.c
	avltest.lst
	hashtabletest.c
	hooksample.h
	hooksample_internal.h
	#hooksample.hdf
	#hooksample.sentinel
	#hooksample_internal.h
	hooktest.c
	main.cpp
	pooltest.c
)

target_link_libraries(omralgotest
	j9avl
	j9hashtable
	j9pool
	omrGtestGlue
	#omrGtest
	omrcore
	omrport
	omrtestutil
)

#TODO hack to get building
target_link_libraries(omralgotest
	#iconv
	rt
	j9hookstatic
)
add_test(algotest omralgotest -avltest:${CMAKE_CURRENT_SOURCE_DIR}/avltest.lst)
