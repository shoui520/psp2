#include <kermit.h>

static int hpremoteSysEventHandler(int ev_id, char *ev_name, void *param, int *result);

static SceSysEventHandler g_sysEventHandler = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceHpRemote",
	.type_mask = 0x00FFFF00,
	.handler = hpremoteSysEventHandler,
};

static SceUID g_callbacks[16];

typedef struct HprmUidCB {
	u32 header[5];
	u16 sizeAndType;
	u16 attr;
} HprmUidCB;

static inline u32 hprmShiftK1(void)
{
	u32 old = pspGetK1();
	pspSetK1(old << 11);
	return old;
}

int sceHprmRegisterCallback(int slot, SceUID cbid)
{
	u32 old_k1 = hprmShiftK1();

	if ((u32)(slot + 1) >= 0x21u) {
		pspSetK1(old_k1);
		return 0x80000102;
	}
	if ((s32)pspGetK1() < 0 && slot >= 16) {
		pspSetK1(old_k1);
		return 0x80000023;
	}

	if (sceKernelGetThreadmanIdType(cbid) != 8) {
		pspSetK1(old_k1);
		return 0x80000100;
	}

	void *cbRaw = NULL;
	if (sceKernelGetUIDcontrolBlock(cbid, &cbRaw) != 0) {
		pspSetK1(old_k1);
		return 0x80000023;
	}
	HprmUidCB *cb = (HprmUidCB *)cbRaw;

	u32 perm = 2;
	if ((pspGetK1() >> 31) != 0) {
		perm = cb->attr & 2u;
	}
	if (perm == 0) {
		pspSetK1(old_k1);
		return 0x80000023;
	}

	int intr = sceKernelCpuSuspendIntr();
	int res;
	if (slot != -1) {
		if (g_callbacks[slot] < 0) {
			g_callbacks[slot] = cbid;
			res = 0;
		} else {
			res = 0x80000020;
		}
	} else if ((s32)pspGetK1() >= 0) {
		res = 0x80000004;
	} else {
		res = 0x80000022;
		for (int i = 0; i < 16; i++) {
			if (g_callbacks[i] < 0) {
				g_callbacks[i] = cbid;
				res = i;
				break;
			}
		}
	}
	sceKernelCpuResumeIntr(intr);
	pspSetK1(old_k1);
	return res;
}

int sceHprmUnregisterCallback(u32 slot)
{
	u32 old_k1 = hprmShiftK1();

	if (slot >= 0x20u) {
		pspSetK1(old_k1);
		return 0x80000102;
	}
	if ((s32)pspGetK1() < 0 && (s32)slot >= 16) {
		pspSetK1(old_k1);
		return 0x80000023;
	}
	pspSetK1(old_k1);
	if (g_callbacks[slot] < 0) {
		return 0x80000025;
	}
	g_callbacks[slot] = -1;
	return 0;
}

int module_start(SceSize args, void *argp)
{
	for (u32 i = 0; i < 16; i++) {
		g_callbacks[i] = -1;
	}
	sceKernelRegisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int sceHprm_driver_1C5BC5A0(void)
{
	for (u32 i = 0; i < 16; i++) {
		g_callbacks[i] = -1;
	}
	sceKernelRegisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int sceHprm_driver_588845DA(void)
{
	sceKernelUnregisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

int sceHprm_driver_526BB7F4(void)
{
	return 0;
}

int sceHprm_driver_2C7B8B05(void)
{
	return 0;
}

int sceHprm_driver_8AEC8EE9(void)
{
	return 0;
}

int sceHprm_driver_2E1A7B7C(void)
{
	return 0;
}

int sceHprm_71B5FB67(void)
{
	volatile u32 seq1, seq2;
	u32 val;
	do {
		seq1 = HW(0xBFC00104u);
		val = HW(0xBFC00108u);
		seq2 = HW(0xBFC00100u);
	} while (seq1 != seq2);
	return val & 1u;
}

int sceHprm_driver_1C3525DD(void)
{
	return 0;
}

int sceHprm_driver_DFE72E99(void)
{
	return 1;
}

int sceHprmIsHeadphoneExist(void)
{
	volatile u32 seq1, seq2;
	u32 val;
	do {
		seq1 = HW(0xBFC00104u);
		val = HW(0xBFC00108u);
		seq2 = HW(0xBFC00100u);
	} while (seq1 != seq2);
	return val & 1u;
}

int sceHprmIsRemoteExist(void)
{
	return 0;
}

int sceHprmIsMicrophoneExist(void)
{
	return 1;
}

int sceHprm_driver_9BDE174B(void)
{
	return 0x10001;
}

int sceHprm_driver_B2BEADB8(void)
{
	return 0;
}

int sceHprm_driver_A0B1A19B(void)
{
	return 0;
}

int sceHprm_driver_0B83352B(void)
{
	return 0x80000025;
}

int sceHprm_driver_06C478D4(void)
{
	return 0;
}

int sceHprm_driver_C00DE13F(void)
{
	return 0;
}

int sceHprm_driver_B6895C1C(void)
{
	return 0;
}

int sceHprm_driver_2F17A548(void)
{
	return 0;
}

int sceHprm_driver_D3F17366(u32 *out)
{
	if (out != NULL) {
		*out = 0;
	}
	return 0;
}

int sceHprmPeekCurrentKey(u32 *key)
{
	s32 shifted_k1 = (s32)(pspGetK1() << 11);
	if ((shifted_k1 & (s32)key) < 0) {
		return 0x80000023;
	}
	*key = 0;
	return 0;
}

int sceHprmPeekLatch(u32 *latch)
{
	u32 old_k1 = hprmShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	if ((s32)(pspGetK1() & (u32)latch) < 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(old_k1);
		return 0x80000023;
	}
	*(volatile u32 *)&latch[3] = 0;
	*(volatile u32 *)&latch[0] = 0;
	*(volatile u32 *)&latch[1] = 0;
	*(volatile u32 *)&latch[2] = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(old_k1);
	return 0;
}

int sceHprmReadLatch(u32 *latch)
{
	u32 old_k1 = hprmShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	if ((s32)(pspGetK1() & (u32)latch) < 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(old_k1);
		return 0x80000023;
	}
	*(volatile u32 *)&latch[3] = 0;
	*(volatile u32 *)&latch[0] = 0;
	*(volatile u32 *)&latch[1] = 0;
	*(volatile u32 *)&latch[2] = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(old_k1);
	return 0;
}

int sceHprm_3953DE6B(void)
{
	return 0;
}

int sceHprm_396FD885(void)
{
	return 0;
}

int sceHprm_driver_DC895B2B(void)
{
	return 0;
}

int sceHprm_driver_8C728BB4(u32 *outFirst, u32 *outSecond)
{
	if (outFirst != NULL) {
		*outFirst = 0;
	}
	if (outSecond != NULL) {
		*outSecond = 0;
	}
	return 0;
}

int sceHprm_driver_301063CC(void)
{
	return 0;
}

static int hpremoteSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	return 0;
}
