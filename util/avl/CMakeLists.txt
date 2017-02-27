add_tracegen(avl.tdf)

add_library(j9avl STATIC
	avlsup.c
	ut_avl.c
)

target_include_directories(j9avl
	PUBLIC
		.
)

