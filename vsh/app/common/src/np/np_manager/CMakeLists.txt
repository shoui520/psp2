# vs0:sys/external/np_manager - SceNpManager, built with SNC in C++. The NP service state
# client is Sony's impsutility/np_imps_utility_for_np_infra.cpp.
enable_language(CXX)
psp2_module(vs0_sys_external_np_manager KIND user COMPILER snc SOURCES
	np_manager.cpp
	../impsutility/np_imps_utility_for_np_infra.cpp
	np_manager_ipc_client.cpp
	np_manager_ipc_stub.cpp
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
