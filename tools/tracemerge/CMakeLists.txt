add_executable(tracemerge
	DATMerge.cpp
	DATMerge.hpp
	main.cpp
)

target_include_directories(tracemerge
	PRIVATE
		../tracegen/
)

target_link_libraries(tracemerge
	PRIVATE
		trace # static
)
