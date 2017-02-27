add_executable(omrgctest
	GCConfigObjectTable.cpp
	GCConfigTest.cpp
	gcTestHelpers.cpp
	main.cpp
	StartupManagerTestExample.cpp
)

#TODO this is a real gross, tangled mess
target_link_libraries(omrgctest
	omrGtestGlue
	pugixml
	omrtestutil
	omrcore
	omrglue
	omrgc
	omrglue
	omrcore
	omrtrace
	j9hookstatic
	omrport
	j9thrstatic
	j9hashtable
	omrutil
	omrglue
)
#	./omrgctest -configListFile=fvtest/gctest/configuration/fvConfigListFile.txt
#ifeq (linux,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += rt pthread
#endif
#ifeq (osx,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += iconv pthread
#endif
#ifeq (aix,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += iconv perfstat
#endif
#ifeq (win,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += ws2_32 shell32 Iphlpapi psapi pdh
#endif

