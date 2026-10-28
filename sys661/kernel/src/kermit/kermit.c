#include <kermit.h>

static int kermitSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static void kermitDmaSync(void);
static int kermitShutdown(void);
static int kermitInit(void);
static void kermitDispatchVirtualIntr(u32 mask);
static void *kermitTranslateAndFlushRequest(void *addr, u32 size);

static SceSysEventHandler g_sysEventHandler = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceKermit",
	.type_mask = 0x00FFFF00,
	.handler = kermitSysEventHandler,
};

static SceUID g_respSema[16];
static SceUID g_pipeSema[3];
static SceUID g_msgPipeId;
static int (*g_intrHandlers[14])(void);
static int g_activeReqs;
static int g_isEnabled;

static inline u32 ctz32(u32 x)
{
	u32 rev, lz;
	__asm__("bitrev %0, %1" : "=r"(rev) : "r"(x));
	__asm__("clz %0, %1" : "=r"(lz) : "r"(rev));
	return lz;
}

static int kermitIntrHandler(void)
{
	volatile u32 dummy;
	int intr = sceKernelCpuSuspendIntr();
	u32 pend = HW(KERMIT_REG_INTR_PEND) & ~0x4002u;

	HW(KERMIT_REG_INTR_PEND) = pend;
	__asm__ volatile("sync");

	if ((pend >> 16) != 0) {
		kermitDispatchVirtualIntr(pend >> 16);
	}
	pend &= 0xFFFFu;

	if (pend != 0) {
		u32 bit = 0;
		do {
			u32 tz = ctz32(pend);
			bit += tz;
			pend >>= tz;
			if (bit >= 4) {
				if (bit < 7) {
					sceKernelSignalSema(g_pipeSema[bit - 4], 1);
					pend >>= 1;
					bit++;
					continue;
				}
				if (bit < 10) {
					volatile SceKermitResponse *resp = &KERMIT_RESP_BOX[bit - 7];
					u64 *dst = resp->resp_ptr;
					SceUID sema = resp->sema_id;
					((u32 *)dst)[0] = (u32)resp->result;
					((u32 *)dst)[1] = (u32)(resp->result >> 32);
					if (sceKernelSignalSema(sema, 1) == 0) {
						u32 mask = 1u << bit;
						u32 trig = HW(KERMIT_REG_TRIG);
						HW(KERMIT_REG_TRIG) = trig & ~mask;
						dummy = HW(KERMIT_REG_TRIG);
						HW(KERMIT_REG_TRIG) = trig | mask;
						dummy = HW(KERMIT_REG_TRIG);
						(void)dummy;
					}
					pend >>= 1;
					bit++;
					continue;
				}
			}
			pend >>= 1;
			bit++;
		} while (pend != 0);
	}

	sceKernelCpuResumeIntr(intr);
	return -1;
}

int sceKermit_driver_4C3A362D(u32 channel, u32 is_cb)
{
	volatile u32 dummy;
	int res = sceKernelPowerLockForUser(1);
	if (res != 0) {
		return res;
	}

	kermitDmaSync();
	if (channel >= 16) {
		sceKernelPowerUnlockForUser(1);
		return 0x80010016;
	}

	u32 mask = 1u << channel;
	int intr = sceKernelCpuSuspendIntr();
	HW(KERMIT_REG_INTR_MASK) |= mask;
	u32 trig = HW(KERMIT_REG_TRIG);
	HW(KERMIT_REG_TRIG) = trig & ~mask;
	dummy = HW(KERMIT_REG_TRIG);
	HW(KERMIT_REG_TRIG) = trig | mask;
	dummy = HW(KERMIT_REG_TRIG);
	(void)dummy;
	sceKernelCpuResumeIntr(intr);

	if (is_cb != 0) {
		res = sceKernelWaitSemaCB(g_respSema[channel], 1, NULL);
	} else {
		res = sceKernelWaitSema(g_respSema[channel], 1, NULL);
	}

	if (res != 0) {
		sceKernelPowerUnlockForUser(1);
		return res;
	}

	res = sceKernelPowerUnlockForUser(1);
	return (res < 0) ? res : 0;
}

int sceKermit_driver_D69C50BB(void)
{
	volatile u32 dummy;
	int intr = sceKernelCpuSuspendIntr();

	HW(0xBE140000u) &= ~1u;
	u32 start = sceKernelGetSystemTimeLow();
	while ((sceKernelGetSystemTimeLow() - start) < 0x40u) {
	}

	u32 trig = HW(KERMIT_REG_TRIG);
	HW(KERMIT_REG_TRIG) = trig & ~0x400u;
	dummy = HW(KERMIT_REG_TRIG);
	HW(KERMIT_REG_TRIG) = trig | 0x400u;
	dummy = HW(KERMIT_REG_TRIG);
	(void)dummy;

	while ((HW(KERMIT_REG_INTR_PEND) & 0x400u) == 0) {
	}
	HW(KERMIT_REG_INTR_PEND) = 0x400u;
	__asm__ volatile("sync");

	HW(0xBE140000u) |= 3u;
	sceKernelCpuResumeIntr(intr);
	return intr;
}

int sceKermit_driver_36666181(SceKermitRequest *req, u32 mode, u32 cmd, u32 num_args, u32 is_cb, u64 *resp)
{
	volatile u32 dummy;
	if (g_isEnabled == 0) {
		sceKernelDelayThread(10000);
		*resp = 0;
		return 0;
	}

	int intr = sceKernelCpuSuspendIntr();
	g_activeReqs++;
	sceKernelCpuResumeIntr(intr);

	u32 pipe;
	if (mode == KERMIT_MODE_AUDIOOUT) {
		pipe = 1;
	} else if (mode == KERMIT_MODE_ME) {
		pipe = 2;
	} else {
		pipe = 0;
	}

	SceUInt timeout = 5000000;
	int res = sceKernelWaitSema(g_pipeSema[pipe], 1, &timeout);
	if (res == 0) {
		SceUID sema_id = 0;
		res = sceKernelReceiveMsgPipe(g_msgPipeId, &sema_id, sizeof(sema_id), 0, NULL, NULL);
		if (res == 0) {
			volatile SceKermitCommand *box = &KERMIT_CMD_BOX[pipe];
			box->cmd = (mode << 16) | cmd;
			u32 req_size = (((num_args + 9) & ~7u) << 3);
			req->cmd = cmd;
			req->sema_id = sema_id;
			req->self = req;
			box->req = kermitTranslateAndFlushRequest(req, req_size);
			kermitDmaSync();

			res = sceKernelPowerLockForUser(1);
			if (res == 0) {
				u32 mask = 1u << (pipe + 4);
				intr = sceKernelCpuSuspendIntr();
				u32 trig = HW(KERMIT_REG_TRIG);
				HW(KERMIT_REG_TRIG) = trig & ~mask;
				dummy = HW(KERMIT_REG_TRIG);
				HW(KERMIT_REG_TRIG) = trig | mask;
				dummy = HW(KERMIT_REG_TRIG);
				(void)dummy;
				sceKernelCpuResumeIntr(intr);

				if (is_cb != 0) {
					sceKernelWaitSemaCB(sema_id, 1, NULL);
				} else {
					sceKernelWaitSema(sema_id, 1, NULL);
				}

				res = sceKernelSendMsgPipe(g_msgPipeId, &sema_id, sizeof(sema_id), 0, NULL, NULL);
				if (res == 0) {
					((u32 *)resp)[0] = ((u32 *)req)[0];
					((u32 *)resp)[1] = ((u32 *)req)[1];
					res = sceKernelPowerUnlockForUser(1);
				} else {
					sceKernelPowerUnlockForUser(1);
				}
			}
		}
	}

	intr = sceKernelCpuSuspendIntr();
	g_activeReqs--;
	sceKernelCpuResumeIntr(intr);
	return (res < 0) ? res : 0;
}

int module_start(SceSize args, void *argp)
{
	sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
	int res = kermitInit();
	if (res >= 0) {
		g_activeReqs = 0;
		g_isEnabled = 1;
		res = 0;
	}
	if (res < 0) {
		return 1;
	}
	sceKernelRegisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	kermitShutdown();
	return 0;
}

int sceKermit_driver_9DDD7DCA(void)
{
	g_isEnabled = 0;
	while (g_activeReqs > 0) {
		sceKernelDelayThread(100);
	}
	return 0;
}

int sceKermit_driver_5280B410(u32 intr_code, int (*handler)(void))
{
	if (intr_code < 14) {
		g_intrHandlers[intr_code] = handler;
	}
	return 0;
}

static void kermitDmaSync(void)
{
	volatile u32 status;
	__asm__ volatile("sync");
	HW(KERMIT_DMA_SYNC) = 0xFu;
	do {
		status = HW(KERMIT_DMA_SYNC);
	} while (status != 0);
	do {
		status = HW(KERMIT_DMA_STATUS);
	} while (status != 0xFu);
}

static int kermitSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	if (ev_id == 0x4000) {
		HW(KERMIT_REG_INTR_MASK) &= 0x4002u;
	} else if (ev_id == 0x10000) {
		HW(KERMIT_REG_CFG_40) = 0xFFFFFFFFu;
		HW(KERMIT_REG_CFG_44) = 0;
		HW(KERMIT_REG_CFG_48) = 0xFFFFFFFFu;
		HW(KERMIT_REG_CFG_4C) = 0;
		HW(KERMIT_REG_TRIG) = 0;
		HW(KERMIT_REG_INTR_MASK) = (HW(KERMIT_REG_INTR_MASK) & 0x4002u) | 0xFFFF07F0u;
	}
	return 0;
}

static int kermitShutdown(void)
{
	HW(KERMIT_REG_INTR_MASK) = 0;
	int res = sceKernelReleaseIntrHandler(0x1F);
	if (res != 0) {
		return res;
	}
	for (int i = 0; i < 16; i++) {
		res = sceKernelDeleteSema(g_respSema[i]);
		if (res != 0) {
			return res;
		}
	}
	return 0;
}

static int kermitInit(void)
{
	for (u32 i = 0; i < 16; i++) {
		SceUID s = sceKernelCreateSema("sceCompat", SCE_KERNEL_SA_THPRI, 0, 1, NULL);
		if (s <= 0) {
			return -1;
		}
		g_respSema[i] = s;
	}
	for (u32 i = 0; i < 3; i++) {
		SceUID s = sceKernelCreateSema("sceCompat", SCE_KERNEL_SA_THPRI, 1, 1, NULL);
		if (s <= 0) {
			return -1;
		}
		g_pipeSema[i] = s;
	}
	g_msgPipeId = sceKernelCreateMsgPipe("sceCompat", 1, 0x1000, (void *)0x40, NULL);
	if (g_msgPipeId < 0) {
		return -1;
	}
	if (sceKernelSendMsgPipe(g_msgPipeId, g_respSema, sizeof(g_respSema), 0, NULL, NULL) != 0) {
		return -1;
	}
	if (sceKernelRegisterIntrHandler(0x1F, 2, kermitIntrHandler, NULL, NULL) != 0) {
		return -1;
	}
	HW(KERMIT_REG_CFG_40) = 0xFFFFFFFFu;
	HW(KERMIT_REG_CFG_44) = 0;
	HW(KERMIT_REG_CFG_48) = 0xFFFFFFFFu;
	HW(KERMIT_REG_CFG_4C) = 0;
	HW(KERMIT_REG_TRIG) = 0;
	HW(KERMIT_REG_INTR_MASK) = (HW(KERMIT_REG_INTR_MASK) & 0x4002u) | 0xFFFF07F0u;
	if (sceKernelEnableIntr(0x1F) != 0) {
		return -1;
	}
	return 0;
}

static void kermitDispatchVirtualIntr(u32 mask)
{
	u32 bit = 0;
	while (mask != 0) {
		u32 tz = ctz32(mask);
		bit += tz;
		mask >>= (tz + 1);
		if (g_intrHandlers[bit] != NULL) {
			g_intrHandlers[bit]();
		}
		KERMIT_INTR_BOX[bit].intr = 0;
		bit++;
	}
}

static void *kermitTranslateAndFlushRequest(void *addr, u32 size)
{
	u32 raw = (u32)addr;
	u32 paddr = raw & 0x1FFFFFFFu;
	u32 low26 = raw & 0x03FFFFFFu;
	u32 kaddr;

	if ((paddr - 0x04000000u) <= 0x007FFFFFu) {
		kaddr = low26 + 0xAA1D0000u;
		if ((paddr - 0x00010000u) < 0x4000u) {
			return addr;
		}
	} else {
		kaddr = low26 + 0xAA1E0000u;
	}

	void *uncached = (void *)(kaddr & ~0x60000000u);
	sceKernelMemcpy(uncached, addr, size);
	sceKernelDcacheWritebackInvalidateRange(uncached, size);
	return (void *)kaddr;
}
