if (NOT CMAKE_CROSSCOMPILING)

	add_subdirectory(hookgen)
	add_subdirectory(tracemerge)
	add_subdirectory(tracegen)

	export(TARGETS hookgen tracemerge tracegen FILE "ImportTools.cmake")

else(NOT CMAKE_CROSSCOMPILING)

	set(OMR_TOOLS_IMPORTFILE "IMPORTFILE-NOTFOUND" CACHE FILEPATH
		"Point it to the ImportTools.cmake file of a native build")

	#Logically we should be importing the file here, however,
	#burried in the cmake docs, it says imported targets are directory scoped
	#thus they would be useless if imported here
	#include(${OMR_TOOLS_IMPORTFILE})

endif(NOT CMAKE_CROSSCOMPILING)


function(add_hookgen input)
	#TODO: this is a hack to work arround the fact that hookgen outputs relative to sourcefile rather than to CWD
	get_filename_component(input_base "${input}" NAME)
	add_custom_command(
		OUTPUT ${ARGN}
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${input}" "${input_base}"
		COMMAND hookgen "${input_base}"
		COMMAND ${CMAKE_COMMAND} -E remove -f "${input_base}"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
endfunction()


#TODO: pehaps should detect output by searching for "executable=" line
#takes extra optional argument name to override output filename
function(add_tracegen input)
	get_filename_component(input_dir "${input}" DIRECTORY)

	if(ARGV1)
		set(base_name "${ARGV1}")
	else()
		get_filename_component(base_name "${input}" NAME_WE)
	endif()
	#file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/${input_dir}/ut_${base_name}" generated_filename)
	file(TO_CMAKE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ut_${base_name}" generated_filename)

	add_custom_command(
		OUTPUT "${generated_filename}.c" "${generated_filename}.h" "${generated_filename}.pdat"
		COMMAND $<TARGET_FILE:tracegen> -w2cd -treatWarningAsError -generatecfiles -threshold 1 -file ${CMAKE_CURRENT_SOURCE_DIR}/${input}
		DEPENDS ${input}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
endfunction()

#TODO we still dont do tracemerge properly
