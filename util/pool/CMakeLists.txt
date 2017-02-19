add_tracegen(
	pool.tdf

	${CMAKE_CURRENT_SOURCE_DIR}/ut_pool.c
	${CMAKE_CURRENT_SOURCE_DIR}/ut_pool.h
)

add_library(j9pool STATIC
	pool.c
	pool_cap.c
	ut_pool.c
)

#target_include_directories(j9pool
#	PUBLIC
#		.
#)
