add_library(omrglue STATIC
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
	LanguageVMGlue.c
	MixedObjectModel.hpp
	MixedObjectScanner.hpp
	ObjectAllocationModel.hpp
	ObjectIterator.cpp
	ObjectIterator.hpp
	ObjectModel.hpp
	ObjectModelDelegate.cpp
	ObjectModelDelegate.hpp
	ObjectScannerState.hpp
	Profiling.c
	ScavengerBackOutScanner.hpp
	ScavengerRootScanner.hpp
	StartupManagerImpl.cpp
	StartupManagerImpl.hpp
	UtilGlue.c
	VerboseManagerImpl.cpp
	VerboseManagerImpl.hpp
	omrExampleVM.cpp
	omrExampleVM.hpp
)

target_include_directories(omrglue PUBLIC . ..)
target_link_libraries(omrglue PRIVATE omrgc)
