/*
 * Solaris cyclics (Sony's dtrace/sce/solaris_cyclic.c): a low-level cyclic
 * runs on a thread (solaris_cyclic_low.c), a high-level one on a CPU's
 * timer interrupt (solaris_cyclic_high.c); an omni cyclic is one per CPU.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

typedef void (*cyc_func_t)(void *arg);
typedef uint16_t cyc_level_t;

#define CY_LOW_LEVEL	0
#define CY_LOCK_LEVEL	1
#define CY_HIGH_LEVEL	2
#define CY_SOFT_LEVELS	2
#define CY_LEVELS	3

typedef struct cyc_handler {
	cyc_func_t cyh_func;
	void *cyh_arg;
	cyc_level_t cyh_level;
} cyc_handler_t;

typedef struct cyc_time {
	hrtime_t cyt_when;
	hrtime_t cyt_interval;
} cyc_time_t;

typedef struct cyc_omni_handler {
	void (*cyo_online)(void *arg, processorid_t cpu, cyc_handler_t *hdlr,
	    cyc_time_t *when);
	void (*cyo_offline)(void *arg, processorid_t cpu, void *oarg);
	void *cyo_arg;
} cyc_omni_handler_t;

/* a cyclic: its handler, on a thread manager timer (low) or a system timer (high) */
struct cyclic {
	cyc_handler_t cy_handler;
	cyc_time_t cy_when;
	uint32_t cy_cpumask;		/* the CPUs it runs on */
	SceUID cy_timer;		/* low: thread manager timer; high: system timer */
	SceUID cy_thread;		/* low: the thread that runs the handler */
	SceUID cy_eventflag;		/* low: tells the thread to stop */
};
typedef struct cyclic *cyclic_id_t;

/* an omni cyclic: one cyclic per CPU, and the omni handler they came from */
typedef struct cyclic_id_list {
	cyclic_id_t cyl_cyclic[4];
	cyclic_id_t cyl_high;		/* runs the high-level ones */
	cyc_omni_handler_t cyl_omni;
} *cyclic_id_list_t;

#define CYCLIC_NONE	((cyclic_id_t)0)

cyclic_id_t cyclic_add(cyc_handler_t *hdlr, cyc_time_t *when);
void cyclic_remove(cyclic_id_t id);
cyclic_id_list_t cyclic_add_omni(cyc_omni_handler_t *omni);
void cyclic_remove_omni(cyclic_id_list_t list);

/* solaris_cyclic_low.c */
cyclic_id_t _cyclic_low_alloc(cyc_handler_t *hdlr, cyc_time_t *when, uint32_t cpumask);
void _cyclic_low_free(cyclic_id_t id);
int _cyclic_low_init(cyclic_id_t id);
int _cyclic_low_start(cyclic_id_t id);
int _cyclic_low_stop(cyclic_id_t id);
void _cyclic_low_remove(cyclic_id_t id);

/* solaris_cyclic_high.c */
void _dtrace_heap_free_later(void *ptr);
cyclic_id_t _cyclic_high_alloc(cyc_handler_t *hdlr, cyc_time_t *when, uint32_t cpumask);
void _cyclic_high_free(cyclic_id_t id);
int _cyclic_high_init(cyclic_id_t id);
int _cyclic_high_start(cyclic_id_t id);
int _cyclic_high_stop(cyclic_id_t id);
void _cyclic_high_remove(cyclic_id_t id);
int sceDtracepCyclicHighSuspend(void);
int sceDtracepCyclicHighResume(void);
