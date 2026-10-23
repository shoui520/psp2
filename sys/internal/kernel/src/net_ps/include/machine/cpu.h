/*
 * SCE: the psp2 port's CPU definitions: one CPU, whose current lwp the headers name
 * (sys/lwp.h, sys/proc.h); none of arm32's kernel CPU support.
 */
#ifndef _PSP2_CPU_H_
#define _PSP2_CPU_H_

struct lwp;

struct cpu_info {
	struct lwp *ci_curlwp;
};

extern struct cpu_info cpu_info_store;

#define curcpu()	(&cpu_info_store)
#define cpu_number()	0

#endif /* _PSP2_CPU_H_ */
