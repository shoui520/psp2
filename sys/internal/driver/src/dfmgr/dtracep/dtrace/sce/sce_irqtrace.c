/*
 * The interrupt provider (Sony's dtrace/sce/sce_irqtrace.c): an entry and a
 * return probe for each interrupt, fired by a hook the interrupt manager
 * calls in place of the interrupt's handler.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/heap.h>
#include <dtrace/sce/dtrace_security.h>
#include <dtrace/sce/solaris_ddi.h>
#include <dtrace/sce/solaris_kmem.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>

#define IRQTRACE_NINTR		256
#define IRQTRACE_NAME_LEN	32

typedef int (*irqtrace_handler_t)(unsigned int intrCode, void *a, void *b);

typedef struct irqtrace_probe {
	irqtrace_handler_t handler;	/* the handler the hook calls */
	dtrace_id_t entryId;
	int entryEnabled;
	dtrace_id_t returnId;
	int returnEnabled;
} irqtrace_probe_t;

static dtrace_pattr_t irqtrace_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static void irqtrace_provide(void *arg, const dtrace_probedesc_t *desc);
static void irqtrace_enable(void *arg, dtrace_id_t id, void *parg);
static void irqtrace_disable(void *arg, dtrace_id_t id, void *parg);
static void irqtrace_destroy(void *arg, dtrace_id_t id, void *parg);

static dtrace_pops_t irqtrace_pops = {
	irqtrace_provide,
	NULL,
	irqtrace_enable,
	irqtrace_disable,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	irqtrace_destroy
};

static dev_info_t *irqtrace_devi;
static dtrace_provider_id_t irqtrace_id;

static SceKernelSpinlock s_irqtraceLock;
static irqtrace_probe_t *s_irqtraceProbes;
static int s_irqtraceProvided;

static void irqtrace_destroy(void *arg, dtrace_id_t id, void *parg)
{
}

static int irqtrace_hook(unsigned int intrCode, void *a, void *b)
{
	irqtrace_probe_t *probe = &s_irqtraceProbes[intrCode];
	int res;

	if (probe->entryEnabled)
		dtrace_probe(probe->entryId, 0, 0, 0, 0, 0);
	res = probe->handler(intrCode, a, b);
	if (probe->returnEnabled)
		dtrace_probe(probe->returnId, 0, 0, 0, 0, 0);
	return res;
}

/* the interrupt manager released an interrupt's handler */
static void OnIntrMgrInterruptHandlerReleased(unsigned int intrCode)
{
	irqtrace_probe_t *probes;

	sceKernelSpinlockLowLock(&s_irqtraceLock);
	if (s_irqtraceProvided) {
		probes = s_irqtraceProbes;
		if (probes[intrCode].handler != NULL) {
			sceKernelReleaseIntrHookHandler(intrCode);
			probes[intrCode].handler = NULL;
		}
	}
	sceKernelSpinlockLowUnlock(&s_irqtraceLock);
}

/* the interrupt's name, or, outside an internal kit, its number */
static inline int irqtrace_name(unsigned int intrCode, char *name)
{
	SceKernelIntrHandlerInfo info;
	int external;

	external = dtrace_security_check_internal();
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	info.name = name;
	if (sceKernelQueryIntrHandlerInfo(intrCode, -1, &info) != 0 || info.handler == NULL)
		return 0;
	if (external)
		snprintf(name, IRQTRACE_NAME_LEN - 1, "interrupt_%d", intrCode);
	name[IRQTRACE_NAME_LEN - 1] = '\0';
	return 1;
}

static inline irqtrace_probe_t *_createIrqProbe(unsigned int intrCode, const char *name)
{
	irqtrace_probe_t *probe = &s_irqtraceProbes[intrCode];

	if (probe == NULL) {
		SCE_DBG_ASSERT(probe == NULL, 130);
		return NULL;
	}
	probe->entryId = dtrace_probe_create(irqtrace_id, NULL, name, "entry", 5, NULL);
	probe->returnId = dtrace_probe_create(irqtrace_id, NULL, name, "return", 5, NULL);
	return probe;
}

/* the interrupt manager registered an interrupt's handler */
static void OnIntrMgrInterruptHandlerRegistered(unsigned int intrCode)
{
	irqtrace_probe_t *probes;
	void *handler;

	sceKernelSpinlockLowLock(&s_irqtraceLock);
	if (s_irqtraceProvided) {
		char name[IRQTRACE_NAME_LEN] = { 0 };

		if (irqtrace_name(intrCode, name) && _createIrqProbe(intrCode, name) != NULL) {
			probes = s_irqtraceProbes;
			if (probes[intrCode].handler == NULL) {
				sceKernelRegisterIntrHookHandler(intrCode, irqtrace_hook, &handler);
				probes[intrCode].handler = handler;
			}
		}
	}
	sceKernelSpinlockLowUnlock(&s_irqtraceLock);
}

static void irqtrace_enable(void *arg, dtrace_id_t id, void *parg)
{
	irqtrace_probe_t *probes = s_irqtraceProbes;
	irqtrace_probe_t *probe;
	void *handler;
	int i;

	for (i = 0; i < IRQTRACE_NINTR; i++) {
		probe = &probes[i];
		if (probe->entryId == id) {
			if (probe->handler == NULL) {
				sceKernelRegisterIntrHookHandler(i, irqtrace_hook, &handler);
				probe->handler = handler;
			}
			probe->entryEnabled = 1;
			return;
		}
		if (probe->returnId == id) {
			if (probe->handler == NULL) {
				sceKernelRegisterIntrHookHandler(i, irqtrace_hook, &handler);
				probe->handler = handler;
			}
			probe->returnEnabled = 1;
			return;
		}
	}
}

static void irqtrace_disable(void *arg, dtrace_id_t id, void *parg)
{
	irqtrace_probe_t *probes = s_irqtraceProbes;
	irqtrace_probe_t *probe;
	int i;

	for (i = 0; i < IRQTRACE_NINTR; i++) {
		probe = &probes[i];
		if (probe->entryId == id) {
			probe->entryEnabled = 0;
			if (!probe->returnEnabled && probe->handler != NULL) {
				sceKernelReleaseIntrHookHandler(i);
				probe->handler = NULL;
			}
			return;
		}
		if (probe->returnId == id) {
			probe->returnEnabled = 0;
			if (!probe->entryEnabled && probe->handler != NULL) {
				sceKernelReleaseIntrHookHandler(i);
				probe->handler = NULL;
			}
			return;
		}
	}
}

static void irqtrace_provide(void *arg, const dtrace_probedesc_t *desc)
{
	unsigned int i;

	if (s_irqtraceProvided)
		return;
	sceKernelSpinlockLowLock(&s_irqtraceLock);
	s_irqtraceProvided = 1;
	SceIntrmgrForKernel_6F5B8AF2(OnIntrMgrInterruptHandlerRegistered, OnIntrMgrInterruptHandlerReleased);
	for (i = 0; i < IRQTRACE_NINTR; i++) {
		char name[IRQTRACE_NAME_LEN] = { 0 };

		if (irqtrace_name(i, name))
			_createIrqProbe(i, name);
	}
	sceKernelSpinlockLowUnlock(&s_irqtraceLock);
}

int irqtrace_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	unsigned int i;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	s_irqtraceProbes = dtrace_heap_alloc(IRQTRACE_NINTR * sizeof(irqtrace_probe_t));
	if (ddi_create_minor_node(devi, "irqtrace", S_IFCHR, 0, DDI_PSEUDO, 0) == DDI_FAILURE ||
	    dtrace_register("interrupt", &irqtrace_attr, DTRACE_PRIV_USER, NULL,
			    &irqtrace_pops, NULL, &irqtrace_id) != 0) {
		ddi_remove_minor_node(devi, NULL);
		kmem_free(s_irqtraceProbes, IRQTRACE_NINTR * sizeof(irqtrace_probe_t));
		s_irqtraceProbes = NULL;
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	irqtrace_devi = devi;
	for (i = 0; i < IRQTRACE_NINTR; i++) {
		s_irqtraceProbes[i].handler = NULL;
		s_irqtraceProbes[i].entryId = 0;
		s_irqtraceProbes[i].entryEnabled = 0;
		s_irqtraceProbes[i].returnId = 0;
		s_irqtraceProbes[i].returnEnabled = 0;
	}
	s_irqtraceLock = 0;
	return (DDI_SUCCESS);
}

int irqtrace_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	irqtrace_probe_t *probes;
	unsigned int i;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	sceKernelSpinlockLowLock(&s_irqtraceLock);
	SceIntrmgrForKernel_6F5B8AF2(NULL, NULL);
	s_irqtraceProvided = 0;
	sceKernelSpinlockLowUnlock(&s_irqtraceLock);

	probes = s_irqtraceProbes;
	if (probes != NULL) {
		for (i = 0; i < IRQTRACE_NINTR; i++) {
			if (probes[i].handler != NULL) {
				sceKernelReleaseIntrHookHandler(i);
				probes[i].handler = NULL;
				probes = s_irqtraceProbes;
			}
		}
		dtrace_heap_free(probes);
		s_irqtraceProbes = NULL;
	}

	if (dtrace_unregister(irqtrace_id) != 0)
		return (DDI_FAILURE);
	ddi_remove_minor_node(devi, NULL);
	return (DDI_SUCCESS);
}

int irqtrace_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	return 0;
}

int irqtrace_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	return 0;
}
