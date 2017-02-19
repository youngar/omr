add_tracegen(
	avl.tdf

	${CMAKE_CURRENT_SOURCE_DIR}/ut_avl.c
	${CMAKE_CURRENT_SOURCE_DIR}/ut_avl.h
)
add_library(j9avl STATIC
	avlsup.c
	ut_avl.c
)

target_include_directories(j9avl
	PUBLIC
		.
)

