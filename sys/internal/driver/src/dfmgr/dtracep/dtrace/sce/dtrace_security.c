/*
 * What DTrace may trace (Sony's dtrace/sce/dtrace_security.c): everything on
 * a kit QA flags mark internal, otherwise only processes the kit may debug and
 * modules that are not the system's.
 */
#include <sys/dtrace_impl.h>
#include <dtrace/sce/dtrace_security.h>
#include <dtrace/sce/dtrace_process.h>
#include <acmgr/acmgr.h>
#include <modulemgr/modulemgr_control.h>
#include <processmgr/processmgr.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>

/* s_security: not asked yet, then what the QA flags said */
#define DTRACE_SECURITY_UNKNOWN		0
#define DTRACE_SECURITY_INTERNAL	1
#define DTRACE_SECURITY_EXTERNAL	2

/* the module attribute bits of a system module */
#define DTRACE_MODULE_ATTR_SYSTEM	0xe000

static int s_security;

static inline int dtrace_security_is_internal(void)
{
	if (s_security == DTRACE_SECURITY_UNKNOWN)
		s_security = SceQafMgrForDriver_382C71E8() != 0 ? DTRACE_SECURITY_INTERNAL
								 : DTRACE_SECURITY_EXTERNAL;
	return s_security == DTRACE_SECURITY_INTERNAL;
}

int dtrace_security_check_internal(void)
{
	return dtrace_security_is_internal() ? 0 : -1;
}

int dtrace_security_check_access_process_state(SceUID pid)
{
	SceUInt32 status;
	int res;

	if (pid == DTRACE_KERNEL_PROCESS || dtrace_security_is_internal())
		return 0;
	res = sceKernelGetProcessStatus(pid, &status);
	if (res != 0)
		return res;
	return SceSblACMgrForDriver_4CBD6156(pid) != 0 ? 0 : -1;
}

int dtrace_security_check_access_current_process_state(void)
{
	return dtrace_security_check_access_process_state(dtrace_get_current_process_id());
}

int dtrace_security_check_access_process(SceUID pid)
{
	int res;

	if (dtrace_security_is_internal())
		return 0;
	res = sceGUIDReferObjectWithClassLevel(pid, SceProcessmgrForKernel_98AE4BC8(), 3, NULL);
	if (res != 0)
		return -1;
	sceGUIDReleaseObject(pid);
	return res;
}

int dtrace_security_check_access_thread(SceUID thid)
{
	int res;

	if (dtrace_security_is_internal())
		return 0;
	res = sceGUIDReferObjectWithClassLevel(thid, SceThreadmgrForKernel_565BD2DA(), 3, NULL);
	if (res != 0)
		return -1;
	sceGUIDReleaseObject(thid);
	return res;
}

int dtrace_security_check_module_info(struct SceModuleCB *ctl)
{
	if (dtrace_security_is_internal())
		return 0;
	return (ctl->attr & DTRACE_MODULE_ATTR_SYSTEM) == 0 ? 0 : -1;
}

int dtrace_security_check_module_probes(struct SceModuleCB *ctl)
{
	return 0;
}
