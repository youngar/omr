add_executable(omrutiltest
	main.cpp
)

target_link_libraries(omrutiltest
	#omrGtestGlue
	omrGtest
	omrutil
	omrglue
)

target_include_directories(omrutiltest
	PRIVATE
	$<TARGET_PROPERTY:omrGtestGlue,INTERFACE_INCLUDE_DIRECTORIES>
)

add_test(utiltest omrutiltest)
