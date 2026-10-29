#include <kermit.h>

typedef struct SceUsbDriverEntry {
	u32 size;
	char name[32];
	u32 state;
	u32 unk28[2];
} SceUsbDriverEntry;

typedef struct SceUsbPspcmCallbacks {
	int (*start)(int size, void *args);
	int (*stop)(int size, void *args);
} SceUsbPspcmCallbacks;

u32 strlen(const char *s);
int strncmp(const char *s1, const char *s2, u32 n);
char *strncpy(char *dst, const char *src, u32 n);
int sceKernelCancelEventFlag(SceUID evid, u32 new_pattern, int *num_wait_threads);

int sceUsb_driver_D9EE03ED(void);
int sceUsb_driver_9009AD6B(void);

static int _sceUsbSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int _sceUsbUpdateEventFlag(void);
static int _sceUsbWaitStateInternal(u32 state, s32 waitmode, SceUInt *timeout, int is_cb);
static int _sceUsbInitKermit(void);
static int _sceUsbKermitActivate(u32 pid);
static int _sceUsbKermitDeactivate(u32 pid);
static int _sceUsbKermitCmd0(u32 cmd);
static int _sceUsbKermitCmd1(u32 cmd, u32 arg0);
static int _sceUsbKermitCmdBuf(u32 cmd, const void *buf, u32 size);
static int _sceUsbKermitPspcmStart(u32 size, const void *args);
static int _sceUsbInitStates(void);
static int _sceUsbGetActiveMode(void);
static int _sceUsbSetActivated(void);
static int _sceUsbSetDeactivated(void);
static int _sceUsbUpdateModeState(int val);
static int _sceUsbRegisterPspcm(SceUsbPspcmCallbacks *cb);
static int _sceUsbUnregisterPspcm(SceUsbPspcmCallbacks *cb);
static int _sceUsbStartSubDriver(int drvIdx);
static int _sceUsbStartPspcmDriver(int size, void *args);
static int _sceUsbStopSubDriver(int drvIdx);
static int _sceUsbStopPspcmDriver(int size, void *args);
static int _sceUsbRegisterSysEvent(void);
static int _sceUsbUnregisterSysEvent(void);

static const s8 g_modeMap[6] = { 4, 0, 0, 1, 2, 3 };

static int g_activateState = 0x100;
static int g_activeMode = 4;

static const char *g_usbDriverNames[7] = {
	"USBAccBaseDriver",
	"USBCamDriver",
	"USBCamMicDriver",
	"USBMicDriver",
	"USBGps_Driver",
	"USBPSPCommunicationDriver",
	"USBBusDriver",
};

static SceSysEventHandler g_usbSysEvent = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceUsb",
	.type_mask = 0x00FFFF00,
	.handler = _sceUsbSysEventHandler,
};

static SceUID g_eventFlagId;
static SceUID g_mutexId;
static void *g_bufBase;
static void *g_workBuf;
static u32 g_workSize;
static SceUsbPspcmCallbacks *g_pspcmArg;
static int g_drvStates[7];
static int g_modeActivated[4];
static int g_modeConnected[4];
static int g_drvRegistered[7];

int sceUsb_driver_var_A942D291;
int sceUsb_driver_var_3EA085BF;
int sceUsbSer_var_A02EBCBF;

int sceUsbActivate(u32 pid)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);

	if (sceUsb_driver_D9EE03ED() == 0) {
		pspSetK1(oldK1);
		return 0x80243007;
	}

	u32 expectedPid;
	switch (_sceUsbGetActiveMode()) {
	case 0:
		expectedPid = 0x282;
		break;
	case 1:
		expectedPid = 0x25B;
		break;
	case 2:
		expectedPid = 0x283;
		break;
	case 3:
		expectedPid = 0x1CB;
		break;
	case 4:
	default:
		pspSetK1(oldK1);
		return 0x80243005;
	}

	if (pid == expectedPid) {
		if ((sceUsb_driver_9009AD6B() & 0xF00) == 0x200) {
			pspSetK1(oldK1);
			return 0x80243001;
		}
	}

	int res = _sceUsbKermitActivate(pid);
	if (res == 0) {
		g_activateState = 0x200;
		_sceUsbSetActivated();
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsbDeactivate(u32 pid)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);

	if (sceUsb_driver_D9EE03ED() == 0) {
		pspSetK1(oldK1);
		return 0x80243007;
	}

	if ((sceUsb_driver_9009AD6B() & 0xF00) == 0x100) {
		pspSetK1(oldK1);
		return 0x80243001;
	}

	u32 deactPid;
	switch (_sceUsbGetActiveMode()) {
	case 0:
		deactPid = 0x282;
		break;
	case 1:
		deactPid = 0x25B;
		break;
	case 2:
		deactPid = 0x283;
		break;
	case 3:
		deactPid = 0x1CB;
		break;
	case 4:
	default:
		pspSetK1(oldK1);
		return 0x80243007;
	}

	int res = _sceUsbKermitDeactivate(deactPid);
	if (res == 0) {
		g_activateState = 0x100;
		_sceUsbSetDeactivated();
	}

	pspSetK1(oldK1);
	return res;
}

static int _sceUsbUpdateEventFlag(void)
{
	sceKernelClearEventFlag(g_eventFlagId, 0);
	int state = sceUsb_driver_9009AD6B();
	return sceKernelSetEventFlag(g_eventFlagId, (u32)state);
}

int sceUsbWaitState(u32 state, s32 waitmode, SceUInt *timeout)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);

	int res;
	if (sceUsb_driver_D9EE03ED() == 0) {
		res = 0x80243007;
	} else if (sceKernelIsIntrContext() != 0) {
		res = 0x80020064;
	} else if (((s32)((((u32)timeout + 4u) | (u32)timeout) & shiftedK1)) < 0) {
		res = 0x800200D3;
	} else {
		res = _sceUsbWaitStateInternal(state, waitmode, timeout, 0);
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsb_616F2B61(u32 state, s32 waitmode, SceUInt *timeout)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);

	int res;
	if (sceUsb_driver_D9EE03ED() == 0) {
		res = 0x80243007;
	} else if (sceKernelIsIntrContext() != 0) {
		res = 0x80020064;
	} else if (((s32)((((u32)timeout + 4u) | (u32)timeout) & shiftedK1)) < 0) {
		res = 0x800200D3;
	} else {
		res = _sceUsbWaitStateInternal(state, waitmode, timeout, 1);
	}

	pspSetK1(oldK1);
	return res;
}

int module_start(SceSize args, void *argp)
{
	if (_sceUsbInitKermit() < 0) {
		return 1;
	}
	SceUID ev = sceKernelCreateEventFlag("SceUsb", 0x201, 0, NULL);
	if (ev < 0) {
		return 1;
	}
	g_eventFlagId = ev;
	return (_sceUsbInitStates() < 0) ? 1 : 0;
}

int sceUsbGetState(void)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);

	int res = 0x80243007;
	if (sceUsb_driver_D9EE03ED() != 0) {
		res = sceUsb_driver_9009AD6B();
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsb_E20B23A6(void)
{
	return 0;
}

int sceUsbWaitCancel(void)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);

	int res = 0x80243007;
	if (sceUsb_driver_D9EE03ED() != 0) {
		int state = sceUsb_driver_9009AD6B();
		res = sceKernelCancelEventFlag(g_eventFlagId, (u32)state, NULL);
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsb_8BFC3DE8(void)
{
	return 0;
}

int sceUsb_89DE0DC5(void)
{
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	int res = sceKernelDeleteEventFlag(g_eventFlagId);
	return (res < 0) ? res : 0;
}

int module_reboot_phase(SceSize args, void *argp)
{
	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	return 0;
}

static int _sceUsbWaitStateInternal(u32 state, s32 waitmode, SceUInt *timeout, int is_cb)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);

	_sceUsbUpdateEventFlag();

	int res;
	if (is_cb != 0) {
		res = sceKernelWaitEventFlagCB(g_eventFlagId, state, (u32)waitmode, NULL, timeout);
	} else {
		res = sceKernelWaitEventFlag(g_eventFlagId, state, (u32)waitmode, NULL, timeout);
	}

	if (res >= 0) {
		res = sceUsb_driver_9009AD6B();
	} else if (res == (int)0x800201A8) {
		res = 0x80243008;
	} else if (res == (int)0x800201A9) {
		res = 0x80243009;
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsb_driver_32627D8F(void *buf, u32 size)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	void *work = g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	sceKermitMemory_driver_AAF047AC(req, 0, 0, work, size, 2);
	req->args[1] = size;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, 0x15F, 2, 0, &resp);
	res = sceKernelUnlockMutex(g_mutexId, 1);
	if (res != 0) {
		return res;
	}
	if (kres != 0) {
		return kres;
	}

	int cmdRes = (int)resp;
	sceKernelMemcpy(buf, work, size);
	return cmdRes;
}

int sceUsb_driver_AB8ADCC8(void *buf1, void *buf2)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);
	u32 half = (g_workSize >> 1) & ~0x3Fu;

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u8 *work1 = (u8 *)g_workBuf;
	u8 *work2 = work1 + half;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	req->cmd = 0xC6F;
	sceKermitMemory_driver_AAF047AC(req, 0, 0, work1, 0x30, 2);
	sceKermitMemory_driver_AAF047AC(req, 1, 1, work2, 0x104, 2);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, 0x159, 2, 0, &resp);
	if (kres != 0) {
		return kres;
	}

	int cmdRes = (int)resp;
	if (cmdRes == 0) {
		if (buf1 != NULL) {
			sceKernelMemcpy(buf1, work1, 0x30);
		}
		if (buf2 != NULL) {
			sceKernelMemcpy(buf2, work2, 0x104);
		}
	}

	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _sceUsbKermitPspcmStart(u32 size, const void *args)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	void *work = g_workBuf;

	if ((size - 1u) >= 0x40u) {
		return 0x80243002;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	sceKernelMemcpy(work, args, size);
	req->args[0] = size;
	sceKermitMemory_driver_AAF047AC(req, 1, 1, work, size, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, 0x164, 2, 0, &resp);
	res = sceKernelUnlockMutex(g_mutexId, 1);
	if (res != 0) {
		return res;
	}
	return (kres == 0) ? (int)resp : kres;
}

int sceUsb_driver_9F89B84C(u32 *out)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, 0x17B, 0, 0, &resp);
	if (kres == 0) {
		u32 lo = (u32)resp;
		u32 hi = (u32)(resp >> 32);
		if (lo != 0xFFFFFFFFu || hi != 0xFFFFFFFFu) {
			out[0] = lo;
			out[1] = hi;
		}
	}

	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? kres : res;
}

static int _sceUsbInitKermit(void)
{
	sceKernelMemset((void *)0xABD60000u, 0, 0x8000);
	g_bufBase = (void *)0xABD60000u;
	g_workBuf = (void *)0xABD64000u;
	g_workSize = 0x4000;

	SceUID mtx = sceKernelCreateMutex("SceKermitUsb", SCE_KERNEL_SA_THPRI, 0, NULL);
	g_mutexId = mtx;
	return (mtx > 0) ? 0 : (int)0x80020001;
}

static int _sceUsbKermitActivate(u32 pid)
{
	return _sceUsbKermitCmd1(0x176, pid);
}

static int _sceUsbKermitDeactivate(u32 pid)
{
	return _sceUsbKermitCmd1(0x15D, pid);
}

int sceUsb_driver_BDA21CF7(const void *buf)
{
	return _sceUsbKermitCmdBuf(0x16E, buf, 0x34);
}

int sceUsb_driver_63E55EBE(void)
{
	return _sceUsbKermitCmd0(0x16D);
}

int sceUsb_driver_ED8C8695(void)
{
	return _sceUsbKermitCmd0(0x165);
}

int sceUsb_driver_18DB54DC(void)
{
	return _sceUsbKermitCmd0(0x177);
}

int sceUsb_driver_A119F065(void)
{
	return _sceUsbKermitCmd0(0x168);
}

int sceUsb_driver_8C079564(const void *buf)
{
	return _sceUsbKermitCmdBuf(0x171, buf, 0x18);
}

int sceUsb_driver_D1F264A2(void)
{
	return _sceUsbKermitCmd0(0x15B);
}

int sceUsb_driver_3B578B06(void)
{
	return _sceUsbKermitCmd0(0x173);
}

int sceUsb_driver_BFCDFDD1(void)
{
	return _sceUsbKermitCmd0(0x15C);
}

int sceUsb_driver_B3E9ABCE(void)
{
	return _sceUsbKermitCmd0(0x160);
}

int sceUsb_driver_8839766F(u32 cmd, u32 arg0)
{
	return _sceUsbKermitCmd1(cmd, arg0);
}

int sceUsb_driver_292740A2(u32 cmd)
{
	return _sceUsbKermitCmd0(cmd);
}

int sceUsb_driver_F183B4EF(u32 arg0)
{
	return _sceUsbKermitCmd1(0x17D, arg0);
}

int sceUsb_driver_A229F2CF(void)
{
	return _sceUsbKermitCmd0(0x17E);
}

int sceUsb_driver_E09431A0(void)
{
	return _sceUsbKermitCmd0(0x17F);
}

int sceUsb_driver_CCA3CDF7(const void *buf)
{
	return _sceUsbKermitCmdBuf(0x174, buf, 0x14);
}

int sceUsb_driver_1159202A(void)
{
	return _sceUsbKermitCmd0(0x17A);
}

int sceUsb_driver_469C73EB(void)
{
	return _sceUsbKermitCmd0(0x16F);
}

int sceUsb_driver_96A32FBC(void)
{
	return _sceUsbKermitCmd0(0x16A);
}

int sceUsb_driver_D27EDB31(u32 arg0)
{
	return _sceUsbKermitCmd1(0x16B, arg0);
}

int sceUsb_driver_8A2D38D7(void)
{
	return _sceUsbKermitCmd0(0x175);
}

int sceUsb_driver_79545577(void)
{
	return _sceUsbKermitCmd0(0x163);
}

int sceUsb_driver_CB30E794(void)
{
	return _sceUsbKermitCmd0(0x180);
}

static int _sceUsbKermitCmd0(u32 cmd)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, cmd, 0, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _sceUsbKermitCmd1(u32 cmd, u32 arg0)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	req->args[0] = arg0;
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, cmd, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _sceUsbKermitCmdBuf(u32 cmd, const void *buf, u32 size)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	void *work = g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	sceKernelMemcpy(work, buf, size);
	sceKermitMemory_driver_AAF047AC(req, 0, 0, work, size, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USB, cmd, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

int sceUsb_driver_C99E1766(int drvIdx, SceUsbPspcmCallbacks *cb)
{
	if (drvIdx < 0) {
		return 0x80243002;
	}
	if (drvIdx < 5) {
		if (g_drvRegistered[drvIdx] == 1) {
			return 0x80243001;
		}
		g_drvRegistered[drvIdx] = 1;
		g_activeMode = g_modeMap[drvIdx];
		return 0;
	}
	if (drvIdx == 5) {
		if (g_drvRegistered[5] == 1) {
			return 0x80243001;
		}
		int res = _sceUsbRegisterPspcm(cb);
		if (res < 0) {
			return res;
		}
		g_drvRegistered[5] = 1;
		g_activeMode = g_modeMap[5];
		return 0;
	}
	return 0x80243002;
}

int sceUsb_driver_6C68EE25(int drvIdx, SceUsbPspcmCallbacks *cb)
{
	if (drvIdx < 0) {
		return 0x80243002;
	}
	if (drvIdx < 5) {
		if (g_drvRegistered[drvIdx] == 0) {
			return 0x80243005;
		}
		g_drvRegistered[drvIdx] = 0;
		g_activeMode = 4;
		return 0;
	}
	if (drvIdx == 5) {
		if (g_drvRegistered[5] == 0) {
			return 0x80243005;
		}
		int res = _sceUsbUnregisterPspcm(cb);
		if (res < 0) {
			return res;
		}
		g_drvRegistered[5] = 0;
		g_activeMode = 4;
		return 0;
	}
	return 0x80243002;
}

int sceUsbStart(const char *driverName, int size, void *args)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);

	if (driverName == NULL) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	if (sceKernelIsIntrContext() != 0) {
		pspSetK1(oldK1);
		return 0x80000030;
	}
	if (((s32)(shiftedK1 & (u32)driverName)) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	u32 len = strlen(driverName);
	if (((s32)(shiftedK1 & ((u32)driverName + len))) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	if (((s32)(shiftedK1 & (((u32)args + (u32)size) | (u32)args | (u32)size))) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}

	int drvIdx = -1;
	for (int i = 0; i < 7; i++) {
		if (strncmp(g_usbDriverNames[i], driverName, strlen(g_usbDriverNames[i])) == 0) {
			drvIdx = i;
			break;
		}
	}

	int res;
	switch (drvIdx) {
	case 0:
		if (g_drvStates[0] != 2) {
			res = 0x80243001;
		} else if (g_drvStates[6] == 2 || g_drvRegistered[0] == 0) {
			res = 0x80243007;
		} else {
			g_drvStates[0] = 1;
			res = 0;
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		res = _sceUsbStartSubDriver(drvIdx);
		break;
	case 5:
		res = _sceUsbStartPspcmDriver(size, args);
		break;
	case 6:
		if (g_drvStates[6] != 2) {
			res = 0x80243001;
		} else {
			g_drvStates[6] = 1;
			res = (_sceUsbRegisterSysEvent() >= 0) ? 0 : 1;
		}
		break;
	default:
		res = 0x80243002;
		break;
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsbStop(const char *driverName, int size, void *args)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);

	if (driverName == NULL) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	if (sceKernelIsIntrContext() != 0) {
		pspSetK1(oldK1);
		return 0x80000030;
	}
	if (((s32)(shiftedK1 & (u32)driverName)) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	u32 len = strlen(driverName);
	if (((s32)(shiftedK1 & ((u32)driverName + len))) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	if (((s32)(shiftedK1 & (((u32)args + (u32)size) | (u32)args | (u32)size))) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}

	int drvIdx = -1;
	for (int i = 0; i < 7; i++) {
		if (strncmp(g_usbDriverNames[i], driverName, strlen(g_usbDriverNames[i])) == 0) {
			drvIdx = i;
			break;
		}
	}

	int res;
	switch (drvIdx) {
	case 0:
		if (g_drvStates[0] != 1) {
			res = 0x80243001;
		} else {
			res = 0;
			for (int i = 1; i < 6; i++) {
				if (g_drvStates[i] == 1) {
					res = 0x80243006;
					break;
				}
			}
			if (res == 0) {
				g_drvStates[0] = 2;
			}
		}
		break;
	case 1:
	case 2:
	case 3:
	case 4:
		res = _sceUsbStopSubDriver(drvIdx);
		break;
	case 5:
		res = _sceUsbStopPspcmDriver(size, args);
		break;
	case 6:
		if (g_drvStates[6] != 1) {
			res = 0x80243001;
		} else if (g_drvStates[0] == 1) {
			res = 0x80243006;
		} else {
			g_drvStates[6] = 2;
			res = (_sceUsbUnregisterSysEvent() >= 0) ? 0 : 1;
		}
		break;
	default:
		res = 0x80243002;
		break;
	}

	pspSetK1(oldK1);
	return res;
}

int sceUsbGetDrvList(u32 r4flags, SceUsbDriverEntry *list, int size)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);

	if (list == NULL && (u32)size > 0u) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	if ((r4flags & 3u) == 0) {
		pspSetK1(oldK1);
		return 0x80000105;
	}
	if (sceKernelIsIntrContext() != 0) {
		pspSetK1(oldK1);
		return 0x80000030;
	}

	u32 byteSize = (u32)size * sizeof(SceUsbDriverEntry);
	if (((s32)(shiftedK1 & (((u32)list + byteSize) | (u32)list | byteSize))) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}

	for (int i = 0; i < size; i++) {
		if (list[i].size != sizeof(SceUsbDriverEntry)) {
			pspSetK1(oldK1);
			return 0x800001FE;
		}
	}

	int count = 0;
	for (int i = 0; i < 7; i++) {
		if (g_drvRegistered[i] != 0) {
			if ((r4flags & (u32)g_drvStates[i]) != 0) {
				u32 nameLen = strlen(g_usbDriverNames[i]) + 1u;
				strncpy(list[count].name, g_usbDriverNames[i], nameLen);
				list[count].state = (u32)g_drvStates[i];
				count++;
				if (count >= size) {
					break;
				}
			}
		}
	}

	pspSetK1(oldK1);
	return count;
}

int sceUsbGetDrvState(const char *driverName)
{
	u32 oldK1 = pspGetK1();
	u32 shiftedK1 = oldK1 << 11;
	pspSetK1(shiftedK1);

	if (driverName == NULL) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	if (sceKernelIsIntrContext() != 0) {
		pspSetK1(oldK1);
		return 0x80000030;
	}
	if (((s32)(shiftedK1 & (u32)driverName)) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}
	u32 len = strlen(driverName);
	if (((s32)(shiftedK1 & ((u32)driverName + len))) < 0) {
		pspSetK1(oldK1);
		return 0x80000103;
	}

	int drvIdx = -1;
	for (int i = 0; i < 7; i++) {
		if (strncmp(g_usbDriverNames[i], driverName, strlen(g_usbDriverNames[i])) == 0) {
			drvIdx = i;
			break;
		}
	}

	int res = ((u32)drvIdx < 7u) ? g_drvStates[drvIdx] : (int)0x80243002;
	pspSetK1(oldK1);
	return res;
}

static int _sceUsbInitStates(void)
{
	for (int i = 0; i < 7; i++) {
		g_drvStates[i] = 2;
	}
	g_drvRegistered[6] = 1;
	return 0;
}

int sceUsb_driver_D9EE03ED(void)
{
	return (g_drvStates[6] == 1) ? 1 : 0;
}

static int _sceUsbGetActiveMode(void)
{
	return g_activeMode;
}

int sceUsb_driver_9009AD6B(void)
{
	int mode = g_activeMode;
	if (mode == 3) {
		sceKernelDelayThread(1000);
		mode = g_activeMode;
	} else if (mode >= 4) {
		return (mode == 4) ? 0x121 : 0;
	} else if (mode < 0) {
		return 0;
	}

	int base = (g_modeActivated[mode] == 1) ? 0x220 : 0x120;
	return (g_modeConnected[mode] == 1) ? (base | 2) : (base | 1);
}

static int _sceUsbSetActivated(void)
{
	return _sceUsbUpdateModeState(1);
}

static int _sceUsbSetDeactivated(void)
{
	return _sceUsbUpdateModeState(0);
}

int sceUsb_driver_7C5791D3(void)
{
	int mode = g_activeMode;
	if (mode >= 0 && mode < 4) {
		g_modeConnected[mode] = 1;
	}
	_sceUsbUpdateEventFlag();
	return 0;
}

int sceUsb_driver_55494514(void)
{
	int mode = g_activeMode;
	if (mode >= 0 && mode < 4) {
		g_modeConnected[mode] = 0;
	}
	_sceUsbUpdateEventFlag();
	return 0;
}

static int _sceUsbUpdateModeState(int val)
{
	switch (g_activeMode) {
	case 0:
	case 3:
		if (g_activeMode >= 0 && g_activeMode < 4) {
			g_modeConnected[g_activeMode] = val;
		}
		_sceUsbUpdateEventFlag();
		/* fallthrough */
	case 1:
	case 2:
		g_modeActivated[g_activeMode] = val;
		_sceUsbUpdateEventFlag();
		break;
	case 4:
	default:
		_sceUsbUpdateEventFlag();
		break;
	}
	return 0;
}

static int _sceUsbRegisterPspcm(SceUsbPspcmCallbacks *cb)
{
	if (cb == NULL || g_pspcmArg != NULL) {
		return 0x80243002;
	}
	g_pspcmArg = cb;
	return 0;
}

static int _sceUsbUnregisterPspcm(SceUsbPspcmCallbacks *cb)
{
	if (cb == NULL) {
		return 0x80243002;
	}
	if (g_pspcmArg == NULL) {
		return 0x80243005;
	}
	if (g_pspcmArg != cb) {
		return 0x80243002;
	}
	g_pspcmArg = NULL;
	return 0;
}

static int _sceUsbStartSubDriver(int drvIdx)
{
	if (g_drvStates[6] == 2 || g_drvStates[0] == 2 || g_drvRegistered[drvIdx] == 0) {
		return 0x80243007;
	}
	if (g_drvStates[drvIdx] == 1) {
		return 0x80243001;
	}
	g_drvStates[drvIdx] = 1;
	return 0;
}

static int _sceUsbStartPspcmDriver(int size, void *args)
{
	if (g_drvStates[6] == 2 || g_drvRegistered[5] == 0) {
		return 0x80243007;
	}
	if (g_drvStates[5] == 1) {
		return 0x80243001;
	}
	if (g_pspcmArg == NULL) {
		return 0x80243007;
	}

	int res = _sceUsbKermitPspcmStart((u32)size, args);
	if (res < 0) {
		return res;
	}

	g_drvStates[5] = 1;
	g_pspcmArg->start(size, args);
	return res;
}

static int _sceUsbStopSubDriver(int drvIdx)
{
	if (g_drvStates[drvIdx] == 2) {
		return 0x80243001;
	}
	int res = (g_drvRegistered[drvIdx] != 0) ? 0 : (int)0x80243007;
	g_drvStates[drvIdx] = 2;
	return res;
}

static int _sceUsbStopPspcmDriver(int size, void *args)
{
	if (g_drvStates[5] == 2) {
		return 0x80243001;
	}
	int res = (g_drvRegistered[5] != 0) ? 0 : (int)0x80243007;
	g_drvStates[5] = 2;
	g_pspcmArg->stop(size, args);
	return res;
}

static int _sceUsbRegisterSysEvent(void)
{
	return sceKernelRegisterSysEventHandler(&g_usbSysEvent);
}

static int _sceUsbUnregisterSysEvent(void)
{
	return sceKernelUnregisterSysEventHandler(&g_usbSysEvent);
}

static int _sceUsbSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	return 0;
}
