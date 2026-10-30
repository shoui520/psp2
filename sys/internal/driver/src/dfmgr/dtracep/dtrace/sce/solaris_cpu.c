/*
 * Solaris CPU setup callbacks (Sony's dtrace/sce/solaris_cpu.c): the table of
 * functions called when a CPU's configuration changes.
 */
#include <dtrace/sce/solaris_cpu.h>
#include <sysmem/debug.h>

#define NCPU_SETUPS	20

cpu_t *cpu_list;
cpu_core_t *cpu_core;
kmutex_t cpu_lock;
int max_ncpus = NCPU;
int boot_max_ncpus = -1;

static struct cpu_setup_func {
	cpu_setup_func_t *func;
	void *arg;
} cpu_setup_func_list[NCPU_SETUPS];

void register_cpu_setup_func(cpu_setup_func_t *func, void *arg)
{
	int i;

	for (i = 0; i < NCPU_SETUPS; i++) {
		if (cpu_setup_func_list[i].func == NULL)
			break;
	}
	if (i >= NCPU_SETUPS)
		SCE_PANIC_AT(65);

	cpu_setup_func_list[i].func = func;
	cpu_setup_func_list[i].arg = arg;
}

void unregister_cpu_setup_func(cpu_setup_func_t *func, void *arg)
{
	int i;

	for (i = 0; i < NCPU_SETUPS; i++) {
		if (cpu_setup_func_list[i].func == func &&
		    cpu_setup_func_list[i].arg == arg) {
			cpu_setup_func_list[i].func = NULL;
			cpu_setup_func_list[i].arg = NULL;
			return;
		}
	}

	SCE_PANIC_AT(85);
}
