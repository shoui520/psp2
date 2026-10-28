#include <kermit.h>

static int hpremoteSysEventHandler(int ev_id, char *ev_name, void *param, int *result);

static SceSysEventHandler g_sysEventHandler = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceHpRemote",
	.type_mask = 0x00FFFF00,
	.handler = hpremoteSysEventHandler,
};

static SceUID g_callbacks[16];

int sceHprm_C7154136(int slot, SceUID cbid)
{
	u32 old_k1 = pspGetK1();
	s32 shifted_k1 = (s32)(old_k1 << 11);

	if ((u32)(slot + 1) >= 0x21u) {
		pspSetK1(old_k1);
		return 0x80000102;
	}
	if (shifted_k1 < 0 && slot >= 16) {
		pspSetK1(old_k1);
		return 0x80000023;
	}

	pspSetK1((u32)shifted_k1);
	if (sceKernelGetThreadmanIdType(cbid) != 8) {
		pspSetK1(old_k1);
		return 0x80000100;
	}

	void *cb = NULL;
	if (sceKernelGetUIDcontrolBlock(cbid, &cb) != 0) {
		pspSetK1(old_k1);
		return 0x80000023;
	}

	u32 perm = 2;
	if (((u32)shifted_k1 >> 31) != 0) {
		perm = *(u16 *)((u8 *)cb + 0x16) & 2u;
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
	} else if (shifted_k1 >= 0) {
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

int sceHprm_444ED0B7(u32 slot)
{
	u32 old_k1 = pspGetK1();
	s32 shifted_k1 = (s32)(old_k1 << 11);

	if (slot >= 0x20u) {
		pspSetK1(old_k1);
		return 0x80000102;
	}
	if (shifted_k1 < 0 && (s32)slot >= 16) {
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
	u32 old_k1 = pspGetK1();
	s32 shifted_k1 = (s32)(old_k1 << 11);
	pspSetK1((u32)shifted_k1);

	int intr = sceKernelCpuSuspendIntr();
	if ((shifted_k1 & (s32)latch) < 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(old_k1);
		return 0x80000023;
	}
	latch[3] = 0;
	latch[0] = 0;
	latch[1] = 0;
	latch[2] = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(old_k1);
	return 0;
}

int sceHprmReadLatch(u32 *latch)
{
	u32 old_k1 = pspGetK1();
	s32 shifted_k1 = (s32)(old_k1 << 11);
	pspSetK1((u32)shifted_k1);

	int intr = sceKernelCpuSuspendIntr();
	if ((shifted_k1 & (s32)latch) < 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(old_k1);
		return 0x80000023;
	}
	latch[3] = 0;
	latch[0] = 0;
	latch[1] = 0;
	latch[2] = 0;
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

int sceHprm_driver_8C728BB4(u32 *a0, u32 *a1)
{
	if (a0 != NULL) {
		*a0 = 0;
	}
	if (a1 != NULL) {
		*a1 = 0;
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
