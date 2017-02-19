

###
### Platform flags
### TODO: arch flags. Defaulting to x86-64

message("CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM}")
message("CMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}")
message("CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")

#set(OMR_ARCH_POWER Off Internal "Power CPU Architecture")
#set(OMR_ARCH_ARM Off Internal "Arm CPU Architecture")
#set(OMR_ARCH_X86 Off Internal "TODO: Document")
#set(OMR_ARCH_S390 Off Interal "TODO: Document")

#set(OMR_ENV_DATA64 Off Internal "System words are 8 octets wide")
#set(OMR_ENV_DATA32 Off Internal "System words are 4 octets wide")

# TODO: Support all system types known in OMR
# TODO: Is there a better way to do system flags?

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
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


if(OMR_ARCH_x86 AND OMR_ENV_DATA64)
	add_definitions(-DJ9HAMMER)
endif()

if(OMR_HOST_OS STREQUAL "osx")
	add_definitions(
		-DOSX
		-D_REENTRANT
		-D_FILE_OFFSET_BITS=64
		-D_XOPEN_SOURCE
	)
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

