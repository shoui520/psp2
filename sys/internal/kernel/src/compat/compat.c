/* SceCompat implementation. */

#include <psp2/compat.h>
#include <compat/compat.h>
#include <psp2/power.h>
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/ssmgr.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/intrmgr.h>
#include <psp2kern/kernel/proc_event.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysmem/uid_guid.h>
#include <psp2kern/kernel/sysmem/address_space.h>
#include <sysmem/sysevent.h>
#include <intrmgr/syscall.h>
#include <psp2kern/kernel/cpu.h>
#include <sysmem/debug.h>
#include <display/display.h>
#include <idstorage/idstorage.h>
#include <rtc/rtc.h>
#include <sysmem/sysroot.h>
#include <sm_comm/sm_comm.h>
#include <compat/compat_regs.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/devctl.h>
#include <psp2kern/npdrm.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct CompatInterruptConfig {
	int intrcode;
	const char *name;
	int priority;
	int target_cpu;
};

_Static_assert(offsetof(SceCompatLcdDmaRegisters, startControl) == 0xc0,
	"compat LCDDMA start control");
_Static_assert(offsetof(SceCompatLcdDmaRegisters, physicalAddress) == 0x4c,
	"compat LCDDMA physical address");

struct CompatRequestQueue {
	unsigned int requestWords[16];
	unsigned int readIndex;
	unsigned int writeIndex;
	unsigned int count;
	unsigned int overflow;
};

_Static_assert(sizeof(struct CompatRequestQueue) == 80,
	"compat request queue");
_Static_assert(offsetof(struct CompatRequestQueue, readIndex) == 64,
	"compat request queue read index");
_Static_assert(offsetof(struct CompatRequestQueue, writeIndex) == 68,
	"compat request queue write index");
_Static_assert(offsetof(struct CompatRequestQueue, count) == 72,
	"compat request queue count");
_Static_assert(offsetof(struct CompatRequestQueue, overflow) == 76,
	"compat request queue overflow");

union CompatSecureCommand {
	SceUInt32 arguments[2];
	SceSblSmCommPair stopResult;
};

_Static_assert(sizeof(union CompatSecureCommand) == 8,
	"compat secure command and stop result");

_Static_assert(sizeof(struct CompatInterruptConfig) == 16,
	"compat interrupt configuration");

const unsigned int gCompatPocketStationTitleHashes[142] = {
	0x43455003, 0x434b4022, 0x434b6020, 0x434b0026, 0x434b3025, 0x434a60d0,
	0x434940c2, 0x434970c1, 0x434960c0, 0x434920c4, 0x43521157, 0x43520156,
	0x43523155, 0x43527151, 0x4351c14a, 0x4350d17b, 0xc75acb56, 0xd550c10e,
	0xd56d31dd, 0xd564a244, 0xd566f269, 0xd560920b, 0xd560820a, 0xd563c23e,
	0xd563f239, 0xd57dd2df, 0xd576c36e, 0xd577a374, 0xd570e308, 0xd571e318,
	0xd372432c, 0xd572c32e, 0xd50df3d9, 0xd50db3d5, 0xd505845a, 0xd505b455,
	0xd507a474, 0xd500840a, 0xd501341d, 0xd526266c, 0xd537b775, 0x522566c1,
	0x523c3756, 0x523fd764, 0x5238f71a, 0x523a2735, 0x523ac73b, 0x523bd724,
	0x523bf72a, 0x52cc4853, 0x52ccf85a, 0x52cf4863, 0x52ca2835, 0x52cb4823,
	0x52cb2825, 0x52c448d3, 0x52dc2955, 0x52dd7942, 0x52de7972, 0x52df6961,
	0x52db4923, 0x52d479d2, 0x52d549c3, 0x514ee07a, 0x514ff06b, 0x515cf15b,
	0x515e7173, 0x515e3177, 0x515ec174, 0x515f3167, 0x51587113, 0x51586112,
	0x515b7123, 0x515bc124, 0x515421d6, 0x5155e1ca, 0x516c2256, 0x516df24b,
	0x516e7273, 0x516ed275, 0x516fd265, 0x51680210, 0x51692206, 0x516a1231,
	0x516502c0, 0x5165c2c4, 0x5165e2ca, 0x517cc354, 0x517e3377, 0x517ff36b,
	0x517bf32b, 0x517be32a, 0x517403d0, 0x510c3457, 0x510d1441, 0x5109c404,
	0x5104e4da, 0x5105c4c4, 0x5105f4cb, 0x5105e4ca, 0x511d3547, 0x511e7573,
	0x511a6532, 0x511b1521, 0x512c1651, 0x512f2666, 0x512fd665, 0x5128e61a,
	0x51297603, 0x51296602, 0x5129f60b, 0x512a7633, 0x512a3637, 0x513e3777,
	0x5138e71a, 0x51cc7853, 0x51cd2846, 0x51cdc844, 0x51cdf84b, 0x51cde84a,
	0x51cf1861, 0x51c83817, 0x51c408d0, 0x51dd6942, 0x51d4f9db, 0x51d519c1,
	0x50496003, 0x504a2037, 0x504bc025, 0x5044f0d4, 0x505c1152, 0x505c3150,
	0x505d2147, 0x505e1172, 0x5058d116, 0x5058c115, 0x5059c105, 0x505461d3,
	0x506d1242, 0x50690201, 0xc2520107, 0xc2010417,
};

const struct CompatInterruptConfig gCompatInterruptConfig[7] = {
	{ 68, "SceCompat0c", 160, 1 },
	{ 69, "SceCompat1c", 160, 2 },
	{ 70, "SceCompat2c", 160, 4 },
	{ 71, "SceCompat0r", 160, 1 },
	{ 72, "SceCompat1r", 160, 2 },
	{ 73, "SceCompat2r", 160, 4 },
	{ 74, "SceCompatLcdc", 160, 1 },
};

const char gCompatRifPathTemplate[] =
	"ux0:pspemu/PSP/LICENSE/AA0000-AAAA00000_00-0000000000000000.rif";
const char gCompatPocketStationRifPath[] =
	"ux0:pspemu/PSP/LICENSE/JA0003-PCSC80018_00-POCKETSTATION001.rif";
const char gCompatPocketStationContentId[] =
	"JA0003-PCSC80018_00-POCKETSTATION001";
const char gCompatCachedName[] = "SceCompatCached";
const char gCompatUncachedName[] = "SceCompatUncached";
const char gCompatCdramName[] = "SceCompatCdram";
const char gCompatDevice[] = "ux0:";
const char gCompatTitleListPath[] = "ur0:emu_list/title_list.bin";
const char gCompatPspFirmwarePath[] = "vs0:app/NPXS10028/pcff.skprx";
const char gCompatBootFirmwarePath[] = "os0:kd/pcbc.skprx";
const char gCompatSecureModulePath[] = "os0:sm/compat_sm.self";
const SceSelfAuthInfo gCompatSecureModuleAuth = {
	.program_authority_id = 0x2808000000000001ULL,
	.capability = {
		0x80, 0, 0, 0, 0xc0, 0, 0xf0, 0,
		0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
	},
	.attribute = {
		0x80, 0x09, 0x80, 0x03, 0, 0, 0xc3, 0,
		0, 0, 0x80, 0x09, 0x80, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff,
	},
};

_Static_assert(sizeof(SceCompatMailboxRegisters) == 0x1000,
	"compat mailbox register page");
_Static_assert(offsetof(SceCompatMailboxRegisters, enableInterrupt) == 0x80,
	"compat mailbox interrupt enable bank");

char gCompatRifPath[256];
SceCompatMailboxRegisters *gCompatMailbox;
struct CompatRequestQueue gCompatRequestQueues[16];
int gCompatRequestsEnabled;
SceUID gCompatMutex;
SceUID gCompatProcessId;
int gCompatState;
int gCompatPreviousState;
void *gCompatMainDdr;
SceUID gCompatMemoryBlocks[3][3];
int gCompatMode;
SceUID gCompatFrameBuffer;
SceUID gCompatSuspendSema;
SceUID gCompatResumeSema;
SceUInt32 gCompatResumeCookie;
SceNpDrmActivationData gCompatActivationData;
SceCompatLcdDmaRegisters *gCompatLcdDma;
SceCompatSharedSram *gCompatSharedSram;
SceUID gCompatLcdDmaMemBlock;
SceUID gCompatMailboxMemBlock;
SceUID gCompatSharedSramMemBlock;
SceUID gCompatExitMutex;
SceUID gCompatHandlerSema[16];
SceUID gCompatReturnSema[3];
SceRtcTick gCompatRtcDeadline;
uintptr_t gCompatCacheRangeBase;
SceSblSmCommId gCompatSecureModuleId;
SceUInt32 gCompatColorSpaceMode;
#if FW_VERSION >= 0x03650000
unsigned int gCompatBootRandom;
#endif
unsigned char gCompatActivationReadBuffer[2048];
char gCompatTitleListBuffer[100];

int compatProcessKillHandler(SceUID pid, SceProcEventInvokeParam1 *param,
	int flags);
int compatProcessExitHandler(SceUID pid, SceProcEventInvokeParam1 *param,
	int flags);
int compatProcessStopHandler(SceUID pid, int event,
	SceProcEventInvokeParam1 *param, int flags);
int compatCallIntrHandler(int intrcode, void *context);
int compatEnterRunningState(void);
int compatInitEx(int mode);
int compatSecureInitialize(SceUInt32 argument);
int compatSecureResume(void);
int compatReleaseFrameBuffer(void);
int compatWaitAndGetRequest(int mode, int id);
int compatWaitSpecialRequest(int mode);
int compatInterrupt(int intrCode);
int compatReturnValueEx(SceUInt32 channel, const SceUInt32 values[2],
	SceUInt64 value);

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, gCompatWaitSpecialRequestInfo, 1402,
	"sceCompatWaitSpecialRequest");
#endif

const SceProcEventHandler gCompatProcessHandlers = {
	sizeof(SceProcEventHandler), NULL, compatProcessExitHandler,
	compatProcessKillHandler, compatProcessStopHandler, NULL, NULL,
};

SceKernelIntrOptParam gCompatInterruptOptions = {
	sizeof(SceKernelIntrOptParam), 0, NULL, 1, 3,
};

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, gCompatMapMainDdrTranslationInfo, 751, "mapMainDDR");
SCE_DBGINFO_DEFINE(static, gCompatMapMainDdrBankInfo, 753, "mapMainDDR");
#endif

int SceDisplayForDriver_332C5410(int head);
int ksceDisplaySetColorSpaceMode(SceUInt32 head, int mode);
int ksceDisplayGetDeviceType(SceUInt32 head, SceUInt32 *type);
int SceGrabForDriver_F13C63DD(void);
int SceGrabForDriver_379130B2(void);
int SceGrabForDriver_E9C25A28(SceUInt32 bank, uintptr_t paddr);
int SceGrabForDriver_072B8D93(int mode);
int ksceSblFwLoaderLock(const char *path, int flags);
int ksceSblFwLoaderLoad(int type, void *base, SceSize size, SceSize *loaded);
int ksceSblFwLoaderUnlock(void);
SceUInt64 ksceKernelGetProcessTimeWideCore(void);
int sceDeci2pTermForCompat(void);
int SceDeci2pForDriver_F6427E0F(void *regs, void *shm, SceSize size);
int kscePowerSetCompatClockFrequency(int frequency);
int SceHpremoteForDriver_7E69EDA4(void);
int SceKernelSuspendForDriver_4DF40893(SceUInt32 flags);
int SceKernelSuspendForDriver_2BB92967(SceUInt32 flags);

int compatLcdcIntrHandler(int intrcode, void *context)
{
	SceDisplayResolutionInfo info;
	int head;
	int hcount;

	head = ksceDisplayGetPrimaryHead();
	if (head == 1) {
		info.size = sizeof(info);
		if (ksceDisplayGetResolutionInfoInternal(head, &info) < 0) {
#if FW_DEBUG
			info.vic = 0xffff;
#endif
			goto unsupported;
		}
		switch ((int)info.vic) {
		case 0x8100:
		case 0x8300:
			hcount = 486;
			break;
		case 0x8500:
			hcount = 1000;
			break;
		case 0x8600:
			hcount = 650;
			break;
		default:
unsupported:
#if FW_DEBUG
			ksceKernelPrintfLevelWithInfo(1, 6,
				SCE_DBGINFO_FUNC(284, "waitHCount"),
				"unsupported display mode: %x\n", info.vic);
#endif
			goto interrupt;
		}
	} else {
		hcount = 550;
	}
	while (SceDisplayForDriver_332C5410(head) >= hcount)
		;
	while (SceDisplayForDriver_332C5410(head) < hcount)
		;
interrupt:
	gCompatMailbox->enableInterrupt[10] = ~0U;
	(void)gCompatMailbox->enableInterrupt[10];
	__asm__ volatile("dsb sy" ::: "memory");
	gCompatMailbox->raiseInterrupt[10] = ~0U;
	(void)gCompatMailbox->raiseInterrupt[10];
	__asm__ volatile("dsb sy" ::: "memory");
	return -1;
}

int compatReturnIntrHandler(int intrcode, void *context)
{
	SceUID sema = (SceUID)context;
	int result;

	result = ksceKernelSignalSema(sema, 1);
	if (result)
		SCE_DBG_PRINTF(0, 392,
			"Failed to signal(error=0x%08x), intrcode [%d](ret)\n",
			result, intrcode);
	return -1;
}

int compatCallIntrHandler(int intrcode, void *context)
{
	SceCompatSharedRequest *request;
	struct CompatRequestQueue *queue;
	unsigned int queueId;
	unsigned int writeIndex;
	unsigned int count;
	int result;

	request = &gCompatSharedSram->runtime.requests[intrcode - 68];
	queueId = request->queueSelector >> 16;
	if (queueId > 15)
		return -1;
	queue = &gCompatRequestQueues[queueId];
	writeIndex = queue->writeIndex;
	count = queue->count;
	if (count <= 15) {
		queue->requestWords[writeIndex] = request->encodedRequest;
		queue->writeIndex = (writeIndex + 1) & 15;
		queue->count = count + 1;
		gCompatMailbox->enableInterrupt[intrcode - 64] = ~0U;
		(void)gCompatMailbox->enableInterrupt[intrcode - 64];
		__asm__ volatile("dsb sy" ::: "memory");
		gCompatMailbox->raiseInterrupt[intrcode - 64] = ~0U;
		(void)gCompatMailbox->raiseInterrupt[intrcode - 64];
		__asm__ volatile("dsb sy" ::: "memory");
	} else {
		queue->overflow = 1;
	}
	result = ksceKernelSignalSema(gCompatHandlerSema[queueId], 1);
#if FW_DEBUG
	if (result)
		SCE_DBG_PRINTF(0, 373,
			"Failed to signal(error=0x%08x), intrcode [%d](call)\n",
			result, intrcode);
#endif
	return -1;
}

int compatUnlockMutex(void)
{
	int result;

	result = ksceKernelUnlockMutex(gCompatMutex, 1);
	return result < 0 ? result : 0;
}

int compatCheckRif(void *args)
{
	unsigned char license[sizeof(SceNpDrmLicense)];
	unsigned char klicense[16];
	SceUInt64 startTime;
	SceUInt64 expirationTime;
	SceOff size;
	SceUID fd;
	int flags;
	int result;

	(void)args;
	memset(&license, 0, sizeof(license));
	fd = ksceIoOpen(gCompatRifPath, 1, 0);
	if (fd <= 0)
		return 0;
	size = ksceIoLseek(fd, 0, SCE_SEEK_END);
	result = (SceUInt64)(size - 1) < sizeof(license) &&
		ksceIoPread(fd, &license, size, 0) == (int)size &&
		ksceNpDrmGetRifPspKey((const SceNpDrmLicense *)license, klicense,
			&flags, &startTime,
			&expirationTime) == 0;
	ksceIoClose(fd);
	return result;
}

int compatEnterRunningState(void)
{
	int (*volatile deci2pAvailable)(void *, void *, SceSize);
	SceCompatMailboxRegisters *mailbox;
	SceCompatLcdDmaRegisters *regs;
	void *mainDdr;
	unsigned int physicalAddress;
	int result;
	int state;

	sceKernelSysrootDisableAutoClockDown();
	deci2pAvailable = SceDeci2pForDriver_F6427E0F;
	mailbox = gCompatMailbox;
	mainDdr = gCompatMainDdr;
	if (deci2pAvailable)
		SceDeci2pForDriver_F6427E0F(mailbox,
			(char *)mainDdr + 0x3d80000, 0x40000);
	memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
	result = ksceNpDrmReadActData(&gCompatActivationData);
	if (result < 0 && !ksceSblAimgrIsDEX())
		return result;
	if (!ksceKernelRunWithStack(0x4000, compatCheckRif, NULL) &&
		!ksceSblAimgrIsDEX())
		return (int)0x8001000d;
	if (ksceDisplayGetPrimaryHead() == 1) {
		SceDisplayResolutionInfo info;

		info.size = sizeof(info);
		while (ksceDisplayGetResolutionInfoInternal(1, &info) < 0)
			sceKernelDelayThread(1000);
	}
	result = SceGrabForDriver_379130B2();
	if (result < 0)
		return result;
	state = gCompatState;
	gCompatState = 2;
	gCompatPreviousState = state;
	if (gCompatFrameBuffer > 0) {
		SceKernelVARange va;
		SceKernelPARange pa;

		result = ksceKernelGetMemBlockBase(gCompatFrameBuffer,
			(void **)&va.addr);
		if (result < 0)
			return result;
		va.size = 0x88000;
		result = ksceKernelVARangeToPARange(&va, &pa);
		if (result < 0)
			return result;
		regs = gCompatLcdDma;
		physicalAddress = pa.addr;
		regs->initializationMask = 0xff0000;
		regs->initializationControl = 0;
		regs->startControl = 0;
		__asm__ volatile("dmb sy" ::: "memory");
		if (!(regs->startControl & 0x1000000) && !(regs->status & 3)) {
			regs->width = 480;
			regs->widthSetup = 0;
			regs->height = 272;
			regs->heightSetup = 0;
			regs->physicalAddress = physicalAddress;
			regs->bufferSetup = 0;
			regs->transferSetup = 0;
			regs->stride = 2048;
			regs->pixelFormat = 2;
			regs->enable = 1;
			regs->startControl = 0x1000000;
			regs->completionAck = 1;
			__asm__ volatile("dmb sy" ::: "memory");
		}
	}
	return 0;
}

int compatSysEventHandler(int event, int stage, void *args, void *opt)
{
	int i;

	if (event == 1 && stage == 0x100000) {
		for (i = 0; i != 7; ++i)
			ksceKernelEnableIntr(gCompatInterruptConfig[i].intrcode);
		ksceKernelEnableIntr(78);
		ksceKernelEnableIntr(65);
		gCompatRequestsEnabled = 0;
	}
	return 0;
}

#if !FW_DEBUG
int mapMainDDR(void)
{
	uintptr_t paddr;
	int result;

	ksceKernelVAtoPA(gCompatMainDdr, &paddr);
	SceGrabForDriver_E9C25A28(0, paddr | 1);
	ksceKernelVAtoPA((char *)gCompatMainDdr + 0x1000000, &paddr);
	SceGrabForDriver_E9C25A28(1, paddr | 1);
	ksceKernelVAtoPA((char *)gCompatMainDdr + 0x1000000, &paddr);
	SceGrabForDriver_E9C25A28(2, paddr | 1);
	ksceKernelVAtoPA((char *)gCompatMainDdr + 0x3000000, &paddr);
	SceGrabForDriver_E9C25A28(3, paddr | 1);
	result = gCompatMemoryBlocks[2][1];
	if (result > 0) {
		result = ksceKernelFreeMemBlock(result);
		gCompatMemoryBlocks[2][1] = 0;
	}
	return result;
}
#endif

int compatProcessKillHandler(SceUID pid, SceProcEventInvokeParam1 *param,
	int flags)
{
	SceUID (*group)[3];
	SceUID *block;
	int i;

	if (pid == gCompatProcessId) {
		{
			SceUInt timeout = 5000000;

			ksceKernelLockMutex(gCompatMutex, 1, &timeout);
		}
		if (gCompatState == 2) {
			int (*volatile deci2pAvailable)(void *, void *, SceSize);

			sceKernelSysrootEnableAutoClockDown();
			deci2pAvailable = SceDeci2pForDriver_F6427E0F;
			if (deci2pAvailable)
				sceDeci2pTermForCompat();
		}
		SceGrabForDriver_F13C63DD();
		gCompatPreviousState = gCompatState;
		gCompatRequestsEnabled = 0;
		gCompatState = 1;
		kscePowerSetCompatClockFrequency(0);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
		gCompatMainDdr = NULL;
		group = gCompatMemoryBlocks;
		do {
			block = *group;
			for (i = 3; i != 0; --i) {
				SceUID uid = *block++;

				if (uid > 0) {
					ksceKernelFreeMemBlock(uid);
					block[-1] = 0;
				}
			}
			++group;
		} while (group != gCompatMemoryBlocks + 3);
		ksceGUIDReleaseObject(gCompatFrameBuffer);
		gCompatSuspendSema = 0;
		gCompatFrameBuffer = 0;
		gCompatResumeSema = 0;
		gCompatPreviousState = 0;
		gCompatState = 0;
		gCompatProcessId = 0;
	}
	return 0;
}

int compatProcessExitHandler(SceUID pid, SceProcEventInvokeParam1 *param,
	int flags)
{
	SceUID (*group)[3];
	SceUID *block;
	int i;

	if (pid == gCompatProcessId) {
		{
			SceUInt timeout = 5000000;

			ksceKernelLockMutex(gCompatMutex, 1, &timeout);
		}
		if (gCompatState == 2) {
			int (*volatile deci2pAvailable)(void *, void *, SceSize);

			sceKernelSysrootEnableAutoClockDown();
			deci2pAvailable = SceDeci2pForDriver_F6427E0F;
			if (deci2pAvailable)
				sceDeci2pTermForCompat();
		}
		SceGrabForDriver_F13C63DD();
		gCompatPreviousState = gCompatState;
		gCompatRequestsEnabled = 0;
		gCompatState = 1;
		kscePowerSetCompatClockFrequency(0);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
		gCompatMainDdr = NULL;
		group = gCompatMemoryBlocks;
		do {
			block = *group;
			for (i = 3; i != 0; --i) {
				SceUID uid = *block++;

				if (uid > 0) {
					ksceKernelFreeMemBlock(uid);
					block[-1] = 0;
				}
			}
			++group;
		} while (group != gCompatMemoryBlocks + 3);
		ksceGUIDReleaseObject(gCompatFrameBuffer);
		gCompatSuspendSema = 0;
		gCompatFrameBuffer = 0;
		gCompatResumeSema = 0;
		gCompatPreviousState = 0;
		gCompatState = 0;
		gCompatProcessId = 0;
	}
	return 0;
}

int module_start(SceSize arglen, const void *argp)
{
	SceKernelAllocMemBlockKernelOpt allocOpt;
	SceKernelMutexOptParam mutexOpt;
	const struct CompatInterruptConfig *config;
	SceUID procHandler;
	SceUID memblock;
	SceUID result;
	SceUID *sema;
	char name[32];
	int i;

	gCompatRifPath[0] = 0;
	memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
	gCompatRtcDeadline.tick = 0;
	memset(&allocOpt, 0, sizeof(allocOpt));
	allocOpt.size = sizeof(allocOpt);
	allocOpt.attr = 2;
	allocOpt.paddr = COMPAT_LCDDMA_BASE;
	memblock = ksceKernelAllocMemBlock("SceCompatLCDDMA", 0x20100806,
		0x1000, &allocOpt);
	if (memblock < 0) {
		gCompatLcdDmaMemBlock = memblock;
		goto cleanup;
	}
	if (ksceKernelGetMemBlockBase(memblock, (void **)&gCompatLcdDma)) {
		result = ksceKernelFreeMemBlock(memblock);
		gCompatLcdDmaMemBlock = result;
		if (result < 0)
			goto cleanup;
	} else {
		gCompatLcdDmaMemBlock = memblock;
	}
	memset(&allocOpt, 0, sizeof(allocOpt));
	allocOpt.size = sizeof(allocOpt);
	allocOpt.attr = 2;
	allocOpt.paddr = COMPAT_MAILBOX_BASE;
	memblock = ksceKernelAllocMemBlock("SceCompatMailbox", 0x20100206,
		0x1000, &allocOpt);
	if (memblock < 0) {
		gCompatMailboxMemBlock = memblock;
		goto cleanup;
	}
	if (ksceKernelGetMemBlockBase(memblock, (void **)&gCompatMailbox)) {
		result = ksceKernelFreeMemBlock(memblock);
		gCompatMailboxMemBlock = result;
		if (result < 0)
			goto cleanup;
	} else {
		gCompatMailboxMemBlock = memblock;
	}
	memset(&allocOpt, 0, sizeof(allocOpt));
	allocOpt.size = sizeof(allocOpt);
	allocOpt.attr = 2;
	allocOpt.paddr = COMPAT_SHARED_SRAM_BASE;
	memblock = ksceKernelAllocMemBlock("SceCompatSharedSram", 0x20100806,
		0x1000, &allocOpt);
	if (memblock < 0) {
		gCompatSharedSramMemBlock = memblock;
		goto cleanup;
	}
	if (ksceKernelGetMemBlockBase(memblock, (void **)&gCompatSharedSram)) {
		gCompatSharedSramMemBlock = ksceKernelFreeMemBlock(memblock);
		if (gCompatSharedSramMemBlock < 0)
			goto cleanup;
	} else {
		gCompatSharedSramMemBlock = memblock;
	}
	ksceKernelRegisterSysEventHandler("SceCompat", compatSysEventHandler, NULL);
	procHandler = ksceKernelRegisterProcEventHandler("SceCompat",
		&gCompatProcessHandlers, 0);
	if (procHandler < 0)
		goto cleanup;
	mutexOpt.size = sizeof(mutexOpt);
	mutexOpt.ceilingPriority = 32;
	gCompatMutex = ksceKernelCreateMutex("SceCompat", 4, 0, &mutexOpt);
	if (gCompatMutex < 0 ||
		(gCompatExitMutex = ksceKernelCreateMutex("SceCompatExitCheck",
			0, 0, NULL)) < 0) {
		ksceKernelUnregisterProcEventHandler(procHandler);
		goto cleanup;
	}
	result = ksceKernelCreateSema("SceCompatVirtualHandler", 0xa000,
		1, 1, NULL);
	if (result < 0)
		goto cleanup;
	gCompatHandlerSema[0] = result;
	for (i = 1; i != 16; ++i) {
		snprintf(name, sizeof(name), "SceCompatHandler%d", i);
		result = ksceKernelCreateSema(name, 0x8000, 0, 0x7fffffff, NULL);
		if (result < 0)
			goto cleanup;
		gCompatHandlerSema[i] = result;
	}
	config = gCompatInterruptConfig;
	sema = gCompatReturnSema;
	do {
		if (ksceKernelRegisterIntrHandler(config->intrcode, config->name,
			0, compatCallIntrHandler, NULL, config->priority,
			config->target_cpu, &gCompatInterruptOptions) ||
			ksceKernelEnableIntr(config->intrcode))
			goto cleanup;
		result = ksceKernelCreateSema(config[3].name, 0x8000, 1, 1, NULL);
		if (result < 0)
			goto cleanup;
		*sema++ = result;
		if (ksceKernelRegisterIntrHandler(config[3].intrcode, config[3].name,
			0, compatReturnIntrHandler,
			(void *)result, config[3].priority, config[3].target_cpu,
			&gCompatInterruptOptions) ||
			ksceKernelEnableIntr(config[3].intrcode))
			goto cleanup;
		++config;
	} while (sema != gCompatReturnSema + 3);
	if (!ksceKernelRegisterIntrHandler(74, "SceCompatLcdc", 0,
		compatLcdcIntrHandler, NULL, 160, 1,
		&gCompatInterruptOptions) && !ksceKernelEnableIntr(74))
		return 0;
cleanup:
	sema = gCompatHandlerSema;
	do {
		result = *sema++;
		if (result > 0)
			ksceKernelDeleteSema(result);
	} while (sema != gCompatHandlerSema + 16);
	sema = gCompatReturnSema;
	config = gCompatInterruptConfig;
	do {
		result = *sema++;
		if (result > 0)
			ksceKernelDeleteSema(result);
		ksceKernelReleaseIntrHandler(config->intrcode);
		++config;
		ksceKernelReleaseIntrHandler(config[2].intrcode);
	} while (sema != gCompatReturnSema + 3);
	if (gCompatSharedSramMemBlock > 0) {
		ksceKernelFreeMemBlock(gCompatSharedSramMemBlock);
		gCompatSharedSramMemBlock = 0;
	}
	if (gCompatMailboxMemBlock > 0) {
		ksceKernelFreeMemBlock(gCompatMailboxMemBlock);
		gCompatMailboxMemBlock = 0;
	}
	if (gCompatLcdDmaMemBlock > 0) {
		ksceKernelFreeMemBlock(gCompatLcdDmaMemBlock);
		gCompatLcdDmaMemBlock = 0;
	}
	return 2;
}

int compatInitEx(int mode)
{
	SceSize loaded;
	int result;
	int status;

	if (sceKernelGetProcessId() != gCompatProcessId)
		return (int)0x8002f001;
	memset(gCompatRequestQueues, 0, sizeof(gCompatRequestQueues));
	gCompatPreviousState = 0;
	gCompatState = 0;
	if (!gCompatMainDdr)
		return (int)0x80027205;
#if FW_DEBUG
	{
		uintptr_t paddr;

		result = ksceKernelVAtoPA(gCompatMainDdr, &paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(0, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		result = ksceKernelVAtoPA((char *)gCompatMainDdr + 0x1000000,
			&paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(1, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		result = ksceKernelVAtoPA((char *)gCompatMainDdr + 0x1000000,
			&paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(2, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		result = ksceKernelVAtoPA((char *)gCompatMainDdr + 0x3000000,
			&paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(3, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		if (gCompatMemoryBlocks[2][1] > 0) {
			ksceKernelFreeMemBlock(gCompatMemoryBlocks[2][1]);
			gCompatMemoryBlocks[2][1] = 0;
		}
	}
#else
	mapMainDDR();
#endif
	result = ksceSblFwLoaderLock(gCompatPspFirmwarePath, 0);
	if (result < 0)
		return result;
	status = ksceSblFwLoaderLoad(1, (char *)gCompatMainDdr + 0x3000000,
		0x2000000, &loaded);
	ksceSblFwLoaderUnlock();
	if (status < 0)
		return status;
	ksceKernelDcacheCleanRangeForL1WBWA((char *)gCompatMainDdr + 0x3000000,
		loaded);
	result = ksceSblFwLoaderLock(gCompatBootFirmwarePath, 0);
	if (result < 0)
		return result;
	status = ksceSblFwLoaderLoad(1, (char *)gCompatMainDdr + 0x600000,
		0xa00000, &loaded);
	ksceSblFwLoaderUnlock();
	ksceKernelDcacheCleanRangeForL1WBWA((char *)gCompatMainDdr + 0x600000,
		loaded);
#if FW_VERSION >= 0x03650000
	{
		unsigned int *bootRandomWord;

		/* A complete guest scalar, addressed through the main-DDR mapping. */
		bootRandomWord = (unsigned int *)((char *)gCompatMainDdr + 0x3fff00);
		__builtin_memcpy(bootRandomWord, &gCompatBootRandom,
			sizeof(*bootRandomWord));
		ksceKernelDcacheCleanRangeForL1WBWA(
			(char *)gCompatMainDdr + 0x3fff00, 32);
	}
#endif
	if (status < 0)
		return status;
	result = kscePowerSetCompatClockFrequency(333);
	if (result < 0)
		return result;
	gCompatMode = mode;
	result = SceGrabForDriver_072B8D93(mode);
	if (result >= 0) {
		result = compatSecureInitialize(0);
		if (result >= 0)
			return 0;
		SceGrabForDriver_F13C63DD();
		kscePowerSetCompatClockFrequency(0);
		SCE_DBG_PRINTF(1, 931,
			"sceCompatSecLoadSCBootCode failed (0x%08x)\n", result);
		return result;
	}
	kscePowerSetCompatClockFrequency(0);
	return result;
}

int compatSetRifPath(const signed char *contentId)
{
	int valid;
	int i;

	if (sceKernelGetProcessId() != gCompatProcessId)
		return (int)0x8002f001;
	valid = 1;
	for (i = 0; i != 36; ++i) {
		if (!contentId[i])
			valid = 0;
	}
	if (valid) {
		strncpy(gCompatRifPath, gCompatRifPathTemplate, 255);
		__builtin_memcpy(gCompatRifPath + sizeof("ux0:pspemu/PSP/LICENSE/") - 1,
			contentId, 36);
	} else {
		gCompatRifPath[0] = 0;
	}
	return 0;
}

int compatCheckPocketStationRif(void)
{
	_Alignas(SceNpDrmLicense) unsigned char license[sizeof(SceNpDrmLicense)];
	unsigned char klicense[16];
	SceUInt64 startTime;
	SceUInt64 expirationTime;
	SceRtcTick tick;
	char contentId[48];
	SceOff size;
	SceUID fd;
	int flags;
	int result;

	memset(&license, 0, sizeof(license));
	fd = ksceIoOpen(gCompatPocketStationRifPath, 1, 0);
	if (fd <= 0) {
		result = 0;
		goto out;
	}
	size = ksceIoLseek(fd, 0, SCE_SEEK_END);
	if ((SceUInt64)(size - 1) >= sizeof(license) ||
		ksceIoPread(fd, &license, size, 0) != (int)size ||
		ksceNpDrmGetRifPspKey((const SceNpDrmLicense *)license, klicense,
			&flags, &startTime,
			&expirationTime) ||
		(expirationTime != 0x7fffffffffffffffULL &&
			(ksceRtcGetCurrentSecureTick(&tick) ||
			 startTime >= tick.tick || tick.tick >= expirationTime))) {
		ksceIoClose(fd);
		result = 0;
		goto out;
	}
	ksceIoClose(fd);
	result = ksceNpDrmGetRifInfo((const SceNpDrmLicense *)license,
		sizeof(license), 0, contentId,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) >= 0 &&
		strncmp(contentId, gCompatPocketStationContentId,
			sizeof(contentId)) == 0;
out:
	return result;
}

int compatStart(void)
{
	int result;

	if (sceKernelGetProcessId() != gCompatProcessId)
		return (int)0x8002f001;
	result = compatEnterRunningState();
	if (result < 0) {
		{
			SceUInt timeout = 5000000;

			ksceKernelLockMutex(gCompatMutex, 1, &timeout);
		}
		if (gCompatState == 2) {
			int (*volatile deci2pAvailable)(void *, void *, SceSize);

			sceKernelSysrootEnableAutoClockDown();
			deci2pAvailable = SceDeci2pForDriver_F6427E0F;
			if (deci2pAvailable)
				sceDeci2pTermForCompat();
		}
		SceGrabForDriver_F13C63DD();
		gCompatPreviousState = gCompatState;
		gCompatRequestsEnabled = 0;
		gCompatState = 1;
		kscePowerSetCompatClockFrequency(0);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
		return result;
	}
	return 0;
}

int compatStop(void)
{
	int result;

	{
		SceUInt timeout = 5000000;

		ksceKernelLockMutex(gCompatMutex, 1, &timeout);
	}
	if (gCompatState == 2) {
		int (*volatile deci2pAvailable)(void *, void *, SceSize);

		sceKernelSysrootEnableAutoClockDown();
		deci2pAvailable = SceDeci2pForDriver_F6427E0F;
		if (deci2pAvailable)
			sceDeci2pTermForCompat();
	}
	result = SceGrabForDriver_F13C63DD();
	gCompatPreviousState = gCompatState;
	gCompatRequestsEnabled = 0;
	gCompatState = 1;
	kscePowerSetCompatClockFrequency(0);
	ksceKernelUnlockMutex(gCompatMutex, 1);
	memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
	return result;
}

int compatWaitAndGetRequest(int mode, int id)
{
	struct CompatRequestQueue *queue;
	SceUInt timeout;
	SceUInt32 intrState;
	unsigned int readIndex;
	unsigned int writeIndex;
	unsigned int count;
	unsigned int request;
	int result;

	if ((unsigned int)mode > 15)
		goto invalid;
	if ((unsigned int)id > 2)
		goto invalid;
	result = ksceKernelWaitSema(gCompatHandlerSema[mode], 1, NULL);
	if (result < 0)
		goto out;
	timeout = 5000000;
	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0) {
		result = (int)0x80028005;
		goto out;
	}
	intrState = ksceKernelCpuSuspendIntr();
	queue = &gCompatRequestQueues[mode];
	readIndex = queue->readIndex;
	count = queue->count;
	request = queue->requestWords[readIndex];
	queue->readIndex = (readIndex + 1) & 15;
	if (!count) {
		ksceKernelCpuResumeIntr(intrState);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		result = (int)0x80010016;
		goto out;
	}
	queue->count = count - 1;
	if (queue->overflow) {
		writeIndex = queue->writeIndex;
		queue->requestWords[writeIndex] =
			gCompatSharedSram->runtime.requests[id].encodedRequest;
		queue->writeIndex = (writeIndex + 1) & 15;
		gCompatMailbox->enableInterrupt[id + 4] = ~0U;
		(void)gCompatMailbox->enableInterrupt[id + 4];
		__asm__ volatile("dsb sy" ::: "memory");
		gCompatMailbox->raiseInterrupt[id + 4] = ~0U;
		(void)gCompatMailbox->raiseInterrupt[id + 4];
		__asm__ volatile("dsb sy" ::: "memory");
		queue->overflow = 0;
	}
	ksceKernelCpuResumeIntr(intrState);
	ksceKernelUnlockMutex(gCompatMutex, 1);
	ksceKernelDcacheInvalidateRangeForL1WBWA(
		(char *)gCompatMainDdr + (request & 0x3ffffff), 128);
	__asm__ volatile("dsb sy" ::: "memory");
	result = request >> 1;
	goto out;
invalid:
	result = (int)0x80020005;
out:
	return result;
}

int compatWaitSpecialRequest(int mode)
{
	unsigned int *requestState;
	unsigned int state;
	SceUInt64 start;
	SceUInt timeout;
	int result;
	int intrcode;

	if (mode == 1) {
		ksceKernelDcacheInvalidateRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = *requestState;
		start = ksceKernelGetProcessTimeWideCore();
		while (state != 1) {
			sceKernelDelayThread(100);
			ksceKernelDcacheInvalidateRangeForL1WBWA(
				(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
			requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
			state = *requestState;
			if (ksceKernelGetProcessTimeWideCore() - start > 5000000)
				goto timedOut;
			if (!gCompatProcessId)
				goto terminated;
		}
		timeout = 5000000;
		result = ksceKernelLockMutex(gCompatMutex, 1, &timeout);
		if (result < 0)
			goto timedOut;
		result = compatSecureResume();
		if (result != 0) {
			ksceKernelUnlockMutex(gCompatMutex, 1);
			goto out;
		}
		gCompatRequestsEnabled = 1;
		ksceKernelUnlockMutex(gCompatMutex, 1);
		for (intrcode = 64; intrcode != 96; ++intrcode) {
			if (intrcode != 65 && intrcode != 78)
				gCompatMailbox->enableInterrupt[intrcode - 64] = ~0U;
		}
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = 2;
		__builtin_memcpy(requestState, &state, sizeof(state));
		ksceKernelDcacheCleanRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		ksceKernelDcacheInvalidateRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = *requestState;
		start = ksceKernelGetProcessTimeWideCore();
		while (state != 3) {
			sceKernelDelayThread(100);
			ksceKernelDcacheInvalidateRangeForL1WBWA(
				(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
			requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
			state = *requestState;
			if (ksceKernelGetProcessTimeWideCore() - start > 5000000)
				goto timedOut;
			if (!gCompatProcessId)
				goto terminated;
		}
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = 0;
		__builtin_memcpy(requestState, &state, sizeof(state));
		ksceKernelDcacheCleanRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		goto verified;
	}
	if (mode < 1 || mode > 3) {
#if FW_DEBUG
		ksceKernelAssert(0,
			SCE_DBGINFO_FUNC(1398, "sceCompatWaitSpecialRequest"),
			__builtin_return_address(0));
#endif
		goto verified;
	}
	ksceKernelDcacheInvalidateRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = *requestState;
	start = ksceKernelGetProcessTimeWideCore();
	while (state != 1) {
		sceKernelDelayThread(100);
		result = ksceKernelLockMutex(gCompatExitMutex, 1, NULL);
		if (result < 0)
			goto out;
		if (!gCompatMainDdr) {
			result = ksceKernelUnlockMutex(gCompatExitMutex, 1);
			if (result < 0)
				goto out;
			goto terminated;
		}
		ksceKernelDcacheInvalidateRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = *requestState;
		result = ksceKernelUnlockMutex(gCompatExitMutex, 1);
		if (result < 0)
			goto out;
		if (ksceKernelGetProcessTimeWideCore() - start > 30000000)
			goto timedOut;
		if (!gCompatProcessId)
			goto terminated;
	}
	result = ksceKernelLockMutex(gCompatExitMutex, 1, NULL);
	if (result < 0)
		goto out;
	if (!gCompatMainDdr) {
		result = ksceKernelUnlockMutex(gCompatExitMutex, 1);
		if (result < 0)
			goto out;
		goto terminated;
	}
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = 0;
	__builtin_memcpy(requestState, &state, sizeof(state));
	ksceKernelDcacheCleanRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	result = ksceKernelUnlockMutex(gCompatExitMutex, 1);
	if (result < 0)
		goto out;
verified:
#if FW_DEBUG
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	ksceKernelAssert(*requestState == 0,
		&gCompatWaitSpecialRequestInfo,
		__builtin_return_address(0));
#endif
	result = 0;
	goto out;
timedOut:
	result = (int)0x80028005;
	goto out;
terminated:
	result = (int)0x80028007;
out:
	return result;
}

int sceCompatWaitIntr(unsigned int index)
{
	SceUInt32 savedContext;
	int result;

	if (index > 2)
		return (int)0x80020005;
	savedContext = sceSyscallEnter();
	result = ksceKernelWaitSema(gCompatReturnSema[index], 1, NULL);
	sceSyscallLeave(savedContext);
	return result;
}

int compatReturnValueEx(SceUInt32 channel, const SceUInt32 values[2],
	SceUInt64 value)
{
	volatile SceCompatSharedReturn *response;
	SceUInt32 firstValue = values[0];
	SceUInt32 secondValue = values[1];
	SceUInt timeout;
	int result;

	if (channel > 2)
		return (int)0x80020005;
	timeout = 5000000;
	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		return (int)0x80028005;
	if (!gCompatRequestsEnabled) {
		result = (int)0x80027205;
		goto unlock;
	}
	response = &gCompatSharedSram->runtime.returns[channel];
	if ((uintptr_t)gCompatSharedSram > (uintptr_t)response ||
		(uintptr_t)response > (uintptr_t)gCompatSharedSram +
		sizeof(*gCompatSharedSram) - sizeof(response->value)) {
		result = (int)0x8002000b;
		goto unlock;
	}
	response->value = value;
	response->firstValue = firstValue;
	response->secondValue = secondValue;
	(void)response->value;
	__asm__ volatile("dsb sy" ::: "memory");
	__asm__ volatile("dsb sy" ::: "memory");
	gCompatMailbox->enableInterrupt[channel + 7] = ~0U;
	(void)gCompatMailbox->enableInterrupt[channel + 7];
	__asm__ volatile("dsb sy" ::: "memory");
	gCompatMailbox->raiseInterrupt[channel + 7] = ~0U;
	(void)gCompatMailbox->raiseInterrupt[channel + 7];
	__asm__ volatile("dsb sy" ::: "memory");
	result = 0;
unlock:
	ksceKernelUnlockMutex(gCompatMutex, 1);
	return result;
}

int sceCompatCacheInternal(int mode, void *base, int size)
{
	uintptr_t address = (uintptr_t)base;
	int remainder;

	if ((unsigned int)(size - 1) >= 0x4000000)
		return (int)0x8002000b;
	if (address - gCompatCacheRangeBase >= 0x4000000 ||
		address + size > gCompatCacheRangeBase + 0x4000000)
		return (int)0x80020006;
	remainder = address & 63;
	if (remainder) {
		size += remainder;
		address -= remainder;
	}
	if (size & 63)
		size = size + 64 - (size & 63);
	if (mode == 2) {
		ksceKernelDcacheCleanRangeForL1WBWA((void *)address, size);
	} else if (mode == 3) {
		ksceKernelDcacheCleanInvalidateRangeForL1WBWA((void *)address, size);
	} else if (mode == 1) {
		ksceKernelDcacheInvalidateRangeForL1WBWA((void *)address, size);
	} else {
#if FW_DEBUG
		ksceKernelAssert(0, SCE_DBGINFO_FUNC(1535, "sceCompatCache"),
			__builtin_return_address(0));
#endif
	}
	return 0;
}

int compatAllocCdramWithHole(SceCompatCdram *cdram)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID (*group)[3];
	SceUID *block;
	SceUInt timeout;
	SceSize size;
	int result;
	int i;
#if FW_VERSION >= 0x03650000
	unsigned int bootRandom;

	result = ksceSblRngPseudoRandomNumber(&bootRandom, sizeof(bootRandom));
	if (result < 0)
		return result;
#endif
	timeout = 5000000;
	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		return (int)0x80028005;
	if (gCompatProcessId) {
		ksceKernelUnlockMutex(gCompatMutex, 1);
		return (int)0x80010078;
	}
	gCompatProcessId = sceKernelGetProcessId();
#if FW_VERSION >= 0x03650000
	gCompatBootRandom = bootRandom;
#endif
	ksceKernelUnlockMutex(gCompatMutex, 1);
	for (i = 0; i != 3; ++i) {
		memset(&opt, 0, sizeof(opt));
		size = i ? 0x1000000 : 0x2000000;
		opt.size = sizeof(opt);
		opt.attr = 0x400084;
		opt.alignment = 0x1000000;
		result = ksceKernelAllocMemBlock(gCompatCachedName, 0x09404060,
			size, &opt);
		if (result < 0)
			goto failed;
		gCompatMemoryBlocks[0][i] = result;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x10000c0;
		opt.alignment = 0x1000000;
		opt.mirror_blockid = result;
		result = ksceKernelAllocMemBlock(gCompatUncachedName, 0x0c208060,
			size, &opt);
		if (result < 0)
			goto failed;
		gCompatMemoryBlocks[1][i] = result;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x10000c0;
		opt.alignment = 0x1000000;
		opt.mirror_blockid = gCompatMemoryBlocks[0][i];
		result = ksceKernelAllocMemBlock(gCompatCdramName, 0x40404006,
			size, &opt);
		if (result < 0)
			goto failed;
		gCompatMemoryBlocks[2][i] = result;
	}
	ksceKernelGetMemBlockBase(gCompatMemoryBlocks[0][0],
		&cdram->cached_cdram);
	ksceKernelGetMemBlockBase(gCompatMemoryBlocks[0][2],
		&cdram->uncached_cdram);
	if ((uintptr_t)cdram->uncached_cdram !=
		(uintptr_t)cdram->cached_cdram + 0x3000000) {
		result = (int)0x80010078;
		goto failed;
	}
	ksceKernelGetMemBlockBase(gCompatMemoryBlocks[1][0],
		&cdram->uncached_cdram);
	gCompatCacheRangeBase = (uintptr_t)cdram->cached_cdram;
	ksceKernelGetMemBlockBase(gCompatMemoryBlocks[2][0], &gCompatMainDdr);
	ksceKernelUnmapMemBlock(gCompatMemoryBlocks[0][1]);
	ksceKernelUnmapMemBlock(gCompatMemoryBlocks[1][1]);
	return 0;
failed:
	gCompatMainDdr = NULL;
	group = gCompatMemoryBlocks;
	do {
		block = *group;
		for (i = 3; i != 0; --i) {
			SceUID uid = *block++;

			if (uid > 0) {
				ksceKernelFreeMemBlock(uid);
				block[-1] = 0;
			}
		}
		++group;
	} while (group != gCompatMemoryBlocks + 3);
	return result;
}

int compatInterrupt(int intrCode)
{
	volatile SceUInt32 *pending;
	SceUInt64 start;
	SceUInt timeout;
	int result;

	if ((unsigned int)intrCode - 1 > 14) {
		result = (int)0x80010016;
		goto out;
	}
	timeout = 5000000;
	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0) {
		result = (int)0x80028005;
		goto out;
	}
	if (!gCompatRequestsEnabled) {
		ksceKernelUnlockMutex(gCompatMutex, 1);
		result = (int)0x80010086;
		goto out;
	}
	pending = &gCompatSharedSram->runtime.interrupts[intrCode].pending;
	start = ksceKernelGetProcessTimeWideCore();
	while (ksceKernelGetProcessTimeWideCore() - start <= 1000000) {
		if (!*pending)
			break;
	}
	*pending = 1;
	(void)*pending;
	__asm__ volatile("dsb sy" ::: "memory");
	gCompatMailbox->enableInterrupt[intrCode + 16] = ~0U;
	(void)gCompatMailbox->enableInterrupt[intrCode + 16];
	__asm__ volatile("dsb sy" ::: "memory");
	gCompatMailbox->raiseInterrupt[intrCode + 16] = ~0U;
	(void)gCompatMailbox->raiseInterrupt[intrCode + 16];
	__asm__ volatile("dsb sy" ::: "memory");
	ksceKernelUnlockMutex(gCompatMutex, 1);
	result = 0;
out:
	return result;
}

int compatProcessStopHandler(SceUID pid, int event,
	SceProcEventInvokeParam1 *param, int flags)
{
	SceCompatLcdDmaRegisters *lcdDma;
	unsigned int *requestState;
	unsigned int state;
	SceUInt64 start;
	SceUInt timeout;
	int result;

	if (pid != gCompatProcessId || event != 0x400)
		return 0;
	if (gCompatSuspendSema &&
		ksceKernelSignalSema(gCompatSuspendSema, 1) == 0 &&
		gCompatResumeSema)
		ksceKernelWaitSema(gCompatResumeSema, 1, NULL);
	if (gCompatState != 2)
		goto color;
	gCompatResumeCookie = 0;
	if (gCompatFrameBuffer) {
		timeout = 5000000;
		if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
			goto color;
		if (!gCompatRequestsEnabled) {
			compatUnlockMutex();
			goto color;
		}
		lcdDma = gCompatLcdDma;
		lcdDma->stopControl = 0x1000000;
		start = ksceKernelGetProcessTimeWideCore();
		while (!(lcdDma->stopControl & 0x1000000)) {
			if (ksceKernelGetProcessTimeWideCore() - start > 5000000 ||
				!gCompatProcessId)
				goto hardwareFailed;
		}
		lcdDma->stopCommand = 1;
		start = ksceKernelGetProcessTimeWideCore();
		while (lcdDma->activeStatus) {
			sceKernelDelayThread(100);
			if (ksceKernelGetProcessTimeWideCore() - start > 5000000 ||
				!gCompatProcessId)
				goto hardwareFailed;
		}
		lcdDma->stopControl = 0x11100000;
		start = ksceKernelGetProcessTimeWideCore();
		while (lcdDma->stopControl) {
			sceKernelDelayThread(100);
			if (ksceKernelGetProcessTimeWideCore() - start > 5000000 ||
				!gCompatProcessId)
				goto hardwareFailed;
		}
		result = ksceDisplaySetFrameBuf(NULL, 1);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		if (result != 0)
			goto color;
	}
	if (compatInterrupt(10) != 0)
		goto color;
	ksceKernelDcacheInvalidateRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = *requestState;
	start = ksceKernelGetProcessTimeWideCore();
	while (state != 1) {
		sceKernelDelayThread(100);
		if (ksceKernelLockMutex(gCompatExitMutex, 1, NULL) < 0)
			goto color;
		if (!gCompatMainDdr)
			goto unavailable;
		ksceKernelDcacheInvalidateRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = *requestState;
		if (ksceKernelUnlockMutex(gCompatExitMutex, 1) < 0)
			goto color;
		if (ksceKernelGetProcessTimeWideCore() - start > 30000000 ||
			!gCompatProcessId)
			goto color;
	}
	if (ksceKernelLockMutex(gCompatExitMutex, 1, NULL) < 0)
		goto color;
	if (!gCompatMainDdr)
		goto unavailable;
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = 0;
	__builtin_memcpy(requestState, &state, sizeof(state));
	ksceKernelDcacheCleanRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	if (ksceKernelUnlockMutex(gCompatExitMutex, 1) < 0)
		goto color;
#if FW_DEBUG
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	ksceKernelAssert(*requestState == 0,
		&gCompatWaitSpecialRequestInfo,
		__builtin_return_address(0));
#endif
	timeout = 5000000;
	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		goto color;
	gCompatResumeCookie = gCompatSharedSram->runtime.resumeCookie;
	ksceKernelUnlockMutex(gCompatMutex, 1);
	if (gCompatResumeCookie) {
		timeout = 5000000;
		ksceKernelLockMutex(gCompatMutex, 1, &timeout);
		if (gCompatState == 2) {
			int (*volatile deci2pAvailable)(void *, void *, SceSize);

			sceKernelSysrootEnableAutoClockDown();
			deci2pAvailable = SceDeci2pForDriver_F6427E0F;
			if (deci2pAvailable)
				sceDeci2pTermForCompat();
		}
		SceGrabForDriver_F13C63DD();
		gCompatPreviousState = gCompatState;
		gCompatRequestsEnabled = 0;
		gCompatState = 1;
		kscePowerSetCompatClockFrequency(0);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
	}
color:
	ksceDisplaySetColorSpaceMode(0, gCompatColorSpaceMode);
	return 0;
hardwareFailed:
	ksceKernelUnlockMutex(gCompatMutex, 1);
	goto color;
unavailable:
	ksceKernelUnlockMutex(gCompatExitMutex, 1);
	goto color;
}

int compatSetDisplayConfig(int resolution, int colorSpaceMode)
{
	DisplayFrameBufParam param;
	void *base;
	int result;

	if (gCompatFrameBuffer <= 0)
		return (int)0x80027205;
	result = ksceKernelGetMemBlockBase(gCompatFrameBuffer, &base);
	if (result < 0)
		return result;
	param.resolution = resolution ? 0x100000 : 0;
	param.frameBuf.base = base;
	param.frameBuf.pixelformat = 0;
	param.frameBuf.size = sizeof(param);
	param.frameBuf.pitch = 512;
	param.frameBuf.width = 480;
	param.frameBuf.height = 272;
	result = ksceDisplaySetFrameBuf(&param.frameBuf, 1);
	if (result < 0)
		return result;
	return ksceDisplaySetColorSpaceMode(0, colorSpaceMode);
}

int sceCompatGetPrimaryHead(void)
{
	unsigned int syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = ksceDisplayGetPrimaryHead();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int compatFrameBufferInit(SceUID frameBuffer)
{
	SceUInt timeout = 5000000;
	SceUID guid;
	int result;

	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		return (int)0x80028005;
	if (gCompatFrameBuffer) {
		ksceKernelUnlockMutex(gCompatMutex, 1);
		return (int)0x80010078;
	}
	guid = kscePUIDtoGUID(0, frameBuffer);
	if (guid < 0) {
		ksceKernelUnlockMutex(gCompatMutex, 1);
		return guid;
	}
	result = ksceGUIDReferObject(guid, NULL);
	if (result < 0) {
		ksceKernelUnlockMutex(gCompatMutex, 1);
		return result;
	}
	gCompatFrameBuffer = guid;
	ksceKernelUnlockMutex(gCompatMutex, 1);
	return 0;
}

int compatIdStorageLookup(short leaf, unsigned int offset, void *buffer,
	unsigned int size)
{
	unsigned short storageLeaf;

	if (sceKernelGetProcessId() != gCompatProcessId)
		return (int)0x8002f001;
	memset(buffer, 0, 512);
	storageLeaf = leaf - 256;
	if (storageLeaf > 63)
		return (int)0x80000100;
	return ksceIdStorageLookup(storageLeaf, offset, buffer, size);
}

int compatDatRead(int index, unsigned int offset, void *buffer,
	unsigned int size)
{
	if (sceKernelGetProcessId() != gCompatProcessId)
		return (int)0x8002f001;
	memcpy(buffer, (const unsigned char *)&gCompatActivationData + offset,
		size);
	return 0;
}

int compatReadShared32(int location, int *value)
{
	SceUInt timeout = 5000000;
	int result;

	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		return (int)0x80028005;
	result = (int)0x80027205;
	if (gCompatRequestsEnabled) {
		switch (location) {
		case 0:
			result = 0;
			*value = gCompatSharedSram->runtime.sharedValue;
			break;
		case 1:
			result = 0;
			*value = gCompatSharedSram->runtime.sharedSecondaryValue;
			break;
		case 2:
			result = 0;
			*value = gCompatSharedSram->runtime.sharedTertiaryValue;
			break;
		case 4:
			result = 0;
			*value = gCompatSharedSram->runtime.guestToHostValues[0];
			break;
		case 5:
			result = 0;
			*value = gCompatSharedSram->runtime.guestToHostValues[1];
			break;
		case 6:
			result = 0;
			*value = gCompatSharedSram->runtime.guestToHostValues[2];
			break;
		case 7:
			result = 0;
			*value = gCompatSharedSram->runtime.guestToHostValue;
			break;
		default:
			result = (int)0x80020005;
			break;
		}
	}
	ksceKernelUnlockMutex(gCompatMutex, 1);
	return result;
}

int compatWriteSharedCtrl(const SceCtrlDataPsp *padData)
{
	volatile SceCompatSharedSram *shared = gCompatSharedSram;
	volatile SceCompatSharedSram *published;
	SceUInt timeout = 5000000;
	SceUInt32 timestamp;
	int result;

	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		return (int)0x80028005;
	if (gCompatRequestsEnabled) {
		timestamp = padData->TimeStamp;
		shared->runtime.controller.TimeStamp = timestamp;
		(void)shared->runtime.controller.TimeStamp;
		__asm__ volatile("dsb sy" ::: "memory");
		shared->runtime.controller.Buttons = padData->Buttons;
		__builtin_memcpy((void *)&shared->runtime.controller.Lx,
			&padData->Lx, sizeof(SceUInt32));
		(void)shared->runtime.controller.TimeStamp;
		__asm__ volatile("dsb sy" ::: "memory");
		published = gCompatSharedSram;
		published->runtime.publishedTimestamp = timestamp;
		(void)published->runtime.publishedTimestamp;
		__asm__ volatile("dsb sy" ::: "memory");
		(void)shared->runtime.controller.TimeStamp;
		__asm__ volatile("dsb sy" ::: "memory");
		result = 0;
	} else {
		result = (int)0x80027205;
	}
	ksceKernelUnlockMutex(gCompatMutex, 1);
	return result;
}

int compatWriteShared32(int location, int value)
{
	volatile SceCompatSharedSram *shared;
	SceUInt timeout = 5000000;
	int result;

	if (ksceKernelLockMutex(gCompatMutex, 1, &timeout) < 0)
		return (int)0x80028005;
	result = (int)0x80027205;
	if (gCompatRequestsEnabled) {
		switch (location) {
		case 128:
			shared = gCompatSharedSram;
			shared->runtime.sharedValue = value;
			(void)shared->runtime.sharedValue;
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 129:
			shared = gCompatSharedSram;
			shared->runtime.hostToGuestValues[0] = value;
			(void)shared->runtime.hostToGuestValues[0];
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 130:
			shared = gCompatSharedSram;
			shared->runtime.hostToGuestValues[1] = value;
			(void)shared->runtime.hostToGuestValues[1];
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 131:
			shared = gCompatSharedSram;
			shared->runtime.sharedSecondaryValue = value;
			(void)shared->runtime.sharedSecondaryValue;
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 132:
			shared = gCompatSharedSram;
			shared->runtime.secondaryHostToGuestValues[0] = value;
			(void)shared->runtime.secondaryHostToGuestValues[0];
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 133:
			shared = gCompatSharedSram;
			shared->runtime.secondaryHostToGuestValues[1] = value;
			(void)shared->runtime.secondaryHostToGuestValues[1];
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 134:
			shared = gCompatSharedSram;
			shared->runtime.sharedTertiaryValue = value;
			(void)shared->runtime.sharedTertiaryValue;
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 135:
			shared = gCompatSharedSram;
			shared->runtime.tertiaryHostToGuestValues[0] = value;
			(void)shared->runtime.tertiaryHostToGuestValues[0];
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		case 136:
			shared = gCompatSharedSram;
			shared->runtime.tertiaryHostToGuestValues[1] = value;
			(void)shared->runtime.tertiaryHostToGuestValues[1];
			__asm__ volatile("dsb sy" ::: "memory");
			result = 0;
			break;
		default:
			result = (int)0x80020005;
			break;
		}
	}
	ksceKernelUnlockMutex(gCompatMutex, 1);
	return result;
}

int compatGetPeripheralState(int mode)
{
	switch (mode) {
	case 1:
		return SceHpremoteForDriver_7E69EDA4();
	case 2:
		return scePowerGetBatteryLifePercent();
	case 3:
		return scePowerGetBatteryLifeTime();
	case 4:
		return scePowerIsSuspendRequired();
	default:
		return (int)0x80010016;
	}
}

int compatGetPspSystemSoftwareVersion(void)
{
	return 661;
}

int compatSuspendResume(int mode)
{
	unsigned int *requestState;
	unsigned int state;
	SceUInt32 cookie;
	SceUInt64 start;
	SceUInt timeout;
	int result;
	int intrcode;

	if (mode != 1 || gCompatState != 1)
		return 0;
	cookie = gCompatResumeCookie;
	if (!cookie) {
		result = (int)0x8001008d;
		goto cleanup;
	}
#if FW_DEBUG
	{
		uintptr_t paddr;
		unsigned int bank = gCompatState;

		result = ksceKernelVAtoPA(gCompatMainDdr, &paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(0, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		result = ksceKernelVAtoPA((char *)gCompatMainDdr + 0x1000000,
			&paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(bank, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		result = ksceKernelVAtoPA((char *)gCompatMainDdr + 0x1000000,
			&paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(2, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		result = ksceKernelVAtoPA((char *)gCompatMainDdr + 0x3000000,
			&paddr);
		ksceKernelAssert(result >= 0, &gCompatMapMainDdrTranslationInfo,
			__builtin_return_address(0));
		result = SceGrabForDriver_E9C25A28(3, paddr | 1);
		ksceKernelAssert(result == 0, &gCompatMapMainDdrBankInfo,
			__builtin_return_address(0));
		if (gCompatMemoryBlocks[2][1] > 0) {
			ksceKernelFreeMemBlock(gCompatMemoryBlocks[2][1]);
			gCompatMemoryBlocks[2][1] = 0;
		}
	}
#else
	mapMainDDR();
#endif
	if ((result = kscePowerSetCompatClockFrequency(333)) != 0 ||
		(result = SceGrabForDriver_072B8D93(gCompatMode)) != 0 ||
		(result = compatSecureInitialize(cookie)) != 0 ||
		(result = compatEnterRunningState()) != 0) {
cleanup:
		timeout = 5000000;
		gCompatResumeCookie = 0;
		ksceKernelLockMutex(gCompatMutex, 1, &timeout);
		if (gCompatState == 2) {
			int (*volatile deci2pAvailable)(void *, void *, SceSize);

			sceKernelSysrootEnableAutoClockDown();
			deci2pAvailable = SceDeci2pForDriver_F6427E0F;
			if (deci2pAvailable)
				sceDeci2pTermForCompat();
		}
		SceGrabForDriver_F13C63DD();
		gCompatPreviousState = gCompatState;
		gCompatRequestsEnabled = 0;
		gCompatState = 1;
		kscePowerSetCompatClockFrequency(0);
		ksceKernelUnlockMutex(gCompatMutex, 1);
		memset(&gCompatActivationData, 0, sizeof(gCompatActivationData));
		goto out;
	}
	gCompatResumeCookie = 0;
	ksceKernelDcacheInvalidateRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = *requestState;
	start = ksceKernelGetProcessTimeWideCore();
	while (state != 1) {
		sceKernelDelayThread(100);
		ksceKernelDcacheInvalidateRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = *requestState;
		if (ksceKernelGetProcessTimeWideCore() - start > 5000000)
			goto timedOut;
		if (!gCompatProcessId)
			goto terminated;
	}
	timeout = 5000000;
	result = ksceKernelLockMutex(gCompatMutex, 1, &timeout);
	if (result < 0)
		goto timedOut;
	result = compatSecureResume();
	if (result != 0) {
		compatUnlockMutex();
		goto out;
	}
	gCompatRequestsEnabled = 1;
	compatUnlockMutex();
	for (intrcode = 64; intrcode != 96; ++intrcode) {
		if (intrcode != 65 && intrcode != 78)
			gCompatMailbox->enableInterrupt[intrcode - 64] = ~0U;
	}
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = 2;
	__builtin_memcpy(requestState, &state, sizeof(state));
	ksceKernelDcacheCleanRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	ksceKernelDcacheInvalidateRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = *requestState;
	start = ksceKernelGetProcessTimeWideCore();
	while (state != 3) {
		sceKernelDelayThread(100);
		ksceKernelDcacheInvalidateRangeForL1WBWA(
			(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
		requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
		state = *requestState;
		if (ksceKernelGetProcessTimeWideCore() - start > 5000000)
			goto timedOut;
		if (!gCompatProcessId)
			goto terminated;
	}
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	state = 0;
	__builtin_memcpy(requestState, &state, sizeof(state));
	ksceKernelDcacheCleanRangeForL1WBWA(
		(void *)(((uintptr_t)gCompatMainDdr + 0x3d78000) & ~31U), 64);
#if FW_DEBUG
	requestState = (unsigned int *)((char *)gCompatMainDdr + 0x3d78000);
	ksceKernelAssert(*requestState == 0,
		&gCompatWaitSpecialRequestInfo,
		__builtin_return_address(0));
#endif
	result = 0;
	goto out;
timedOut:
	result = (int)0x80028005;
	goto out;
terminated:
	result = (int)0x80028007;
out:
	return result;
}

int compatGetDevInf(SceIoDevInfo *info)
{
	if (sceKernelGetProcessId() != gCompatProcessId)
		return (int)0x8002f001;
	return ksceIoDevctl(gCompatDevice, 0x3001, NULL, 0, info,
		sizeof(*info));
}

int compatGetStatus(void)
{
	return gCompatState;
}

int compatLCDCSync(void)
{
	SceDisplayResolutionInfo info;
	int head;
	int hcount;

	head = ksceDisplayGetPrimaryHead();
	if (head == 1) {
		info.size = sizeof(info);
		if (ksceDisplayGetResolutionInfoInternal(head, &info) < 0) {
#if FW_DEBUG
			info.vic = 0xffff;
#endif
			goto unsupported;
		}
		switch ((int)info.vic) {
		case 0x8100:
		case 0x8300:
			hcount = 486;
			break;
		case 0x8500:
			hcount = 1000;
			break;
		case 0x8600:
			hcount = 650;
			break;
		default:
unsupported:
#if FW_DEBUG
			ksceKernelPrintfLevelWithInfo(1, 6,
				SCE_DBGINFO_FUNC(284, "waitHCount"),
				"unsupported display mode: %x\n", info.vic);
#endif
			return 0;
		}
	} else {
		hcount = 550;
	}
	while (SceDisplayForDriver_332C5410(head) >= hcount)
		;
	while (SceDisplayForDriver_332C5410(head) < hcount)
		;
	return 0;
}

int compatSetSuspendSema(SceUID suspendSema, SceUID resumeSema)
{
	if (suspendSema) {
		suspendSema = kscePUIDtoGUID(0, suspendSema);
		if (suspendSema < 0)
			return suspendSema;
	}
	if (resumeSema) {
		resumeSema = kscePUIDtoGUID(0, resumeSema);
		if (resumeSema < 0)
			return resumeSema;
	}
	gCompatSuspendSema = suspendSema;
	gCompatResumeSema = resumeSema;
	return 0;
}

int compatGetUpdateState(void)
{
	SceRtcTick current;
	int result;

	if (gCompatRtcDeadline.tick == 0)
		goto expired;
	result = ksceRtcGetCurrentTick(&current);
	if (result != 0) {
		result = 0;
		goto out;
	}
	if (gCompatRtcDeadline.tick >= current.tick)
		goto out;
expired:
	result = 1;
out:
	return result;
}

int compatSetUpdateState(int state)
{
	SceRtcTick current;
	int result;

	result = ksceRtcGetCurrentTick(&current);
	if (result == 0)
		result = SceRtcForDriver_0CB72FAF(&gCompatRtcDeadline, &current, 1);
	return result;
}

int compatGetTitleListForVsh(char *buffer, unsigned int size)
{
	SceUID fd;
	int result;

	if (size != 100)
		return (int)0x8002000b;
	fd = ksceIoOpen(gCompatTitleListPath, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	result = ksceIoRead(fd, buffer, size);
	ksceIoClose(fd);
	if (result != 100)
		return (int)0x80010005;
	if (strnlen(buffer, 100) != 95)
		return (int)0x80010002;
	ksceIoRemove(gCompatTitleListPath);
	return 0;
}

int SceCompatForDriver_6EBD9E01(SceUInt32 mode)
{
	gCompatColorSpaceMode = mode;
	return 0;
}

int compatAvailableColorSpaceSetting(void)
{
	SceUInt32 type;
	SceUInt32 syscall;
	int head;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		head = ksceDisplayGetPrimaryHead();
		sceSyscallLeave(syscall);
		result = 0;
		if (head >= 0 &&
			ksceDisplayGetDeviceType(head, &type) == 1) {
			type &= 3;
			result = type == 3;
		}
	} else {
		sceSyscallLeave(syscall);
		result = 0;
	}
	return result;
}

int compatIsPocketStationTitle(const signed char *titleId)
{
	unsigned int prefix;
	unsigned int hash;
	const unsigned int *entry;

	prefix = ((unsigned int)titleId[1] << 16) +
		((unsigned int)titleId[0] << 24) +
		((unsigned int)titleId[2] << 8) + (unsigned int)titleId[3];
	hash = (4097U * strtol((const char *)titleId + 4, NULL, 16)) ^ prefix;
	entry = gCompatPocketStationTitleHashes;
	do {
		if (*entry++ == hash)
			return 1;
	} while (entry != gCompatPocketStationTitleHashes + 142);
	return 0;
}

int compatStartSecureModule(void)
{
	SceAuthInfo auth;
	int result;

	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.request, &gCompatSecureModuleAuth, sizeof(auth.request));
	result = ksceSblACMgrGetMediaType(gCompatSecureModulePath,
		&auth.media_type);
	if (result == 0) {
		auth.self_type = (auth.self_type & ~0xfU) | 2;
		result = ksceSblSmCommStartSmFromFile(0, gCompatSecureModulePath,
			0, &auth, &gCompatSecureModuleId);
	}
	return result;
}

int compatSecureInitialize(SceUInt32 argument)
{
	union CompatSecureCommand command;
	SceUInt32 serviceResult;
	int result;

	SceKernelSuspendForDriver_4DF40893(0);
	result = compatStartSecureModule();
	if (result == 0) {
		command.arguments[0] = argument;
		command.arguments[1] = 0;
		result = ksceSblSmCommCallFunc(gCompatSecureModuleId, 0x10006,
			&serviceResult, &command, sizeof(command));
		if (result == 0) {
			result = serviceResult;
			if (result == 0)
				result = ksceSblSmCommStopSm(gCompatSecureModuleId,
					&command.stopResult);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int compatSecureResume(void)
{
	union CompatSecureCommand command;
	SceUInt32 serviceResult;
	int result;

	SceKernelSuspendForDriver_4DF40893(0);
	result = compatStartSecureModule();
	if (result == 0) {
		command.arguments[0] = 0;
		command.arguments[1] = 0;
		result = ksceSblSmCommCallFunc(gCompatSecureModuleId, 0x30006,
			&serviceResult, &command, sizeof(command));
		if (result == 0) {
			result = serviceResult;
			if (result == 0)
				result = ksceSblSmCommStopSm(gCompatSecureModuleId,
					&command.stopResult);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return result;
}

int sceCompatInitEx(int mode)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatInitEx(mode);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatUninit(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatReleaseFrameBuffer();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatStart(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatStart();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatStop(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatStop();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatWaitAndGetRequest(int mode, int id)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatWaitAndGetRequest(mode, id);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatWaitSpecialRequest(int mode)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatWaitSpecialRequest(mode);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatInterrupt(int intrCode)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = compatInterrupt(intrCode);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatReturnValueEx(SceUInt32 channel, const SceUInt32 *values,
	SceUInt64 value)
{
	SceUInt32 localValues[2];
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsPspEmu(0) == 1) {
		result = ksceKernelCopyFromUser(localValues, values,
			sizeof(localValues));
		if (result >= 0)
			result = compatReturnValueEx(channel, localValues, value);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

