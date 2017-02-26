if (NOT CMAKE_CROSSCOMPILING)

	add_subdirectory(hookgen)
	add_subdirectory(tracemerge)
	add_subdirectory(tracegen)

	export(TARGETS hookgen tracemerge tracegen FILE "ImportTools.cmake")

else(NOT CMAKE_CROSSCOMPILING)

	set(OMR_TOOLS_IMPORTFILE "IMPORTFILE-NOTFOUND" CACHE FILEPATH
  		"Point it to the ImportTools.cmake file of a native build")
  
	include(${OMR_TOOLS_IMPORTFILE})

endif(NOT CMAKE_CROSSCOMPILING)


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

#message(STATUS $<PLAFORM_ID>)
#TODO: this still builds in tree
#TODO: output can probably be determined from input
macro(add_tracegen input)
	add_custom_command(
		OUTPUT ${ARGN}
		COMMAND $<TARGET_FILE:tracegen> -treatWarningAsError -generatecfiles -threshold 1 -file ${input}
		DEPENDS ${input}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
endmacro()
#TODO we still dont do tracemerge properly
