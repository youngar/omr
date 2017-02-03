add_library(omrglue
	ArrayletObjectModel.hpp
	CollectorLanguageInterfaceImpl.cpp
	CollectorLanguageInterfaceImpl.hpp
	CompactSchemeFixupObject.cpp
	CompactSchemeFixupObject.hpp
	ConfigurationLanguageInterfaceImpl.cpp
	ConfigurationLanguageInterfaceImpl.hpp
	ContractslotScanner.hpp
	EnvironmentLanguageInterfaceImpl.cpp
	EnvironmentLanguageInterfaceImpl.hpp
	FrequentObjectsStats.cpp
	FrequentObjectsStats.hpp
	HeapRegionDescriptorTypes.hpp
	LanguageSegregatedAllocationCache.hpp
	LanguageThreadLocalHeap.hpp
	MixedObjectModel.hpp
	ObjectIterator.cpp
	ObjectIterator.hpp
	ObjectModel.cpp
	ObjectModel.hpp
	ObjectScannerState.hpp
	StartupManagerImpl.cpp
	StartupManagerImpl.hpp
	VerboseManagerImpl.cpp
	VerboseManagerImpl.hpp
)

target_include_directories(omrglue PUBLIC .)
target_link_libraries(omrglue PUBLIC omrgc)
