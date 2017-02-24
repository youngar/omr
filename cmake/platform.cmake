

###
### Platform flags
### TODO: arch flags. Defaulting to x86-64

message(STATUS "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM}")
message(STATUS "CMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")

#set(OMR_ARCH_POWER Off Internal "Power CPU Architecture")
#set(OMR_ARCH_ARM Off Internal "Arm CPU Architecture")
#set(OMR_ARCH_X86 Off Internal "TODO: Document")
#set(OMR_ARCH_S390 Off Interal "TODO: Document")

#set(OMR_ENV_DATA64 Off Internal "System words are 8 octets wide")
#set(OMR_ENV_DATA32 Off Internal "System words are 4 octets wide")

# TODO: Support all system types known in OMR
# TODO: Is there a better way to do system flags?

if((CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64") OR (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64"))
	set(OMR_ARCH_X86 ON)
	set(OMR_ENV_DATA64 ON)
	set(OMR_TARGET_DATASIZE 64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
	set(OMR_ARCH_X86 ON)
	set(OMR_ENV_DATA32 ON)
	set(OMR_TARGET_DATASIZE 32)
else()
	message(FATAL_ERROR "Unknown processor: ${CMAKE_PROCESSOR}")
endif()

#TODO: detect other Operating systems (aix, and zos)
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(OMR_HOST_OS "linux")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(OMR_HOST_OS "osx")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set(OMR_HOST_OS "win")
else()
	message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()


if(OMR_HOST_OS STREQUAL "linux")
	add_definitions(
		-DLINUX
		-D_REENTRANT
		-D_FILE_OFFSET_BITS=64
	)
endif()


if(OMR_ARCH_X86)
	set(OMR_ENV_LITTLE_ENDIAN ON CACHE BOOL "TODO: Document")
	if(OMR_ENV_DATA64)
		add_definitions(-DJ9HAMMER)
	else()
		add_definitions(-DJ9X86)
	endif()
endif()

if(OMR_HOST_OS STREQUAL "osx")
	add_definitions(
		-DOSX
		-D_REENTRANT
		-D_FILE_OFFSET_BITS=64
	)
endif()


if(OMR_HOST_OS STREQUAL "win")
	#TODO: should probably check for MSVC
	set(OMR_WINVER "0x501")

	add_definitions(
		-DWIN32
		-D_CRT_SECURE_NO_WARNINGS
		-DCRTAPI1=_cdecl
		-DCRTAPI2=_cdecl
		-D_WIN_95
		-D_WIN32_WINDOWS=0x0500
		-D_WIN32_DCOM
		-D_MT
		-D_WINSOCKAPI_
		-D_WIN32_WINVER=${OMR_WINVER}
		-D_WIN32_WINNT=${OMR_WINVER}
		-D_DLL
	)
	if(OMR_ENV_DATA64)
		add_definitions(
			-DWIN64
			-D_AMD64_=1
		)
		set(TARGET_MACHINE AMD64)
	else()
		add_definitions(
			-D_X86_
		)
		set(TARGET_MACHINE i386)
	endif()
	set(opt_flags "/GS-")
	set(common_flags "-MD -Zm400")

	set(linker_common "-subsystem:console -machine:${TARGET_MACHINE}")
	if(NOT OMR_ENV_DATA64)
		set(linker_common "${linker_common} /SAFESEH")
	endif()

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${linker_common} /INCREMENTAL:NO /NOLOGO /LARGEADDRESSAWARE wsetargv.obj")
	if(OMR_ENV_DATA64)
		#TODO: makefile has this but it seems to break shit
		#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
	endif()

	set(CMAKE_SHARED_LIKER_FLAGS "${CMAKE_SHARED_LIKER_FLAGS} /INCREMENTAL:NO /NOLOGO")
	if(OMR_ENV_DATA64)
		set(CMAKE_SHARED_LIKER_FLAGS "${CMAKE_SHARED_LIKER_FLAGS} -entry:_DllMainCRTStartup")
	else()
		set(CMAKE_SHARED_LIKER_FLAGS "${CMAKE_SHARED_LIKER_FLAGS} -entry:_DllMainCRTStartup@12")
	endif()
	#set(CMAKE_C_FLAGS "${common_flags}")
	#set(CMAKE_CXX_FLAGS "${common_flags}")


	#string(REPLACE "/EHSC" "" filtered_c_flags ${CMAKE_C_FLAGS})
	string(REPLACE "/EHsc" "" filtered_cxx_flags ${CMAKE_CXX_FLAGS})
	string(REPLACE "/GR" "" filtered_cxx_flags ${filtered_cxx_flags})
	set(CMAKE_CXX_FLAGS "${filtered_cxx_flags} ${common_flags}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${common_flags}")

	message(STATUS "CFLAGS = ${CMAKE_C_FLAGS}")
	message(STATUS "CXXFLAGS = ${CMAKE_CXX_FLAGS}")

	#Hack up output dir to fix dll dependency issues on windows
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
###
### Flags we aren't using
###

# TODO: SPEC

# TODO: OMR_HOST_ARCH
# TODO: OMR_TARGET_DATASIZE
# TODO: OMR_TOOLCHAIN
# TODO: OMR_CROSS_CONFIGURE
# TODO: OMR_RTTI

