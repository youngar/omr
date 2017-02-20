
#TODO:
#ifeq ($(OMR_TOOLCHAIN),gcc)
#    MODULE_CFLAGS += -Wno-unused-value
#endif

add_tracegen(
	hashtable.tdf

	${CMAKE_CURRENT_SOURCE_DIR}/ut_hashtable.c
	${CMAKE_CURRENT_SOURCE_DIR}/ut_hashtable.h
)

add_library(j9hashtable STATIC
	hash.c
	hashtable.c
	ut_hashtable.c
)

target_include_directories(j9hashtable
	PUBLIC
		.
)

target_link_libraries(j9hashtable
	PUBLIC
		omrutil
		j9avl
		j9pool
)
