#include <kermit.h>

static int peripheralSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int peripheralSendSimpleCmd(u32 cmd);
static int peripheralGet8Bytes(void *out, u32 cmd);

static SceSysEventHandler g_sysEventHandler = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceKermitPeripheral",
	.type_mask = 0x00FFFF00,
	.handler = peripheralSysEventHandler,
};

static int g_unk_dd0;
static int g_unk_dd4;
static int g_unk_dd8;
static u32 g_watchdogCounter;
static SceUID g_mutexId;
static void *g_bufBase;
static void *g_workBuf;
static u32 g_workSize;

int sceKermitPeripheral_driver_C6F8B4E1(u32 key, u32 offset, void *buf, u32 len)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);
	u16 leaf = key & 0xFFFFu;

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	void *work = g_workBuf;

	if (leaf > 0xFFEFu) {
		return 0x80000100;
	}
	if (offset >= 0x200u || len >= 0x200u || (offset + len) > 0x200u) {
		return 0x800001FE;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	req->cmd = 0x41D;
	req->args[0] = leaf;
	req->args[1] = offset;
	sceKermitMemory_driver_AAF047AC(req, 2, 2, work, len, 2);
	req->args[3] = len;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x41D, 4, 0, &resp);
	if (kres == 0) {
		kres = (int)KERMIT_RESP_BOX[0].result;
		sceKernelMemcpy(buf, work, len);
	}
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? kres : res;
}

int sceKermitPeripheral_driver_76BE2E28(int a0, int a1, int a2)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	if (a0 >= 0) {
		g_unk_dd4 = a0;
	} else {
		a0 = g_unk_dd4;
	}
	if (a1 >= 0) {
		g_unk_dd0 = a1;
	} else {
		a1 = g_unk_dd0;
	}
	if (a2 >= 0) {
		g_unk_dd8 = a2;
	} else {
		a2 = g_unk_dd8;
	}

	req->cmd = 0x416;
	req->args[0] = (u32)a0;
	req->args[1] = (u32)a1;
	req->args[2] = (u32)a2;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x416, 3, 0, &resp);
	if (kres == 0) {
		kres = (int)resp;
	}
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? kres : res;
}

int sceKermitPeripheral_D27C5E03(u32 a0)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);
	u32 old_k1 = pspGetK1();

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	pspSetK1(old_k1 << 11);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		pspSetK1(old_k1);
		return res;
	}

	req->args[0] = a0;
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x418, 1, 0, &resp);
	res = sceKernelUnlockMutex(g_mutexId, 1);
	pspSetK1(old_k1);
	if (res != 0) {
		return res;
	}
	return (kres != 0) ? kres : (int)resp;
}

int sceKermitPeripheral_driver_8C7903E7(u32 a0, u32 a1)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	req->cmd = 0x415;
	req->args[0] = a0;
	req->args[1] = a1;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x415, 2, 0, &resp);
	res = sceKernelUnlockMutex(g_mutexId, 1);
	if (res != 0) {
		return res;
	}
	return (kres != 0) ? kres : (int)resp;
}

int sceKermitPeripheral_driver_61BEA1D2(u32 a0)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	req->args[0] = a0;
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x419, 1, 0, &resp);
	res = sceKernelUnlockMutex(g_mutexId, 1);
	if (res != 0) {
		return res;
	}
	return (kres != 0) ? kres : (int)resp;
}

int module_start(SceSize args, void *argp)
{
	sceKernelMemset((void *)0xABD48000u, 0, 0x8000);
	g_bufBase = (void *)0xABD48000u;
	g_workBuf = (void *)0xABD4C000u;
	g_workSize = 0x4000;
	g_mutexId = sceKernelCreateMutex("SceKermitPeripheral", SCE_KERNEL_SA_THPRI, 0, NULL);
	if (g_mutexId > 0) {
		sceKernelRegisterSysEventHandler(&g_sysEventHandler);
		LoadExecForKernel_CAFC9CB3(peripheralSendSimpleCmd);
	}
	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int sceKermitPeripheral_driver_15F5C107(void)
{
	return peripheralSendSimpleCmd(0x41B);
}

int sceKermitPeripheral_driver_AB29C68B(void)
{
	sceKernelMemset((void *)0xABD48000u, 0, 0x8000);
	g_bufBase = (void *)0xABD48000u;
	g_workBuf = (void *)0xABD4C000u;
	g_workSize = 0x4000;
	g_mutexId = sceKernelCreateMutex("SceKermitPeripheral", SCE_KERNEL_SA_THPRI, 0, NULL);
	if (g_mutexId <= 0) {
		return 0x80020001;
	}
	sceKernelRegisterSysEventHandler(&g_sysEventHandler);
	LoadExecForKernel_CAFC9CB3(peripheralSendSimpleCmd);
	return 0;
}

int sceKermitPeripheral_driver_B778DB22(void)
{
	sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int sceKermitPeripheral_driver_1617E966(void *out)
{
	return peripheralGet8Bytes(out, 0x426);
}

int sceKermitPeripheral_driver_55D690DD(void *out)
{
	return peripheralGet8Bytes(out, 0x41F);
}

int sceKermitPeripheral_driver_9EF7501D(void)
{
	u32 val = g_watchdogCounter + 1;
	g_watchdogCounter = val;
	HW(0xBFC00188u) = val;
	return 0;
}

int sceKermitPeripheral_driver_0648E1A3(void)
{
	return peripheralSendSimpleCmd(0x423);
}

int sceKermitPeripheral_driver_6D6AA215(void)
{
	return peripheralSendSimpleCmd(0x413);
}

int sceKermitPeripheral_driver_65D98A21(void)
{
	return peripheralSendSimpleCmd(0x421);
}

int sceKermitPeripheral_driver_D0A69002(void)
{
	return peripheralSendSimpleCmd(0x414);
}

int sceKermitPeripheral_driver_675DB2C1(void)
{
	return peripheralSendSimpleCmd(0x41E);
}

s64 sceKermitPeripheral_driver_20638B97(void)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u64 resp = 0;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x41C, 0, 0, &resp);
	if (res != 0) {
		return (s64)res;
	}
	return (s64)resp;
}

int sceKermitPeripheral_driver_4A26B7C8(void)
{
	u32 old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	int res = peripheralSendSimpleCmd(0x411);
	pspSetK1(old_k1);
	return res;
}

int sceKermitPeripheral_C0EBC631(void)
{
	u32 old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	sceKermitMemory_driver_80E1240A((void *)0x00013F80u, 0x80);
	int res = peripheralSendSimpleCmd(0x420);
	pspSetK1(old_k1);
	return res;
}

int sceKermitPeripheral_driver_B93A88E2(void)
{
	return peripheralSendSimpleCmd(0x417);
}

static int peripheralSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	return 0;
}

static int peripheralSendSimpleCmd(u32 cmd)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u64 resp = 0;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, cmd, 0, 0, &resp);
	return (res != 0) ? res : (int)resp;
}

static int peripheralGet8Bytes(void *out, u32 cmd)
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

	sceKermitMemory_driver_AAF047AC(req, 0, 0, work, 8, 2);
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, cmd, 1, 0, &resp);
	if (kres == 0) {
		kres = (int)resp;
		sceKernelMemcpy(out, work, 8);
	}
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? kres : res;
}
