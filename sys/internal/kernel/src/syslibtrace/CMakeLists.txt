# os0:kd/syslibtrace.skprx - syscall tracing of exported libraries, NID symbol table
psp2_module(os0_kd_syslibtrace KIND kernel SOURCES
	source/syslibtrace_syscall.c
	source/syslibtrace_module.c
	source/syslibtrace_trampoline.S
	source/nidsymtbl.c
)
