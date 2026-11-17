/*
 * SceNpCommonPs4: hosts that run timed jobs, on a timer thread of their own or on a SceNpCommon event loop.
 */

#include "../../np_common_ps4_internal.h"

extern "C" {

extern const void *g_hostInterfaceVtable[];	/* 0x8102a930 */
extern const void *g_timerHostVtable[];		/* 0x8102a94c */
extern const void *g_jobInterfaceVtable[];	/* 0x8102a968 */
extern const void *g_timerJobVtable[];		/* 0x8102a988 */
extern const void *g_eventLoopHostVtable[];	/* 0x8102a9a8 */
extern const void *g_eventLoopJobVtable[];	/* 0x8102a9c4 */

__attribute__((noinline)) int SceNpCommonPs4_71763AED(struct NpCommonPs4Job *job);
int SceNpCommonPs4_BC7706E0(struct NpCommonPs4Job *job);
__attribute__((noinline)) int SceNpCommonPs4_09998227(struct NpCommonPs4Job *job);
int SceNpCommonPs4_85DB3E3B(struct NpCommonPs4Job *job);

/* the interface of a host */
__attribute__((noinline)) struct NpCommonPs4TimerHost *npCommonPs4HostInterfaceInit(struct NpCommonPs4TimerHost *host)
{
	host->vtable = NP_COMMON_PS4_VTABLE(g_hostInterfaceVtable);
	return host;
}

struct NpCommonPs4TimerHost *npCommonPs4HostInterfaceDestruct(struct NpCommonPs4TimerHost *host)
{
	host->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return host;
}

int npCommonPs4HostInterfaceDelete(struct NpCommonPs4TimerHost *host)
{
	host->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(host);
}

__attribute__((noinline)) struct NpCommonPs4TimerHost *npCommonPs4HostInterfaceTerm(struct NpCommonPs4TimerHost *host)
{
	host->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return host;
}

/* the interface of a job */
__attribute__((noinline)) struct NpCommonPs4Job *npCommonPs4JobInterfaceInit(struct NpCommonPs4Job *job, void *host)
{
	job->host = host;
	job->vtable = NP_COMMON_PS4_VTABLE(g_jobInterfaceVtable);
	return job;
}

struct NpCommonPs4Job *npCommonPs4JobInterfaceDestruct(struct NpCommonPs4Job *job)
{
	job->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return job;
}

int npCommonPs4JobInterfaceDelete(struct NpCommonPs4Job *job)
{
	job->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(job);
}

__attribute__((noinline)) struct NpCommonPs4Job *npCommonPs4JobInterfaceTerm(struct NpCommonPs4Job *job)
{
	job->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return job;
}

/* the timer host */
struct NpCommonPs4TimerHost *SceNpCommonPs4_7CE53222(struct NpCommonPs4TimerHost *host)
{
	host->vtable = NP_COMMON_PS4_VTABLE(g_timerHostVtable);
	host->active = 0;
	host->borrowed = 0;
	return host;
}

struct NpCommonPs4TimerHost *SceNpCommonPs4_0C8E7219(struct NpCommonPs4TimerHost *host)
{
	host->vtable = NP_COMMON_PS4_VTABLE(g_timerHostVtable);
	host->active = 0;
	host->borrowed = 0;
	return host;
}

struct NpCommonPs4TimerHost *SceNpCommonPs4_2EEBF680(struct NpCommonPs4TimerHost *host)
{
	if (host->active != 0) {
		host->vtable = NP_COMMON_PS4_VTABLE(g_timerHostVtable);
		npCommonPs4TimerThreadStop(&host->timer);
	}
	host->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return host;
}

int SceNpCommonPs4_07B74981(struct NpCommonPs4TimerHost *host)
{
	if (host->active != 0) {
		host->vtable = NP_COMMON_PS4_VTABLE(g_timerHostVtable);
		npCommonPs4TimerThreadStop(&host->timer);
	}
	host->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return SceNpCommonPs4_BB5282C8(host);
}

struct NpCommonPs4TimerHost *SceNpCommonPs4_9B170DED(struct NpCommonPs4TimerHost *host)
{
	if (host->active != 0) {
		host->vtable = NP_COMMON_PS4_VTABLE(g_timerHostVtable);
		npCommonPs4TimerThreadStop(&host->timer);
	}
	host->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return host;
}

int npCommonPs4TimerHostStart(struct NpCommonPs4TimerHost *host, const char *name, int priority, SceSize stackSize,
	int cpuAffinity)
{
	int result;

	if (host->active != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	result = npCommonPs4TimerThreadStart(&host->timer, name, stackSize, priority, cpuAffinity);
	if (result >= 0) {
		host->active = 1;
		return 0;
	}
	return result;
}

int npCommonPs4TimerHostStartWithParam(struct NpCommonPs4TimerHost *host, const struct NpCommonPs4HostParam *param)
{
	return ((int (*)(struct NpCommonPs4TimerHost *, const char *, int, SceSize, int))host->vtable[2])(host,
		param->name, param->priority, param->stackSize, param->cpuAffinity);
}

int npCommonPs4TimerHostStop(struct NpCommonPs4TimerHost *host)
{
	if (host->active != 0) {
		if (host->borrowed == 0)
			npCommonPs4TimerThreadStop(&host->timer);
		host->active = 0;
		return 0;
	}
	return host->active;
}

/* the timer job */
struct NpCommonPs4Job *SceNpCommonPs4_E08F8F80(struct NpCommonPs4Job *job, void *host)
{
	job->host = host;
	job->vtable = NP_COMMON_PS4_VTABLE(g_timerJobVtable);
	job->state = 0;
	job->target = NULL;
	sceClibMemset(&job->entry, 0, sizeof(job->entry));
	return job;
}

struct NpCommonPs4Job *SceNpCommonPs4_69BF6A8E(struct NpCommonPs4Job *job, void *host)
{
	job->host = host;
	job->vtable = NP_COMMON_PS4_VTABLE(g_timerJobVtable);
	job->state = 0;
	job->target = NULL;
	sceClibMemset(&job->entry, 0, sizeof(job->entry));
	return job;
}

void SceNpCommonPs4_7EA18269(struct NpCommonPs4Job *job)
{
	struct NpCommonPs4TimerHost *host;
	int removed;

	if (((int (*)(struct NpCommonPs4Job *))job->vtable[4])(job) != 0) {
		host = (struct NpCommonPs4TimerHost *)job->host;
		removed = 0;
		if (npCommonPs4TimerThreadRemove(&host->timer, &job->entry, &removed) == 0) {
			if (removed != 0) {
				job->state = 0;
			} else {
				while (job->state < 0)
					sceKernelDelayThread(100000);
			}
		}
		job->target = NULL;
	}
}

__attribute__((noinline)) int SceNpCommonPs4_71763AED(struct NpCommonPs4Job *job)
{
	return job->target != NULL;
}

__attribute__((noinline)) struct NpCommonPs4Job *SceNpCommonPs4_0DA8B7B4(struct NpCommonPs4Job *job)
{
	struct NpCommonPs4TimerHost *host;
	int (*isActive)(struct NpCommonPs4Job *);
	int removed;

	if (job->target != NULL) {
		isActive = (int (*)(struct NpCommonPs4Job *))g_timerJobVtable[6];
		job->vtable = NP_COMMON_PS4_VTABLE(g_timerJobVtable);
		if (isActive(job)) {
			host = (struct NpCommonPs4TimerHost *)job->host;
			removed = 0;
			if (npCommonPs4TimerThreadRemove(&host->timer, &job->entry, &removed) == 0) {
				if (removed != 0) {
					job->state = 0;
				} else {
					while (job->state < 0)
						sceKernelDelayThread(100000);
				}
			}
			job->target = NULL;
		}
	}
	job->vtable = NP_COMMON_PS4_VTABLE(g_baseVtable);
	return job;
}

int SceNpCommonPs4_CCE35A46(struct NpCommonPs4Job *job)
{
	SceNpCommonPs4_0DA8B7B4(job);
	return SceNpCommonPs4_BB5282C8(job);
}

struct NpCommonPs4Job *SceNpCommonPs4_6A35BC92(struct NpCommonPs4Job *job)
{
	return SceNpCommonPs4_0DA8B7B4(job);
}

int SceNpCommonPs4_E991D631(struct NpCommonPs4Job *job, SceUInt32 delay, struct NpCommonPs4JobTarget *target)
{
	struct NpCommonPs4TimerHost *host;
	int result;

	if (target == NULL)
		return NP_COMMON_PS4_ERROR_80559E11;
	if (((int (*)(struct NpCommonPs4Job *))job->vtable[4])(job) != 0)
		((void (*)(struct NpCommonPs4Job *))job->vtable[3])(job);
	host = (struct NpCommonPs4TimerHost *)job->host;
	job->state = 0;
	job->target = target;
	result = npCommonPs4TimerThreadAdd(&host->timer, &job->entry, delay, (int (*)(void *))SceNpCommonPs4_BC7706E0, job);
	if (result >= 0) {
		job->state = -1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_745AAEDF(struct NpCommonPs4Job *job)
{
	if (job->state == -2 || job->state > 0)
		return 1;
	return 0;
}

int SceNpCommonPs4_BC7706E0(struct NpCommonPs4Job *job)
{
	struct NpCommonPs4JobTarget *target;

	target = job->target;
	job->state = -2;
	((void (*)(struct NpCommonPs4JobTarget *))target->vtable[2])(target);
	job->state = 1;
	return 1;
}

/* the event loop host */
struct NpCommonPs4EventLoopHost *SceNpCommonPs4_326EF8D2(struct NpCommonPs4EventLoopHost *host)
{
	npCommonPs4HostInterfaceInit((struct NpCommonPs4TimerHost *)host);
	host->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopHostVtable);
	host->borrowed = 0;
	host->active = 0;
	host->loop = NULL;
	host->loopData[0] = 0;
	host->loopData[1] = 0;
	host->loopData[2] = 0;
	host->loopData[3] = 0;
	host->loopData[4] = 0;
	host->loopData[5] = 0;
	return host;
}

struct NpCommonPs4EventLoopHost *SceNpCommonPs4_6E263A71(struct NpCommonPs4EventLoopHost *host)
{
	npCommonPs4HostInterfaceInit((struct NpCommonPs4TimerHost *)host);
	host->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopHostVtable);
	host->borrowed = 0;
	host->active = 0;
	host->loop = NULL;
	host->loopData[0] = 0;
	host->loopData[1] = 0;
	host->loopData[2] = 0;
	host->loopData[3] = 0;
	host->loopData[4] = 0;
	host->loopData[5] = 0;
	return host;
}

int SceNpCommonPs4_B6551892(struct NpCommonPs4EventLoopHost *host)
{
	if (host->active != 0) {
		if (host->borrowed == 0)
			SceNpCommon_48314C6E(host->loop);
		host->active = 0;
		return 0;
	}
	return host->active;
}

struct NpCommonPs4EventLoopHost *SceNpCommonPs4_CCD27416(struct NpCommonPs4EventLoopHost *host)
{
	SceUInt8 active;

	active = host->active;
	host->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopHostVtable);
	if (active != 0) {
		if (host->borrowed == 0)
			SceNpCommon_48314C6E(host->loop);
		host->active = 0;
	}
	npCommonPs4HostInterfaceTerm((struct NpCommonPs4TimerHost *)host);
	return host;
}

int SceNpCommonPs4_4681352F(struct NpCommonPs4EventLoopHost *host)
{
	SceUInt8 active;

	active = host->active;
	host->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopHostVtable);
	if (active != 0) {
		if (host->borrowed == 0)
			SceNpCommon_48314C6E(host->loop);
		host->active = 0;
	}
	npCommonPs4HostInterfaceTerm((struct NpCommonPs4TimerHost *)host);
	return SceNpCommonPs4_BB5282C8(host);
}

struct NpCommonPs4EventLoopHost *SceNpCommonPs4_0273B84C(struct NpCommonPs4EventLoopHost *host)
{
	SceUInt8 active;

	active = host->active;
	host->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopHostVtable);
	if (active != 0) {
		if (host->borrowed == 0)
			SceNpCommon_48314C6E(host->loop);
		host->active = 0;
	}
	npCommonPs4HostInterfaceTerm((struct NpCommonPs4TimerHost *)host);
	return host;
}

int SceNpCommonPs4_E9C5CC51(struct NpCommonPs4EventLoopHost *host, const char *name, int priority, SceSize stackSize,
	int cpuAffinity)
{
	int result;

	if (host->active != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	host->loop = host->loopData;
	result = SceNpCommon_9AEF645B(host->loopData, name, stackSize, priority, cpuAffinity);
	if (result >= 0) {
		host->active = 1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_90AA5AC2(struct NpCommonPs4EventLoopHost *host, const struct NpCommonPs4HostParam *param)
{
	return ((int (*)(struct NpCommonPs4EventLoopHost *, const char *, int, SceSize, int))host->vtable[2])(host,
		param->name, param->priority, param->stackSize, param->cpuAffinity);
}

int SceNpCommonPs4_70073BD1(struct NpCommonPs4EventLoopHost *host, void *loop)
{
	if (host->active != 0)
		return NP_COMMON_PS4_ERROR_80559E03;
	host->loop = loop;
	host->borrowed = 1;
	host->active = 1;
	return 0;
}

/* the event loop job */
struct NpCommonPs4Job *SceNpCommonPs4_1CC3A645(struct NpCommonPs4Job *job, void *host)
{
	npCommonPs4JobInterfaceInit(job, host);
	job->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopJobVtable);
	job->state = 0;
	job->target = NULL;
	sceClibMemset(&job->entry, 0, sizeof(job->entry));
	return job;
}

struct NpCommonPs4Job *SceNpCommonPs4_CCF9D566(struct NpCommonPs4Job *job, void *host)
{
	npCommonPs4JobInterfaceInit(job, host);
	job->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopJobVtable);
	job->state = 0;
	job->target = NULL;
	sceClibMemset(&job->entry, 0, sizeof(job->entry));
	return job;
}

void SceNpCommonPs4_7A30E24E(struct NpCommonPs4Job *job)
{
	struct NpCommonPs4EventLoopHost *host;
	int removed;

	if (((int (*)(struct NpCommonPs4Job *))job->vtable[4])(job) != 0) {
		host = (struct NpCommonPs4EventLoopHost *)job->host;
		removed = 0;
		if (SceNpCommon_BDCD318E(host->loop, &job->entry, &removed) == 0) {
			if (removed != 0) {
				job->state = 0;
			} else {
				while (job->state < 0)
					sceKernelDelayThread(100000);
			}
		}
		job->target = NULL;
	}
}

__attribute__((noinline)) int SceNpCommonPs4_09998227(struct NpCommonPs4Job *job)
{
	return job->target != NULL;
}

__attribute__((noinline)) struct NpCommonPs4Job *SceNpCommonPs4_941B0290(struct NpCommonPs4Job *job)
{
	struct NpCommonPs4EventLoopHost *host;
	struct NpCommonPs4JobTarget *target;
	int removed;

	target = job->target;
	job->vtable = NP_COMMON_PS4_VTABLE(g_eventLoopJobVtable);
	if (target != NULL && ((int (*)(struct NpCommonPs4Job *))g_eventLoopJobVtable[6])(job)) {
		host = (struct NpCommonPs4EventLoopHost *)job->host;
		removed = 0;
		if (SceNpCommon_BDCD318E(host->loop, &job->entry, &removed) == 0) {
			if (removed != 0) {
				job->state = 0;
			} else {
				while (job->state < 0)
					sceKernelDelayThread(100000);
			}
		}
		job->target = NULL;
	}
	npCommonPs4JobInterfaceTerm(job);
	return job;
}

int SceNpCommonPs4_A32C2797(struct NpCommonPs4Job *job)
{
	SceNpCommonPs4_941B0290(job);
	return SceNpCommonPs4_BB5282C8(job);
}

struct NpCommonPs4Job *SceNpCommonPs4_1C09F1DB(struct NpCommonPs4Job *job)
{
	return SceNpCommonPs4_941B0290(job);
}

int SceNpCommonPs4_ED3D9A81(struct NpCommonPs4Job *job, SceUInt32 delay, struct NpCommonPs4JobTarget *target)
{
	int result;

	if (target == NULL)
		return NP_COMMON_PS4_ERROR_80559E11;
	if (((int (*)(struct NpCommonPs4Job *))job->vtable[4])(job) != 0)
		((void (*)(struct NpCommonPs4Job *))job->vtable[3])(job);
	job->target = target;
	job->state = 0;
	result = SceNpCommon_40724BEA(((struct NpCommonPs4EventLoopHost *)job->host)->loop, &job->entry, delay, 0,
		(int (*)(void *))SceNpCommonPs4_85DB3E3B, job);
	if (result >= 0) {
		job->state = -1;
		return 0;
	}
	return result;
}

int SceNpCommonPs4_0B7605C0(struct NpCommonPs4Job *job)
{
	if (job->state == -2 || job->state > 0)
		return 1;
	return 0;
}

int SceNpCommonPs4_85DB3E3B(struct NpCommonPs4Job *job)
{
	struct NpCommonPs4JobTarget *target;

	target = job->target;
	job->state = -2;
	((void (*)(struct NpCommonPs4JobTarget *))target->vtable[2])(target);
	job->state = 1;
	return 1;
}

const void *g_hostInterfaceVtable[] = {		/* 0x8102a930 */
	NULL,
	NULL,
	(const void *)npCommonPs4HostInterfaceDestruct,
	(const void *)npCommonPs4HostInterfaceDelete,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
};

const void *g_timerHostVtable[] = {		/* 0x8102a94c */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_2EEBF680,
	(const void *)SceNpCommonPs4_07B74981,
	(const void *)npCommonPs4TimerHostStart,
	(const void *)npCommonPs4TimerHostStartWithParam,
	(const void *)npCommonPs4TimerHostStop,
};

const void *g_jobInterfaceVtable[] = {		/* 0x8102a968 */
	NULL,
	NULL,
	(const void *)npCommonPs4JobInterfaceDestruct,
	(const void *)npCommonPs4JobInterfaceDelete,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
	(const void *)__cxa_pure_virtual,
};

const void *g_timerJobVtable[] = {		/* 0x8102a988 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_0DA8B7B4,
	(const void *)SceNpCommonPs4_CCE35A46,
	(const void *)SceNpCommonPs4_E991D631,
	(const void *)SceNpCommonPs4_7EA18269,
	(const void *)SceNpCommonPs4_71763AED,
	(const void *)SceNpCommonPs4_745AAEDF,
};

const void *g_eventLoopHostVtable[] = {		/* 0x8102a9a8 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_CCD27416,
	(const void *)SceNpCommonPs4_4681352F,
	(const void *)SceNpCommonPs4_E9C5CC51,
	(const void *)SceNpCommonPs4_90AA5AC2,
	(const void *)SceNpCommonPs4_B6551892,
};

const void *g_eventLoopJobVtable[] = {		/* 0x8102a9c4 */
	NULL,
	NULL,
	(const void *)SceNpCommonPs4_941B0290,
	(const void *)SceNpCommonPs4_A32C2797,
	(const void *)SceNpCommonPs4_ED3D9A81,
	(const void *)SceNpCommonPs4_7A30E24E,
	(const void *)SceNpCommonPs4_09998227,
	(const void *)SceNpCommonPs4_0B7605C0,
};

}
