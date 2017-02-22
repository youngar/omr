#TODO: at the moment this assumes all C source files which is a lie
include_directories(
	.
	../nls
)

add_definitions(-DOMRPORT_LIBRARY_DEFINE)

set(OBJECTS "")
set(VPATH "")

if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND OBJECTS omrgetsp.c)
endif()


if(OMR_HOST_OS STREQUAL "zos")
	#TODO these need source file extensions
	list(APPEND OBJECTS
		j9generate_ieat_dump
		omrget_large_pageable_pages_supported
		j9wto
		j9pgser_release
		omrgetuserid
		j9sysinfo_get_number_CPUs
		j9jobname
		j9userid
		j9csrsi
		j9csrsi_wrp
		j9zfs
		j9lpdat)
	#TODO more objects here. See port_objects.mk:55
	if(OMR_ENV_DATA64)
		list(APPEND OBJECTS
			omrget_userExtendedPrivateAreaMemoryType.c
			omrget_large_pages_supported.c
			omrget_large_2gb_pages_supported.c
			omrvmem_support_above_bar.c
			omrvmem_support_below_bar_64.c
			j9ipt_ttoken64.c
		)
	else()
		list(APPEND OBJECTS omrvmem_support_below_bar_31.c)
	endif()
else()
	list(APPEND OBJECTS protect_helpers.c)
endif()

list(APPEND OBJECTS
	omrgetjobname.c
	omrgetjobid.c
	omrgetasid.c
)

#TODO omrtime stuff port_objects.mk:75

#TODO another if block @port_objects.mk:89

if(OMR_HOST_OS STREQUAL "aix")
	list(APPEND OBJECTS 
		rt_divu64.c
		rt_time.c
	)
endif()

list(APPEND OBJECTS 
	omrcpu.c
	omrerror.c
	omrerrorhelpers.c
	omrexit.c
	omrfile.c
	omrfiletext.c
	omrfilestream.c
	omrfilestreamtext.c
)

if(NOT OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS omriconvhelpers.c)
endif()

list(APPEND OBJECTS omrfile_blockingasync.c)

if(OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS omrfilehelpers.c)
endif()

if(OMR_ENV_DATA64)
	list(APPEND OBJECTS omrmem32helpers.c)
endif()

list(APPEND OBJECTS 
	omrheap.c
	omrmem.c
	omrmemtag.c
	omrmemcategories.c
	omrport.c
	omrmmap.c
	j9nls.c
	j9nlshelpers.c
	omrosbacktrace.c
	omrosbacktrace_impl.c
	omrintrospect.c
	omrintrospect_common.c
	omrosdump.c
	omrportcontrol.c
	omrportptb.c
	
	omrsignal.c
)

if(NOT OMR_HOST_OS STREQUAL "win")
	list(APPEND OBJECTS omrsignal_context.c)
endif()

list(APPEND OBJECTS
	omrsl.c
	omrstr.c
	omrsysinfo.c
)

if(OMR_HOST_OS STREQUAL zos)
	list(APPEND OBJECTS omrsysinfo_helpers.c)
endif()

list(APPEND OBJECTS omrsyslog.c)

if(OMR_HOST_OS STREQUAL win)
    #TODO hack to compile mc files
    enable_language(RC)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/win32/omrsyslogmessages.rc
        COMMAND mc  ${CMAKE_CURRENT_SOURCE_DIR}/win32/omrsyslogmessages.mc
        DEPENDS win32/omrsyslogmessages.mc
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/win32
    )
	#TODO need to handle res files properly
	#list(APPEND OBJECTS omrsyslogmessages.res)
endif()

list(APPEND OBJECTS
	omrtime.c 
	omrtlshelpers.c
	omrtty.c
	omrvmem.c
	omrmemtag_checks.c
)

if(OMR_HOST_OS STREQUAL aix)
	list(APPEND OBJECTS omrosdump_helpers.c)
elseif(OMR_HOST_OS STREQUAL linux)
	list(APPEND OBJECTS omrosdump_helpers.c)
elseif(OMR_HOST_OS STREQUAL osx)
	list(APPEND OBJECTS omrosdump_helpers.c)
endif()

if(OMR_HOST_OS STREQUAL zos)
	if(NOT OMR_ENV_DATA64)
		list(APPEND OBJECTS 
			omrsignal_ceehdlr.c
			omrsignal_context_ceehdlr.c
		)
	endif()
endif()

if(OMR_HOST_ARCH STREQUAL ppc)
	if(OMR_HOST_OS STREQUAL linux)
		list(APPEND OBJECTS auxv.c)
	endif()
endif()

if(OMR_OPT_CUDA)
	list(APPEND OBJECTS omrcuda.c)
endif()




#Setup paths and include directories

if(OMR_HOST_OS STREQUAL "zos")
	list(APPEND VPATH zos390)
	include_directories(zos390)
endif()

if(OMR_HOST_OS STREQUAL "win")
	if(OMR_ENV_DATA64)
		list(APPEND VPATH win64amd)
		include_directories(win64amd)
	endif()
endif()

#TODO other platforms port_objects:209

if(OMR_HOST_OS STREQUAL "linux")
	#TODO other archs (line 228)
	
	if(OMR_ARCH_X86)
		if(OMR_ENV_DATA64)
			list(APPEND VPATH linuxamd64)
			include_directories(linuxamd64)
		else()
			list(APPEND VPATH linux386)
			include_directories(linux386)
		endif()
		list(APPEND VPATH linux)
		include_directories(linux)
	endif()
endif()


if(OMR_HOST_OS STREQUAL "osx")
	list(APPEND VPATH osx_include osx)
	include_directories(osx_include osx)
endif()


if(OMR_HOST_OS STREQUAL "win")
	list(APPEND VPATH win32_include win32)
	include_directories(win32_include win32)
else()
	list(APPEND VPATH unix)
	include_directories(unix unix_include)
endif()

list(APPEND VPATH common include)
include_directories(common include)

#TODO need to set some flags and etc see line 300


set(resolvedPaths "")
VPathResolve(OBJECTS VPATH resolvedPaths)

#message("Resolved paths = ${resolvedPaths}")
if(OMR_HOST_OS STREQUAL "win")
    list(APPEND resolvedPaths win32/omrsyslogmessages.rc)
endif()
add_tracegen(
	common/omrport.tdf

	${CMAKE_CURRENT_SOURCE_DIR}/common/ut_omrport.c
	${CMAKE_CURRENT_SOURCE_DIR}/common/ut_omrport.h
)

add_library(omrport STATIC 
	common/ut_omrport.c
	${resolvedPaths}
)

target_link_libraries(omrport
	j9thrstatic
	j9hashtable
)

#TODO hack to get to compile. Need platform checks
if(NOT OMR_HOST_OS STREQUAL "win")
	target_link_libraries(omrport dl)
endif()

if(OMR_HOST_OS STREQUAL "osx")
  target_link_libraries(omrport iconv)
endif()

#add_executable(dummy
#	main.c
#)
#target_link_libraries(dummy omrport)
