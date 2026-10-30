/*
 * The syscall provider's probes (Sony's dtrace/sce/sce_systrace.c): one per
 * system library function, entry and return, hooked through syslibtrace.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/sce/dtrace_security.h>
#include <dtrace/sce/sce_systrace.h>
#include <dtrace/sce/solaris_kmem.h>
#include <sysmem/qafmgr.h>
#include <syslibtrace/syslibtrace.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>

/*
 * A library's probes, the enter or the return ones: a header naming the
 * library, then one probe per exported function.
 */
typedef union systrace_lib_probe {
	struct {
		SceNID libnid;
		uint32_t nfunc;
	} header;			/* [0] */
	struct {
		dtrace_id_t id;
		int enabled;
	} probe;			/* [1 + the function's index] */
} systrace_lib_probe_t;

/* the system call whose probes are never created */
#define SYSTRACE_SKIP_LIBNID	0x5e8d0e22
#define SYSTRACE_SKIP_NID	0x0c8a38e1

/* what an external console shows in place of an unnamed NID */
#define SYSTRACE_NID_MASK(nid)	(~((nid) + 0x162b09c8))

extern dtrace_provider_id_t systrace_id;

static int s_systraceStarted;		/* syslibtrace traces all libraries */
static uint32_t s_systraceEnabled;	/* the enabled probes */
static int s_systraceRegistered;	/* the syslibtrace callbacks are set */

static void syslibtrace_set_target(LibEntCB *lib, void **enterArg, void **returnArg)
{
}

static void syslibtrace_clear_target(LibEntCB *lib, void **enterArg, void **returnArg)
{
}

static void syslibtrace_deregister_lib(LibEntCB *lib, void **enterArg, void **returnArg)
{
	systrace_lib_probe_t *entry, *ret;
	size_t size;

	SCE_DBG_ASSERT(lib != NULL, 296);
	entry = *enterArg;
	if (entry == NULL || (ret = *returnArg) == NULL) {
		dtrace_security_check_module_probes(lib->cb);
		return;
	}

	SCE_DBG_ASSERT(entry->header.libnid == lib->exports->libnid, 311);
	SCE_DBG_ASSERT(ret->header.libnid == lib->exports->libnid, 312);
	SCE_DBG_ASSERT(entry->header.nfunc == ret->header.nfunc, 313);
	kmem_free(entry, (entry->header.nfunc + 1) * sizeof(*entry));
	size = (ret->header.nfunc + 1) * sizeof(*ret);
	*enterArg = NULL;
	kmem_free(ret, size);
	*returnArg = NULL;
}

static void syslibtrace_return(SceUInt32 libraryNid, SceUInt32 nid, SceUInt32 index, void *arg,
			       SceUInt32 r0, SceUInt32 r1, SceUInt32 r2, SceUInt32 r3)
{
	systrace_lib_probe_t *probes = arg;

	if (!s_systraceStarted || probes == NULL)
		return;
	SCE_DBG_ASSERT(probes != NULL, 168);
	SCE_DBG_ASSERT(index < probes->header.nfunc, 169);
	if (!probes[index + 1].probe.enabled)
		return;
	if (dtrace_security_check_access_current_process_state() != 0)
		return;
	dtrace_probe(probes[index + 1].probe.id, r0, r1, r2, r3, 0);
}

static void syslibtrace_entry(SceUInt32 libraryNid, SceUInt32 nid, SceUInt32 index, void *arg,
			      SceUInt32 r0, SceUInt32 r1, SceUInt32 r2, SceUInt32 r3)
{
	systrace_lib_probe_t *probes = arg;

	if (!s_systraceStarted || probes == NULL)
		return;
	SCE_DBG_ASSERT(probes != NULL, 144);
	SCE_DBG_ASSERT(index < probes->header.nfunc, 145);
	if (!probes[index + 1].probe.enabled)
		return;
	if (dtrace_security_check_access_current_process_state() != 0)
		return;
	dtrace_probe(probes[index + 1].probe.id, r0, r1, r2, r3, 0);
}

/* the NID's name from the kernel's table, NULL when unknown or not public */
static inline const char *syslibtrace_nid_name(SceNID nid)
{
	const SceNidsymtblEntry *table;
	int n, lo, hi, mid;

	n = SceNidsymtblForDriver_138A9106(&table);
	if (n < 0 || table == NULL || n == 0)
		return NULL;
	lo = 0;
	hi = n - 1;
	while (lo <= hi) {
		mid = (lo + hi) / 2;
		if (nid > table[mid].nid) {
			lo = mid + 1;
		} else if (nid < table[mid].nid) {
			hi = mid - 1;
		} else {
			if (SceQafMgrForDriver_CAD47130() != 0 || (table[mid].flags & SCE_NIDSYMTBL_PUBLIC))
				return table[mid].name;
			return NULL;
		}
	}
	return NULL;
}

static void syslibtrace_register_lib(LibEntCB *lib, void **enterArg, void **returnArg)
{
	systrace_lib_probe_t *entry, *ret;
	const char *modname, *name;
	char unknown[16];
	char buf[32];
	SceNID nid;
	uint32_t nfunc, i;

	SCE_DBG_ASSERT(lib != NULL, 195);
	if (dtrace_security_check_module_probes(lib->cb) != 0)
		return;
	if (*enterArg != NULL || *returnArg != NULL)
		return;
	nfunc = lib->exports->nfunc;
	if (nfunc == 0)
		return;
	SCE_DBG_ASSERT(nfunc > 0, 212);
	if (lib->exports->nidtable == NULL)
		return;

	entry = kmem_zalloc((nfunc + 1) * sizeof(*entry), KM_SLEEP);
	SCE_DBG_ASSERT(entry != NULL, 220);
	entry->header.libnid = lib->exports->libnid;
	entry->header.nfunc = nfunc;
	ret = kmem_zalloc((nfunc + 1) * sizeof(*ret), KM_SLEEP);
	SCE_DBG_ASSERT(ret != NULL, 225);
	ret->header.libnid = lib->exports->libnid;
	ret->header.nfunc = nfunc;

	memset(unknown, 0, sizeof(unknown));
	strncpy(unknown, "unknown", sizeof(unknown) - 1);

	for (i = 0; i < nfunc; i++) {
		nid = lib->exports->nidtable[i];
		name = syslibtrace_nid_name(nid);
		if (lib->exports->libnid == SYSTRACE_SKIP_LIBNID && nid == SYSTRACE_SKIP_NID)
			continue;
		if (name != NULL) {
			modname = lib->exports->libname;
		} else if (dtrace_security_check_internal() != 0) {
			snprintf(buf, sizeof(buf) - 1, "syscall_0x%08x", SYSTRACE_NID_MASK(nid));
			buf[sizeof(buf) - 1] = '\0';
			name = buf;
			modname = unknown;
		} else {
			snprintf(buf, sizeof(buf) - 1, "syscall_0x%08x", nid);
			buf[sizeof(buf) - 1] = '\0';
			name = buf;
			modname = lib->exports->libname;
		}
		if (dtrace_probe_lookup(systrace_id, modname, name, "entry") != 0)
			continue;
		entry[i + 1].probe.id = dtrace_probe_create(systrace_id, modname, name, "entry", 1,
							    &entry[i + 1]);
		entry[i + 1].probe.enabled = 0;
		ret[i + 1].probe.id = dtrace_probe_create(systrace_id, modname, name, "return", 1,
							  &ret[i + 1]);
		ret[i + 1].probe.enabled = 0;
	}

	*enterArg = entry;
	*returnArg = ret;
}

void systrace_provide_sce(void *arg, const dtrace_probedesc_t *desc)
{
	SceSyslibtraceCallbacks callbacks;
	int res;

	if (desc != NULL || s_systraceRegistered)
		return;

	callbacks.enter = (SceSyslibtraceSyscallCallback)syslibtrace_entry;
	callbacks.leave = (SceSyslibtraceSyscallCallback)syslibtrace_return;
	callbacks.registerLib = (SceSyslibtraceLibCallback)syslibtrace_register_lib;
	callbacks.deregisterLib = (SceSyslibtraceLibCallback)syslibtrace_deregister_lib;
	callbacks.setTarget = (SceSyslibtraceLibCallback)syslibtrace_set_target;
	callbacks.clearTarget = (SceSyslibtraceLibCallback)syslibtrace_clear_target;
	s_systraceEnabled = 0;
	s_systraceRegistered = 1;
	callbacks.size = sizeof(callbacks);
	res = SceSyslibtraceForKernel_F9CB8BA9(&callbacks, 0);
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(371, "failed to create syscall trace (0x%08x)", res);
		return;
	}
	res = sceSyslibtraceCallbackRegisterLibs(0);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(379, "failed to do callbacks to register syscall trace libs (0x%08x)", res);
}

void systrace_probe_destroy_sce(void *arg, dtrace_id_t id, void *parg)
{
}

void systrace_probe_enable_sce(void *arg, dtrace_id_t id, void *parg)
{
	systrace_lib_probe_t *probe = parg;
	SceSyslibtraceOpt target, start;
	int res;

	SCE_DBG_ASSERT(probe != NULL, 406);
	SCE_DBG_ASSERT(probe->probe.id == id, 407);
	if (probe->probe.enabled)
		return;
	probe->probe.enabled = 1;
	if (++s_systraceEnabled == 0 || s_systraceStarted)
		return;

	target.size = sizeof(target);
	res = sceSyslibtraceSetTargetSyscallLib(SCE_SYSLIBTRACE_ALL_LIBS, &target);
	if (res != 0) {
		DTRACE_LOG_ERROR_AT(424, "failed to set syscall trace target lib to be all libs (0x%08x)", res);
		return;
	}
	start.size = sizeof(start);
	res = sceSyslibtraceStartSyscallTrace(&start);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(435, "failed to start syscall trace (0x%08x)", res);
	s_systraceStarted = 1;
}

void systrace_probe_disable_sce(void *arg, dtrace_id_t id, void *parg)
{
	systrace_lib_probe_t *probe = parg;
	SceSyslibtraceOpt stop, clear;
	int res;

	SCE_DBG_ASSERT(probe != NULL, 453);
	SCE_DBG_ASSERT(probe->probe.id == id, 454);
	if (!probe->probe.enabled)
		return;
	probe->probe.enabled = 0;
	SCE_DBG_ASSERT(s_systraceEnabled != 0, 461);
	if (--s_systraceEnabled != 0 || !s_systraceStarted)
		return;

	stop.size = sizeof(stop);
	res = sceSyslibtraceStopSyscallTrace(&stop);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(473, "failed to stop syscall trace (0x%08x)", res);
	clear.size = sizeof(clear);
	res = sceSyslibtraceClearTargetSyscallLib(SCE_SYSLIBTRACE_ALL_LIBS, &clear);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(483, "failed to clear syscall trace target lib (0x%08x)", res);
	s_systraceStarted = 0;
}

void systrace_detach_sce(void)
{
	SceSyslibtraceOpt stop, clear;
	int res;

	if (s_systraceStarted) {
		stop.size = sizeof(stop);
		res = sceSyslibtraceStopSyscallTrace(&stop);
		if (res != 0)
			DTRACE_LOG_ERROR_AT(503, "failed to stop syscall trace (0x%08x)", res);
		clear.size = sizeof(clear);
		res = sceSyslibtraceClearTargetSyscallLib(SCE_SYSLIBTRACE_ALL_LIBS, &clear);
		if (res != 0)
			DTRACE_LOG_ERROR_AT(513, "failed to clear syscall trace target lib (0x%08x)", res);
		s_systraceStarted = 0;
	}
	res = sceSyslibtraceCallbackDeregisterLibs(0);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(523, "failed to do callbacks to deregister syscall trace libs (0x%08x)", res);
	res = SceSyslibtraceForKernel_BD326721(0);
	if (res != 0)
		DTRACE_LOG_ERROR_AT(530, "failed to destroy syscall trace (0x%08x)", res);
	s_systraceStarted = 0;
	s_systraceRegistered = 0;
}
