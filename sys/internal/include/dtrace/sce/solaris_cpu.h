/*
 * Solaris CPUs (Sony's dtrace/sce/solaris_cpu.c): the four Cortex-A9 cores,
 * a cpu_t for each in cpu_list and DTrace's per-CPU state in cpu_core, both
 * indexed by the core ID from MPIDR.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <dtrace/sce/solaris_mutex.h>
#include <dtrace/sce/solaris_rwlock.h>
#include <dtrace/sce/dtrace_cpu.h>

#define NCPU		4

/* cpuc_dtrace_flags */
#define CPU_DTRACE_NOFAULT	0x0001	/* don't fault */
#define CPU_DTRACE_DROP		0x0002	/* drop this ECB */
#define CPU_DTRACE_BADADDR	0x0004	/* DTrace fault: bad address */
#define CPU_DTRACE_BADALIGN	0x0008	/* DTrace fault: bad alignment */
#define CPU_DTRACE_DIVZERO	0x0010	/* DTrace fault: divide by zero */
#define CPU_DTRACE_ILLOP	0x0020	/* DTrace fault: illegal operation */
#define CPU_DTRACE_NOSCRATCH	0x0040	/* DTrace fault: out of scratch */
#define CPU_DTRACE_KPRIV	0x0080	/* DTrace fault: bad kernel access */
#define CPU_DTRACE_UPRIV	0x0100	/* DTrace fault: bad user access */
#define CPU_DTRACE_TUPOFLOW	0x0200	/* DTrace fault: tuple stack overflow */
#define CPU_DTRACE_ENTRY	0x0800	/* pid provider hint to ustack() */
#define CPU_DTRACE_BADSTACK	0x1000	/* DTrace fault: bad stack */

#define CPU_DTRACE_FAULT	(CPU_DTRACE_BADADDR | CPU_DTRACE_BADALIGN | \
				CPU_DTRACE_DIVZERO | CPU_DTRACE_ILLOP | \
				CPU_DTRACE_NOSCRATCH | CPU_DTRACE_KPRIV | \
				CPU_DTRACE_UPRIV | CPU_DTRACE_TUPOFLOW | \
				CPU_DTRACE_BADSTACK)
#define CPU_DTRACE_ERROR	(CPU_DTRACE_FAULT | CPU_DTRACE_DROP)

typedef struct cpu_core {
	uintptr_t cpuc_dtrace_illval;	/* DTrace illegal value */
	kmutex_t cpuc_pid_lock;		/* DTrace pid provider lock */
	uint16_t cpuc_dtrace_flags;	/* DTrace flags */
	uint8_t cpuc_pad[14];
} cpu_core_t;

typedef struct cpu {
	processorid_t cpu_id;
	struct cpu *cpu_next;		/* next CPU in the ring */
	krwlock_t cpu_ft_lock;		/* DTrace: fasttrap lock */
	hrtime_t cpu_dtrace_chillmark;	/* DTrace: chill mark time */
	hrtime_t cpu_dtrace_chilled;	/* DTrace: total chill time */
	uintptr_t cpu_profile_pc;	/* kernel PC in profile interrupt */
	uintptr_t cpu_profile_upc;	/* user PC in profile interrupt */
} cpu_t;

typedef enum {
	CPU_INIT,
	CPU_CONFIG,
	CPU_UNCONFIG,
	CPU_ON,
	CPU_OFF,
	CPU_CPUPART_IN,
	CPU_CPUPART_OUT
} cpu_setup_t;

typedef int cpu_setup_func_t(cpu_setup_t what, int id, void *arg);

extern cpu_t *cpu_list;
extern cpu_core_t *cpu_core;
extern kmutex_t cpu_lock;
extern int max_ncpus;
extern int boot_max_ncpus;

void register_cpu_setup_func(cpu_setup_func_t *func, void *arg);
void unregister_cpu_setup_func(cpu_setup_func_t *func, void *arg);

#define CPU		(&cpu_list[dtrace_processor_id()])
#define CPU_ON_INTR(cpu)	dtrace_is_interrupt_context()
