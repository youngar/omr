
add_executable(hookgen
	HookGen.cpp
	main.cpp
)

target_link_libraries(hookgen pugixml)


#TODO: currently output in source tree, should be in build tree
#TODO: Dependecy checking is broken, since it checks for output in build tree rather than src
function(add_hookgen input)
	get_filename_component(input_dir "${input}" DIRECTORY)
	#message("Hookgen, Output = ${ARGN}")
	#message("Hookgen, depends = ${CMAKE_CURRENT_SOURCE_DIR}/${input}")
	add_custom_command(
		OUTPUT ${ARGN}
		COMMAND hookgen ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${input_dir}
	)
endfunction()
