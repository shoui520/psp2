# os0:kd/syslibtrace.skprx - syscall tracing of exported libraries, NID symbol table
psp2_module(os0_kd_syslibtrace KIND kernel COMPILER gcc48 SOURCES
	syslibtrace_syscall.c
	syslibtrace_module.c
	syslibtrace_trampoline.S
	nidsymtbl.c
)
