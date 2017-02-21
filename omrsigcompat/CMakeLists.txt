add_library(omrsig SHARED
	omrsig.cpp
)

#TODO need more comrehensive symbols
set(exportedSyms
	omrsig_primary_signal
	omrsig_handler
	signal
)

#for now only export symbols on windows
if(OMR_HOST_OS STREQUAL "win")
	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/omrsig.exportlist" "EXPORTS\n" )
	foreach(exportSymbol ${exportedSyms})
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/omrsig.exportlist" "${exportSymbol}\n" )
	endforeach()
	set_target_properties(omrsig
		PROPERTIES LINK_FLAGS "-def:${CMAKE_CURRENT_BINARY_DIR}/omrsig.exportlist")
endif()
#EXPORT_FUNCTIONS_FILE := omrsig.exportlist

### Create platform specific exports

#$(EXPORT_FUNCTIONS_FILE):
	#@# all
	#@echo omrsig_primary_signal >>$@
	#@echo omrsig_handler >>$@
	#@echo signal >>$@
#ifeq (,$(findstring win,$(OMR_HOST_OS)))
	#@# !windows
	#@echo omrsig_primary_sigaction >>$@
	#@echo sigaction >>$@
	#@echo sigset >>$@
	#@echo sigignore >>$@
	#@echo bsd_signal >>$@
	#@echo sysv_signal >>$@
#endif
#ifneq (,$(findstring linux,$(OMR_HOST_OS)))
	#@# linux
	#@echo __sysv_signal >>$@
	#@echo ssignal >>$@
#endif
#ifneq (,$(findstring osx,$(OMR_HOST_OS)))
	#@# osx
	#@echo __sysv_signal >>$@
	#@echo ssignal >>$@
#endif
#ifneq (,$(findstring zos,$(OMR_HOST_OS)))
	#@# z/os
	#@echo __sigactionset >>$@
#endif
#ifeq (,$(findstring win,$(OMR_HOST_OS)))
#ifeq (,$(findstring zos,$(OMR_HOST_OS)))
	#@# !win && !zos
	#@echo sigvec >>$@
#endif
#endif

#MODULE_STATIC_LIBS += omrutil
#ifneq (,$(findstring linux,$(OMR_HOST_OS)))
    #MODULE_SHARED_LIBS += pthread
#endif
#ifneq (,$(findstring osx,$(OMR_HOST_OS)))
    #MODULE_SHARED_LIBS += pthread
#endif

