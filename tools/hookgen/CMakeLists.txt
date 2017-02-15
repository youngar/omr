
add_executable(hookgen
	HookGen.cpp
	main.cpp
)

target_link_libraries(hookgen pugixml)


#TODO: need to get rid of dir argument, hack to work arround the way we define paths in hook definitions
#TODO: currently output in source tree, should be in build tree
#TODO: Dependecy checking is broken, since it checks for output in build tree rather than src
macro(add_hookgen dir input)
	add_custom_command(
		OUTPUT ${ARGN}
		COMMAND hookgen ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${input}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${input}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dir}
	)
endmacro(add_hookgen)
