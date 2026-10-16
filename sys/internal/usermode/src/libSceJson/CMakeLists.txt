# SceLibJson (vs0:sys/external/libSceJson.suprx): sce::Json, C++ built by Sony
# with SNC 3.57.  The library is linked statically into other modules too, so
# the sources live in common/json/.
enable_language(CXX)
psp2_module(vs0_sys_external_libSceJson KIND user SOURCES
	../../../../../common/json/json_libc.cpp
	../../../../../common/json/json_runtime.cpp
	../../../../../common/json/json_value.cpp
	../../../../../common/json/json_container.cpp
	../../../../../common/runtime/unwind_pr0.c
	../../../../../common/runtime/unwind_pr1.c
)

if(TARGET vs0_sys_external_libSceJson)
	# SNC enums are 32 bits (Value::m_type is a word); GCC for the Vita uses short enums
	target_compile_options(vs0_sys_external_libSceJson PRIVATE -fno-short-enums)
	# SNC keeps the stores destructors make (Sony poisons freed objects with -1)
	# and tests what operator new returns before running a constructor
	foreach(f -fcheck-new -fno-lifetime-dse -fno-exceptions -fno-rtti -fno-sized-deallocation -fno-threadsafe-statics -fno-use-cxa-atexit)
		target_compile_options(vs0_sys_external_libSceJson PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${f}>)
	endforeach()
endif()
