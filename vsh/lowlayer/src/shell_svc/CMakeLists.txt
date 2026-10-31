# vs0:sys/external/libshellsvc - SceShellSvc, built with SNC. It links Sony's libipmi
# (vsh/lowlayer/src/ipmi, blocking_ipc, ipmi_wrapper); unconverted exports remain in
# libshellsvc.c.
set(ipmi_source ../ipmi)
enable_language(CXX)
psp2_module(vs0_sys_external_libshellsvc KIND user COMPILER snc SOURCES
	libshellsvc.c
	../blocking_ipc/blocking_ipc_client.cpp
	../blocking_ipc/blocking_ipc_server.cpp
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
	../ipmi_wrapper/ipmi_wrapper_client.cpp
	../ipmi_wrapper/ipmi_wrapper_server.cpp
	shell_util.cpp
	shell_util_ipc_client.cpp
	text_clipboard.cpp
	svc_ctrl.cpp
	allocator.cpp
	shell_svc_client.cpp
	shell_svc_ipc_client.cpp
)
