# vs0:sys/external/libipmi_nongame - SceIpmiNonGameApp, built with SNC.
# Dynamic-library build of Sony's libipmi (vsh/lowlayer/src/ipmi) for non-game apps.
set(ipmi_source ../ipmi)
enable_language(CXX)
psp2_module(vs0_sys_external_libipmi_nongame KIND user COMPILER snc SOURCES
	libipmi_nongame.c
	${ipmi_source}/client.cpp
	${ipmi_source}/client_impl.cpp
	${ipmi_source}/server.cpp
	${ipmi_source}/server_impl.cpp
	${ipmi_source}/session.cpp
	${ipmi_source}/session_impl.cpp
	${ipmi_source}/psp2/event_flag.cpp
	${ipmi_source}/psp2/msg_pipe.cpp
	${ipmi_source}/psp2/mutex.cpp
	${ipmi_source}/psp2/semaphore.cpp
	../shell_svc/allocator.cpp
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
