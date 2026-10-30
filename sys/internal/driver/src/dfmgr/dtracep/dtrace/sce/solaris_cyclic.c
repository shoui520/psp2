/*
 * Solaris cyclics (Sony's dtrace/sce/solaris_cyclic.c): a cyclic at low or
 * lock level runs on a thread (solaris_cyclic_low.c), one at high level on a
 * system timer's interrupt (solaris_cyclic_high.c). An omni cyclic is one
 * cyclic per CPU; its high-level ones are fired together by one system timer.
 */
#include <dtrace/logging.h>
#include <dtrace/heap.h>
#include <dtrace/sce/dtrace_cpu.h>
#include <dtrace/sce/solaris_cpu.h>
#include <dtrace/sce/solaris_cyclic.h>

static const char *const cyclic_level_names[CY_LEVELS] = { "low", "lock", "high" };

static inline const char *cyclic_level_name(cyc_level_t level)
{
	return level <= CY_HIGH_LEVEL ? cyclic_level_names[level] : "unknown";
}

/* the high-level handler of an omni cyclic: each CPU's handler, on that CPU */
static void cyclic_omni_fire(void *arg)
{
	cyclic_id_list_t list = arg;
	cyclic_id_t id;
	int cpu;

	if (list == NULL)
		return;

	for (cpu = 0; cpu < NCPU; cpu++) {
		id = list->cyl_cyclic[cpu];
		if (id != CYCLIC_NONE && id->cy_handler.cyh_func != NULL)
			dtrace_xcall(cpu, id->cy_handler.cyh_func, id->cy_handler.cyh_arg);
	}
}

cyclic_id_t cyclic_add(cyc_handler_t *hdlr, cyc_time_t *when)
{
	cyclic_id_t id;

	switch (hdlr->cyh_level) {
	case CY_LOW_LEVEL:
	case CY_LOCK_LEVEL:
		id = _cyclic_low_alloc(hdlr, when, 0xf);
		if (id != CYCLIC_NONE) {
			if (_cyclic_low_init(id) == 0) {
				_cyclic_low_start(id);
				return id;
			}
			_cyclic_low_free(id);
		}
		break;
	case CY_HIGH_LEVEL:
		id = _cyclic_high_alloc(hdlr, when, 1 << dtrace_processor_id());
		if (id != CYCLIC_NONE) {
			if (_cyclic_high_init(id) == 0) {
				_cyclic_high_start(id);
				return id;
			}
			_cyclic_high_free(id);
		}
		break;
	default:
		DTRACE_LOG_ERROR_AT(57, "%s - invalid level (%d)", __func__, hdlr->cyh_level);
		return CYCLIC_NONE;
	}

	DTRACE_LOG_ERROR_AT(62, "%s - failed to add %s cyclic", __func__,
			    cyclic_level_name(hdlr->cyh_level));
	return CYCLIC_NONE;
}

void cyclic_remove(cyclic_id_t id)
{
	if (id == CYCLIC_NONE)
		return;

	switch (id->cy_handler.cyh_level) {
	case CY_LOW_LEVEL:
	case CY_LOCK_LEVEL:
		_cyclic_low_stop(id);
		_cyclic_low_remove(id);
		break;
	case CY_HIGH_LEVEL:
		_cyclic_high_stop(id);
		_cyclic_high_remove(id);
		break;
	default:
		DTRACE_LOG_ERROR_AT(89, "%s - invalid level (%d)", __func__, id->cy_handler.cyh_level);
		break;
	}
}

cyclic_id_list_t cyclic_add_omni(cyc_omni_handler_t *omni)
{
	cyc_handler_t hdlr;
	cyc_handler_t fire;
	cyc_time_t when;
	cyclic_id_list_t list;
	cyclic_id_t id;
	int cpu;

	list = dtrace_heap_zalloc(sizeof(*list));
	if (list == NULL) {
		DTRACE_LOG_ERROR_AT(117, "failed to allocate omni cyclic wrapper");
		return NULL;
	}
	list->cyl_omni = *omni;

	for (cpu = 0; cpu < NCPU; cpu++) {
		omni->cyo_online(omni->cyo_arg, cpu, &hdlr, &when);
		if (hdlr.cyh_level == CY_HIGH_LEVEL) {
			list->cyl_cyclic[cpu] = _cyclic_high_alloc(&hdlr, &when, 1 << cpu);
			if (cpu == 0) {
				fire.cyh_func = cyclic_omni_fire;
				fire.cyh_arg = list;
				fire.cyh_level = CY_HIGH_LEVEL;
				list->cyl_high = _cyclic_high_alloc(&fire, &when, 0xf);
				if (_cyclic_high_init(list->cyl_high) != 0) {
					_cyclic_high_free(list->cyl_high);
					list->cyl_high = CYCLIC_NONE;
				}
			}
		} else if (hdlr.cyh_level <= CY_LOCK_LEVEL) {
			list->cyl_cyclic[cpu] = _cyclic_low_alloc(&hdlr, &when, 1 << cpu);
			if (_cyclic_low_init(list->cyl_cyclic[cpu]) != 0) {
				_cyclic_low_free(list->cyl_cyclic[cpu]);
				list->cyl_cyclic[cpu] = CYCLIC_NONE;
			}
		} else {
			DTRACE_LOG_ERROR_AT(162, "%s - invalid level (%d)", __func__, hdlr.cyh_level);
			list->cyl_cyclic[cpu] = CYCLIC_NONE;
		}
		if (list->cyl_cyclic[cpu] == CYCLIC_NONE)
			DTRACE_LOG_ERROR_AT(167, "failed to add %s cyclic for CPU%d in omni cyclic %p",
					    cyclic_level_name(hdlr.cyh_level), cpu, list);
	}

	for (cpu = 0; cpu < NCPU; cpu++) {
		id = list->cyl_cyclic[cpu];
		if (id == CYCLIC_NONE)
			continue;

		if (id->cy_handler.cyh_level == CY_HIGH_LEVEL) {
			if (cpu == 0 && list->cyl_high != CYCLIC_NONE)
				_cyclic_high_start(list->cyl_high);
		} else if (id->cy_handler.cyh_level <= CY_LOCK_LEVEL) {
			_cyclic_low_start(id);
		} else {
			DTRACE_LOG_ERROR_AT(196, "%s - invalid level (%d)", __func__,
					    id->cy_handler.cyh_level);
		}
	}

	return list;
}

void cyclic_remove_omni(cyclic_id_list_t list)
{
	cyclic_id_t id;
	void *arg;
	int cpu;

	if (list == NULL)
		return;

	for (cpu = 0; cpu < NCPU; cpu++) {
		id = list->cyl_cyclic[cpu];
		if (id == CYCLIC_NONE)
			continue;

		if (id->cy_handler.cyh_level == CY_HIGH_LEVEL) {
			if (cpu == 0 && list->cyl_high != CYCLIC_NONE)
				_cyclic_high_stop(list->cyl_high);
		} else if (id->cy_handler.cyh_level <= CY_LOCK_LEVEL) {
			_cyclic_low_stop(id);
		} else {
			DTRACE_LOG_ERROR_AT(230, "%s - invalid level (%d)", __func__,
					    id->cy_handler.cyh_level);
		}
	}

	for (cpu = 0; cpu < NCPU; cpu++) {
		id = list->cyl_cyclic[cpu];
		if (id == CYCLIC_NONE)
			continue;

		arg = id->cy_handler.cyh_arg;
		id->cy_handler.cyh_arg = NULL;
		_dtrace_heap_free_later(arg);

		if (id->cy_handler.cyh_level == CY_HIGH_LEVEL) {
			if (cpu == 0 && list->cyl_high != CYCLIC_NONE)
				_cyclic_high_remove(list->cyl_high);
			_cyclic_high_remove(list->cyl_cyclic[cpu]);
		} else if (id->cy_handler.cyh_level <= CY_LOCK_LEVEL) {
			_cyclic_low_remove(list->cyl_cyclic[cpu]);
		} else {
			DTRACE_LOG_ERROR_AT(256, "%s - invalid level (%d)", __func__,
					    id->cy_handler.cyh_level);
		}
		list->cyl_cyclic[cpu] = CYCLIC_NONE;
	}

	_dtrace_heap_free_later(list);
}
