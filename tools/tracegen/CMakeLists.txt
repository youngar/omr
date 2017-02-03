add_executable(tracegen
	main.cpp
)

target_link_libraries(tracegen
	PRIVATE
		trace # static
)

# TODO: Move libtrace to it's own home
# NOTE: trace is static so we don't futz with RPATHs to run the tools.
add_library(trace STATIC
	ArgParser.cpp
	ArgParser.hpp
	CFileWriter.cpp
	CFileWriter.hpp
	DATFileWriter.cpp
	DATFileWriter.hpp
	EventTypes.hpp
	FileReader.cpp
	FileReader.hpp
	FileUtils.cpp
	FileUtils.hpp
	Port.cpp
	Port.hpp
	StringUtils.cpp
	StringUtils.hpp
	TDFParser.cpp
	TDFParser.hpp
	TDFTypes.hpp
	TraceGen.cpp
	TraceGen.hpp
	TraceHeaderWriter.cpp
	TraceHeaderWriter.hpp
)

target_include_directories(trace
	INTERFACE
		./
)