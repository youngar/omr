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
