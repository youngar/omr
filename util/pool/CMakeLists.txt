add_tracegen(pool.tdf)

add_library(j9pool STATIC
	pool.c
	pool_cap.c
	ut_pool.c
)

#target_include_directories(j9pool
#	PUBLIC
#		.
#)
