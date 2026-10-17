# SceTeleportServer (vs0:sys/external/libSceTeleportServer.suprx), built by
# Sony with SNC; unwind_pr*.c are the SNC runtime's EHABI personality routines.
psp2_module(vs0_sys_external_libSceTeleportServer KIND user SOURCES
	teleport_server.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr2.c
)
