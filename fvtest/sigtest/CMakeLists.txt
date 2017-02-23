add_executable(omrsigtest
	sigTest.cpp
	sigTestHelpers.cpp
	main.cpp
)

#TODO: hack for windows. These libraries should be linked on dependants
if(OMR_HOST_OS STREQUAL "win")
	target_link_libraries(omrsigtest
		ws2_32
		shell32
		Iphlpapi
		psapi
		pdh
	)
endif()

#TODO pretty sure omrtestutil only used for headers
target_link_libraries(omrsigtest
	omrGtestGlue
	omrtestutil
	j9thrstatic
	omrport
	omrglue
	omrsig
)


#MODULE_INCLUDES += ../util
#MODULE_INCLUDES += $(OMR_GTEST_INCLUDES)
#MODULE_CXXFLAGS += $(OMR_GTEST_CXXFLAGS)

#vpath argmain.cpp $(top_srcdir)/fvtest/omrGtestGlue

#MODULE_STATIC_LIBS += \
  #omrGtest \
  #testutil \
  #omrstatic

#MODULE_SHARED_LIBS += omrsig

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

add_test(NAME sigtest COMMAND omrsigtest)
