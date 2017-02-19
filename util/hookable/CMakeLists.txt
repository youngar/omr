#TODO there is a bunch of stuff with vpaths, presumably for some extensibility reasons
add_library(j9hookstatic STATIC
	hookable.cpp
)
target_include_directories(j9hookstatic
	PUBLIC
		.
)

