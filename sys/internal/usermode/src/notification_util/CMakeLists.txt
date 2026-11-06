enable_language(CXX)
# SceNotificationUtil (vs0:sys/external/notification_util.suprx), built by Sony with SNC 3.57 in
# C++ (the shell's IPMI client)
psp2_module(vs0_sys_external_notification_util KIND user COMPILER snc SOURCES
	notification_util.cpp
	notification_util_ipc_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
