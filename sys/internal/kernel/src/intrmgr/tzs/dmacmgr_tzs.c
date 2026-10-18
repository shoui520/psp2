/*
 * dmacmgr_tzs.c: the secure-side words of the seven DMA controllers. The
 * secure world maps each controller and gives the kernel two monitor calls
 * that set its access control words (SceDmacRegisters.accessControl).
 * SKBL's revision (INTRMGR_SKBL) only starts the controllers and opens part
 * of the DMAC5 key ring to the Non-secure side.
 */

#include "intrmgr.h"
#include <dmacmgr/dmac_regs.h>
#include <lowio/pervasive.h>

#define DMAC_COUNT	7

#if defined(INTRMGR_SKBL)

static void *mapDmac(const char *name, SceUInt32 pa)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID memBlockId;
	void *base;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = pa;
	memBlockId = sceKernelAllocMemBlockForKernel(name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					     DMAC_REG_SIZE, &opt);
	if (memBlockId < 0) {
		SCE_DBG_PRINTF(0xb, 59, "map %s failed. 0x%08x\n", name, memBlockId);
		SCE_DBG_ASSERT_FAIL(60);
		return NULL;
	}
	sceKernelGetMemBlockBase(memBlockId, &base);
	return base;
}

static void StartDmac(int dmac)
{
	ScePervasiveForDriver_FF4B3CA6(dmac);
	ScePervasiveForDriver_A10583FF(dmac);
	ScePervasiveForDriver_AC475961(dmac);
}

int initDmacSecure(void)
{
	volatile SceDmac5KeyringRegisters *keyring;
	int i;

	for (i = 0; i < 6; i++)
		StartDmac(i);
	/* DMAC6 exists on these units only */
	if (SceDipswForDriver_A98FC2FD(0x9f) || SceDipswForDriver_A98FC2FD(0xc0) ||
	    SceDipswForDriver_A98FC2FD(0xc1) || SceDipswForDriver_A98FC2FD(0xc2))
		StartDmac(6);
	keyring = mapDmac("SceDmacmgrKeyringReg", DMAC5_KEYRING_REG_BASE);
	keyring->nonSecureAccess[0] = 0x200000ff;
	keyring->nonSecureAccess[1] = 0xffffffff;
	return 0;
}
#else

/* the access control words of each controller */
static SceUInt32 *s_dmacAccessControl[DMAC_COUNT];

static int dmacSetReg(SceUInt32 dmac, SceUInt32 off, SceUInt32 val)
{
	SCE_DBG_PRINTF_LEVEL(2, 0xb, 111, "dmac 0x%x off %x val 0x%08x\n", dmac, off, val);
	if (dmac >= DMAC_COUNT)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	s_dmacAccessControl[dmac][off] = val;
	return 0;
}

static int dmacSetAccess1(int dmac, int val, int arg2, int arg3)
{
	return dmacSetReg(dmac, 1, val);
}

static void *mapDmac(char n, SceUInt32 pa)
{
	char name[32];
	SceKernelAllocMemBlockKernelOpt opt;
	SceUID memBlockId;
	void *base;

	__builtin_memcpy(name, "SceDmacmgrDmacRegN", sizeof("SceDmacmgrDmacRegN"));
	name[18] = '0' + n;		/* after the N, as the original does */
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = pa;
	memBlockId = sceKernelAllocMemBlockForKernel(name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					     DMAC_REG_SIZE, &opt);
	if (memBlockId < 0) {
		SCE_DBG_PRINTF(0xb, 59, "map %s failed. 0x%08x\n", name, memBlockId);
		SCE_DBG_ASSERT_FAIL(60);
		return NULL;
	}
	sceKernelGetMemBlockBase(memBlockId, &base);
	return base;
}

static int dmacSetAccess0(int dmac, int val, int arg2, int arg3)
{
	return dmacSetReg(dmac, 0, val);
}

int initDmacSecure(void)
{
	SceDmac4Reg *dmac4;
	int last, i;

	s_dmacAccessControl[0] =
		((SceDmacRegisters *)mapDmac(0, DMAC0_REG_BASE))->accessControl;
	s_dmacAccessControl[1] =
		((SceDmacRegisters *)mapDmac(1, DMAC1_REG_BASE))->accessControl;
	s_dmacAccessControl[2] =
		((SceDmacRegisters *)mapDmac(2, DMAC2_REG_BASE))->accessControl;
	s_dmacAccessControl[3] =
		((SceDmacRegisters *)mapDmac(3, DMAC3_REG_BASE))->accessControl;
	dmac4 = mapDmac(4, DMAC4_REG_BASE);
	s_dmacAccessControl[4] = dmac4->accessControl;
	dmac4->secureGate[0].intrMask = 0xffff;
	dmac4->secureGate[1].intrMask = 0;
	dmac4->secureGate[2].intrMask = 0;
	dmac4->secureGate[3].intrMask = 0;
	s_dmacAccessControl[5] =
		((SceDmacRegisters *)mapDmac(5, DMAC5_REG_BASE))->accessControl;
	s_dmacAccessControl[6] =
		((SceDmacRegisters *)mapDmac(6, DMAC6_REG_BASE))->accessControl;

	/* DMAC6 exists on these units only */
	if (SceDipswForDriver_A98FC2FD(0x9f) || SceDipswForDriver_A98FC2FD(0xc0) ||
	    SceDipswForDriver_A98FC2FD(0xc1) || SceDipswForDriver_A98FC2FD(0xc2))
		last = 6;
	else
		last = 5;
	for (i = 0; i <= last; i++) {
		s_dmacAccessControl[i][0] = 3;
		s_dmacAccessControl[i][1] = 0;
	}
	s_dmacAccessControl[4][0] = 0xffff;

	if (sceKernelRegisterMonitorCallHandler(DMAC_MONCALL_SET_ACCESS0, dmacSetAccess0) < 0)
		SCE_DBG_ASSERT_FAIL(200);
	if (sceKernelRegisterMonitorCallHandler(DMAC_MONCALL_SET_ACCESS1, dmacSetAccess1) < 0)
		SCE_DBG_ASSERT_FAIL(206);
	return 0;
}
#endif
