add_library(omrtrace STATIC
	omrtraceapi.cpp
	omrtracecomponent.cpp
	omrtraceformatter.cpp
	omrtracelog.cpp
	omrtracemain.cpp
	omrtracemisc.cpp
	omrtraceoptions.cpp
	omrtracepublish.cpp
	omrtracewrappers.cpp
)

#ifeq (gcc,$(OMR_TOOLCHAIN))
## disable builtin strncpy buffer length check for optimized builds (_FORTIFY_SOURCE=1 requires -O3)
  #ifeq ($(OMR_OPTIMIZE),1)
    #MODULE_CXXFLAGS+=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
  #endif
#endif
