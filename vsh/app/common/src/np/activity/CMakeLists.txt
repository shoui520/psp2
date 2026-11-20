enable_language(CXX)
# SceNpActivityNet (vs0:sys/external/np_activity.suprx), built by Sony with SNC 3.57 in C++: the
# activity feed's network requests. Its files are Sony's where the library's checks name them
# (W:\Build\J01509657\vsh\app\common\src\np\activity\...).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	np_activity.cpp
	abstract_context_manager.cpp
	context_manager.cpp
	common/activity_object.cpp
	common/activity_lock_manager.cpp
	common/activity_memory.cpp
	common/activity_mutex.cpp
	common/activity_simple_lock.cpp
	component/abstract_component.cpp
	component/auth/component_auth.cpp
	component/context/component_context.cpp
	component/http/component_http.cpp
	component/command_executor/component_command_executor.cpp
	component/utility/component_utility.cpp
	common/activity_xml_writer.cpp
	common/activity_debug.cpp
)
