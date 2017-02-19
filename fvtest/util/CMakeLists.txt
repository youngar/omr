
add_library(omrtestutil STATIC
	printerrorhelper.cpp
	testvmhelper.cpp
)
target_include_directories(omrtestutil PUBLIC .)
