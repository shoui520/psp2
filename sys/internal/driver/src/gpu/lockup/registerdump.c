/* Sony GPU register-list lookup and validation. */
#include <sysmem/sysmem.h>
#include "../../gpu_es4/srvkm/gpu-dump.h"
#include <sysmem/debug.h>
#include <sysmem/barrier.h>
#include <rtc/rtc.h>
#include <lowio/pervasive.h>
#include <string.h>
#include "sgx543_v1.164defs.h"
#include <gpu/sgx_regs.h>
#include "sgxmpdefs.h"
#include <power/power.h>
#include <gpu/gpu-mmu.h>

/* SCE capture selections, identical in both TOOL originals. Numeric register
 * offsets have no register identifier in the pristine candidate hardware defs.
 */
static const unsigned perfRegisterSelectors[109] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 
	0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 
	0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x40, 0x41, 0x42, 
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x60, 
	0x61, 0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 
	0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 
	0x7a,
};

static const unsigned coreRegisterOffsets[399] = {
	EUR_CR_CLKGATECTL, EUR_CR_CLKGATECTL2, EUR_CR_CLKGATESTATUS, 
	EUR_CR_CLKGATECTLOVR, 0x0010, EUR_CR_POWER, EUR_CR_CORE_ID, 
	EUR_CR_CORE_REVISION, EUR_CR_DESIGNER_REV_FIELD1, 
	EUR_CR_DESIGNER_REV_FIELD2, 0x0040, 0x0064, 0x0078, EUR_CR_SOFT_RESET, 
	0x0100, EUR_CR_EVENT_HOST_ENABLE2, EUR_CR_EVENT_STATUS2, 
	EUR_CR_EVENT_STATUS, EUR_CR_EVENT_HOST_ENABLE, 0x0138, 0x013c, EUR_CR_TIMER, 
	0x0148, 0x0204, 0x0208, 0x020c, 0x0210, 0x0214, 0x0218, 0x021c, 0x0220, 
	0x0228, 0x022c, 0x0230, 0x0234, 0x0238, 0x023c, 0x0240, 0x0244, 0x0248, 
	0x0250, 0x0254, 0x0258, 0x025c, 0x0260, 0x026c, 0x0270, 0x0274, 0x0280, 
	0x0288, 0x0294, 0x029c, 0x0304, 0x030c, 0x0310, 0x0324, 0x0328, 0x032c, 
	0x0334, 0x0338, 0x033c, 0x0340, 0x0344, 0x0348, 0x034c, 0x0350, 0x0354, 
	0x0358, 0x0378, 0x0384, 0x0388, 0x0394, 0x0398, 0x039c, 0x0400, 0x0404, 
	0x0408, 0x040c, 0x0410, 0x0414, 0x0418, 0x041c, 0x0420, 0x0424, 0x042c, 
	0x0438, 0x0440, 0x0444, 0x044c, 0x0450, 0x0480, 0x0484, 0x0488, 0x048c, 
	0x0490, 0x04b8, 0x04bc, 0x04c0, 0x04c4, 0x04c8, 0x04cc, 0x04dc, 0x04e4, 
	0x04ec, 0x0500, 0x0504, 0x0508, 0x050c, 0x0520, 0x0524, 0x0528, 0x053c, 
	0x0540, 0x0544, 0x0548, 0x054c, 0x0550, 0x0554, 0x0558, 0x055c, 0x0560, 
	0x0564, 0x0600, 0x0604, 0x0614, 0x0618, 0x061c, 0x0620, 0x0624, 0x0628, 
	0x062c, 0x0630, 0x0634, 0x0638, 0x063c, 0x0640, 0x0644, 0x0648, 0x064c, 
	0x0658, 0x065c, 0x0660, 0x0664, 0x0670, 0x06a8, 0x0700, 0x070c, 0x0710, 
	0x071c, 0x0720, 0x0724, 0x0728, 0x072c, 0x0730, 0x0734, 0x0738, 0x073c, 
	0x0740, 0x0744, 0x074c, 0x0750, 0x0754, 0x0770, 0x077c, 0x0784, 0x07a4, 
	0x0800, 0x0804, 0x080c, 0x0814, 0x0818, 0x081c, 0x0820, 0x0824, 0x0828, 
	0x082c, 0x0830, 0x0834, 0x0838, 0x083c, 0x0840, 0x0844, 0x0848, 0x084c, 
	0x0850, 0x0854, 0x0858, 0x085c, 0x0860, 0x0864, 0x0868, 0x086c, 0x0870, 
	0x0900, 0x0904, 0x0908, 0x090c, 0x0910, 0x0914, 0x0918, 0x091c, 0x0920, 
	0x0924, 0x0a00, 0x0a04, EUR_CR_USE_CODE_BASE_0, EUR_CR_USE_CODE_BASE_1, 
	EUR_CR_USE_CODE_BASE_2, EUR_CR_USE_CODE_BASE_3, EUR_CR_USE_CODE_BASE_4, 
	EUR_CR_USE_CODE_BASE_5, EUR_CR_USE_CODE_BASE_6, EUR_CR_USE_CODE_BASE_7, 
	EUR_CR_USE_CODE_BASE_8, EUR_CR_USE_CODE_BASE_9, EUR_CR_USE_CODE_BASE_10, 
	EUR_CR_USE_CODE_BASE_11, EUR_CR_USE_CODE_BASE_12, EUR_CR_USE_CODE_BASE_13, 
	EUR_CR_USE_CODE_BASE_14, EUR_CR_USE_CODE_BASE_15, 0x0a50, 0x0a54, 0x0a58, 
	0x0a5c, 0x0a60, 0x0a64, 0x0a68, 0x0a6c, 0x0a70, 0x0a74, 0x0a7c, 0x0a80, 
	0x0a84, 0x0a88, 0x0a8c, 0x0a90, 0x0a94, 0x0a98, 0x0a9c, 0x0aa0, 0x0aa4, 
	0x0aa8, 0x0ab4, 0x0ab8, 0x0abc, EUR_CR_EVENT_KICKER, EUR_CR_EVENT_TIMER, 
	0x0ae4, 0x0ae8, 0x0b08, 0x0b0c, 0x0b10, 0x0b14, 0x0b18, 0x0b1c, 0x0b20, 
	0x0b24, 0x0b28, 0x0b2c, 0x0b30, 0x0b38, 0x0b40, 0x0b44, 0x0b48, 0x0b4c, 
	0x0b50, 0x0b54, 0x0b58, 0x0b5c, 0x0b60, 0x0b64, 0x0b68, 0x0b6c, 0x0b70, 
	0x0b74, 0x0b78, 0x0b7c, 0x0b80, 0x0b84, 0x0b88, 0x0b8c, 0x0b90, 0x0b94, 
	0x0b98, 0x0b9c, 0x0ba0, 0x0ba4, 0x0ba8, 0x0bac, 0x0bb0, 0x0bb4, 
	EUR_CR_BIF_CTRL, EUR_CR_BIF_INT_STAT, EUR_CR_BIF_FAULT, EUR_CR_BIF_TILE0, 
	EUR_CR_BIF_TILE1, EUR_CR_BIF_TILE2, EUR_CR_BIF_TILE3, EUR_CR_BIF_TILE4, 
	EUR_CR_BIF_TILE5, EUR_CR_BIF_TILE6, EUR_CR_BIF_TILE7, EUR_CR_BIF_TILE8, 
	EUR_CR_BIF_TILE9, EUR_CR_BIF_CTRL_INVAL, EUR_CR_BIF_DIR_LIST_BASE1, 
	EUR_CR_BIF_DIR_LIST_BASE2, EUR_CR_BIF_DIR_LIST_BASE3, 
	EUR_CR_BIF_DIR_LIST_BASE4, EUR_CR_BIF_DIR_LIST_BASE5, 
	EUR_CR_BIF_DIR_LIST_BASE6, EUR_CR_BIF_DIR_LIST_BASE7, EUR_CR_BIF_BANK_SET, 
	EUR_CR_BIF_BANK0, EUR_CR_BIF_BANK1, EUR_CR_BIF_DIR_LIST_BASE0, 
	EUR_CR_BIF_TA_REQ_BASE, EUR_CR_BIF_MEM_REQ_STAT, EUR_CR_BIF_3D_REQ_BASE, 
	EUR_CR_BIF_ZLS_REQ_BASE, EUR_CR_BIF_BANK_STATUS, 0x0cc0, 0x0cc4, 
	EUR_CR_BIF_MMU_CTRL, 0x0e00, EUR_CR_2D_BLIT_STATUS, 0x0e08, 0x0e0c, 
	EUR_CR_2D_VIRTUAL_FIFO_0, EUR_CR_2D_VIRTUAL_FIFO_1, 0x0e80, 0x0e84, 0x0e88, 
	0x0e8c, 0x0e90, 0x0e94, 0x0e98, 0x0eb4, 0x0eb8, 0x0ebc, 0x0ec0, 0x0ec4, 
	0x0ec8, 0x0ecc, 0x0ed0, 0x0ed4, 0x0ed8, 0x0edc, 0x0ee0, 0x0ee4, 0x0ee8, 
	0x0eec, 0x0ef0, 0x0ef4, 0x0ef8, 0x0efc, 0x0f00, 0x0f04, 0x0f08, 0x0f0c, 
	0x0f10, 0x0f14, 0x0f18, 0x0f1c, 0x0f20, 0x0f24, 0x0f28, 0x0f2c, 0x0f30, 
	0x0f34, 0x0f38, 0x0f3c, 0x0f40, EUR_CR_BREAKPOINT0_START, 
	EUR_CR_BREAKPOINT0_END, EUR_CR_BREAKPOINT0, EUR_CR_BREAKPOINT1_START, 
	EUR_CR_BREAKPOINT1_END, EUR_CR_BREAKPOINT1, EUR_CR_BREAKPOINT2_START, 
	EUR_CR_BREAKPOINT2_END, EUR_CR_BREAKPOINT2, EUR_CR_BREAKPOINT3_START, 
	EUR_CR_BREAKPOINT3_END, EUR_CR_BREAKPOINT3, EUR_CR_BREAKPOINT_READ, 
	EUR_CR_BREAKPOINT, EUR_CR_BREAKPOINT_TRAP_INFO0, 
	EUR_CR_BREAKPOINT_TRAP_INFO1, 0x0f8c, 0x0f90, 0x0f94, 0x0f9c, 0x0fa0, 
	0x0fa4, 0x1000, 0x1004, 0x1008,
};

static const unsigned masterRegisterOffsets[266] = {
	EUR_CR_MASTER_CORE, 0x4004, 0x4008, 0x400c, EUR_CR_MASTER_CORE_ID, 
	EUR_CR_MASTER_CORE_REVISION, 0x4018, 0x401c, 0x4020, 0x4024, 0x4028, 
	EUR_CR_MASTER_SOFT_RESET, 0x4090, 0x4110, 0x4118, 0x412c, 0x4130, 0x4138, 
	0x413c, 0x4144, 0x4148, 0x414c, 0x4150, 0x4154, 0x4174, 0x4178, 0x417c, 
	0x4204, 0x4214, 0x4238, 0x4258, 0x425c, 0x4280, 0x4288, 0x4294, 0x4324, 
	0x4328, 0x432c, 0x4334, 0x433c, 0x4340, 0x4344, 0x4348, 0x4360, 0x4364, 
	0x4394, 0x4400, 0x4404, 0x4408, 0x440c, 0x4410, 0x442c, 0x4510, 0x4514, 
	0x4518, 0x451c, 0x4560, 0x4564, 0x4568, 0x456c, 0x4570, 0x4574, 0x4578, 
	0x457c, 0x4580, 0x4584, 0x4588, 0x458c, 0x4590, 0x4594, 0x4598, 0x459c, 
	0x45a0, 0x45a4, 0x45a8, 0x45ac, 0x45b0, 0x45b4, 0x45b8, 0x45bc, 0x45c0, 
	0x45c4, 0x45c8, 0x45cc, 0x4600, 0x4604, 0x4614, 0x4618, 0x461c, 0x4620, 
	0x4624, 0x4628, 0x462c, 0x4630, 0x4638, 0x463c, 0x4640, 0x4644, 0x4648, 
	0x464c, 0x4658, 0x465c, 0x4660, 0x4664, 0x4670, 0x4674, 0x4684, 0x4700, 
	0x470c, 0x4710, 0x471c, 0x4720, 0x4724, 0x4728, 0x472c, 0x4730, 0x4734, 
	0x473c, 0x4740, 0x4744, 0x4750, 0x4754, 0x4758, 0x475c, 0x4760, 0x4764, 
	0x4768, 0x476c, 0x4770, 0x4774, 0x4778, 0x477c, 0x4780, 0x4784, 0x478c, 
	0x4790, 0x4794, 0x4798, 0x479c, 0x47a0, 0x47a4, 0x47a8, 0x47ac, 0x47b0, 
	0x47b4, 0x47b8, 0x47bc, 0x47c0, 0x47c4, 0x47c8, 0x47cc, 0x47d0, 0x47d4, 
	0x47d8, 0x47dc, 0x47e0, 0x47e4, 0x47e8, 0x47ec, 0x47f0, 0x47f4, 0x47f8, 
	0x47fc, 0x4800, 0x4804, 0x4808, 0x480c, 0x4810, 0x4818, 0x481c, 0x4820, 
	0x4824, 0x4828, 0x482c, 0x4830, 0x4834, 0x4838, 0x483c, 0x4840, 0x4844, 
	0x4848, 0x4900, 0x4904, 0x4908, 0x490c, 0x4910, 0x4914, 0x4918, 0x491c, 
	0x4920, 0x4924, 0x4a50, 0x4a58, EUR_CR_MASTER_BIF_CTRL, 0x4c04, 0x4c08, 
	EUR_CR_MASTER_BIF_CTRL_INVAL, 0x4c38, 0x4c3c, 0x4c40, 0x4c44, 0x4c48, 
	0x4c4c, 0x4c50, 0x4c74, 0x4c78, 0x4c7c, 0x4c84, 0x4ca8, 0x4cac, 0x4cb4, 
	EUR_CR_MASTER_BIF_MMU_CTRL, EUR_CR_MASTER_SLC_CTRL, 
	EUR_CR_MASTER_SLC_CTRL_BYPASS, EUR_CR_MASTER_SLC_CTRL_USSE_INVAL, 0x4d0c, 
	0x4d10, 0x4d18, 0x4d1c, 0x4d20, 0x4d24, EUR_CR_MASTER_SLC_CTRL_INVAL, 
	EUR_CR_MASTER_SLC_CTRL_FLUSH, 0x4d30, EUR_CR_MASTER_SLC_CTRL_FLUSH_INV, 
	0x4e80, 0x4e84, 0x4e88, 0x4e8c, 0x4e90, 0x4e94, 0x4e98, 0x4eb4, 0x4eb8, 
	0x4ebc, 0x4ec0, 0x4ec4, 0x4ec8, 0x4ecc, 0x4ed0, 0x4ed4, 0x4ed8, 0x4edc, 
	0x4ee0, 0x4ee4, 0x4ee8, 0x4eec, 0x4ef0, 0x4ef4, 0x4ef8, 0x4efc, 0x4f00, 
	0x4f04, 0x4f08, 0x4f0c, 0x4f10, 0x4f14, EUR_CR_MASTER_BREAKPOINT_READ, 
	EUR_CR_MASTER_BREAKPOINT_TRAP, EUR_CR_MASTER_BREAKPOINT, 
	EUR_CR_MASTER_BREAKPOINT_TRAP_INFO0, EUR_CR_MASTER_BREAKPOINT_TRAP_INFO1, 
	0x4f30, 0x4f34, 0x4f38, 0x4f3c,
};

/* SCE directory-bank capture order, identical in both TOOL originals. */
static const unsigned coreDirectoryRegisters[8] = {
	EUR_CR_BIF_DIR_LIST_BASE0, EUR_CR_BIF_DIR_LIST_BASE1,
	EUR_CR_BIF_DIR_LIST_BASE2, EUR_CR_BIF_DIR_LIST_BASE3,
	EUR_CR_BIF_DIR_LIST_BASE4, EUR_CR_BIF_DIR_LIST_BASE5,
	EUR_CR_BIF_DIR_LIST_BASE6, EUR_CR_BIF_DIR_LIST_BASE7,
};

static const unsigned masterDirectoryRegisters[8] = {
	EUR_CR_BIF_DIR_LIST_BASE0 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE1 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE2 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE3 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE4 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE5 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE6 + SGX_REG_BANK_SIZE,
	EUR_CR_BIF_DIR_LIST_BASE7 + SGX_REG_BANK_SIZE,
};

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/lockup/registerdump.c"

PVRSRV_DEVICE_NODE *GetGpuDeviceNode(void);
/* The existing module import uses the user spelling for driver NID 0A750DEE. */

#define GPU_REGISTER(base, offset) (*(volatile unsigned *)((unsigned char *)(base) + (offset)))

int FindRegisterIndex(const unsigned *registers, int count, unsigned address)
{
	int first = 0, middle;
	unsigned reg;

	if (gpuDumpRegion.state.registerListsUnsorted)
		return (int)0x804c000a;
	if (!registers)
		return (int)0x804c0000;
	if (count <= 0)
		return (int)0x804c0000;
	while (count) {
		middle = first + (count >> 1);
		reg = registers[middle];
		if (reg == address)
			return middle;
		if (reg < address) {
			first = middle + 1;
			count = (count - 1) >> 1;
		} else {
			count >>= 1;
		}
	}
	return -1;
}

int sceRegisterDumpCheckSortedRegisterList(const unsigned *registers,
		unsigned count)
{
	unsigned index;

	for (index = 1; index < count; index++) {
		if (registers[index - 1] >= registers[index]) {
			SCE_DBG_PRINTF(0, 256,
					"*** ERROR: list entry %u/%u broken ***\n",
					index - 1, count);
			return (int)0x804c000a;
		}
	}
	return 0;
}

int GetRegisterDumpMasterRegister(const SceGpuRegisterDump *dump,
		unsigned *value, unsigned address)
{
	int index;

	if (!dump)
		return (int)0x804c0000;
	if (!value)
		return (int)0x804c0000;
	*value = 0xdeadc0de;
	if (!(dump->flags & 1))
		return (int)0x804c0000;
	index = FindRegisterIndex(masterRegisterOffsets,
			GPU_DUMP_MASTER_REGISTER_COUNT, address);
	if (index < 0)
		return (int)0x804c0000;
	*value = dump->masterRegisters[index].value;
	return 0;
}

int GetRegisterDumpCoreRegister(const SceGpuRegisterDump *dump,
		unsigned *value, unsigned address, unsigned core)
{
	int index;

	if (!dump)
		return (int)0x804c0000;
	if (!value)
		return (int)0x804c0000;
	*value = 0xdeadc0de;
	if (!(dump->flags & 2))
		return (int)0x804c0000;
	if (core > 3)
		return (int)0x804c0000;
	index = FindRegisterIndex(coreRegisterOffsets,
			GPU_DUMP_CORE_REGISTER_COUNT, address);
	if (index < 0)
		return (int)0x804c0000;
	*value = dump->coreRegisters[index].values[core];
	return 0;
}

int SceGpuRegisterDumpCoreExtendedPerfRegisters(const SceGpuRegisterDump *dump,
		unsigned *value, unsigned selector, unsigned core)
{
	int index;

	if (!dump)
		return (int)0x804c0000;
	if (!value)
		return (int)0x804c0000;
	*value = 0xdeadc0de;
	if (!(dump->flags & 4))
		return (int)0x804c0000;
	if (core > 3)
		return (int)0x804c0000;
	index = FindRegisterIndex(perfRegisterSelectors,
			GPU_DUMP_PERF_REGISTER_COUNT, selector);
	if (index < 0)
		return (int)0x804c0000;
	*value = dump->perfRegisters[index].values[core];
	return 0;
}

int sceGpuRegisterDumpCollect(SceGpuRegisterDump *dump, unsigned size,
		unsigned flags)
{
	PVRSRV_DEVICE_NODE *node;
	PVRSRV_SGXDEV_INFO *imgDevice;
	void *registers;
	unsigned index, core, selector, first, second, offset;
	unsigned savedEnable[4], savedFirst[4], savedSecond[4];
	int result;

	if (sceKernelIsAccessibleRange(3, dump, size) < 0 || size < sizeof(*dump)) {
		SCE_DBG_PRINTF(0, 398, "Invalid register dump area\n");
		return (int)0x804c0000;
	}
	memset(dump, 0, sizeof(*dump));
	if (flags & ~7U)
		return (int)0x804c0000;
	dump->version = 0x10003;
	result = sceRtcGetCurrentClockLocalTime(&dump->localTime);
	if (result < 0) {
		SCE_DBG_PRINTF(6, 414,
				"sceRtcGetCurrentClockLocalTime() returned 0x%08x\n", result);
		memset(&dump->localTime, 0, sizeof(dump->localTime));
	}
	node = GetGpuDeviceNode();
	imgDevice = node ? node->pvDevice : NULL;
	result = sceKernelIsAccessibleRange(3, imgDevice, sizeof(*imgDevice));
	if (result < 0) {
		SCE_DBG_PRINTF(0, 431,
				"sceKernelIsAccessibleRange(imgDevice) returned 0x%08x\n", result);
		return (int)0x804c0000;
	}
	registers = imgDevice->pvRegsBaseKM;
	if (!imgDevice->ui32RegSize || !registers) {
		SCE_DBG_PRINTF(0, 443,
				"Invalid GPU control registers (0x%01X@%p)\n",
				imgDevice->ui32RegSize, registers);
		return (int)0x804c000c;
	}
	result = sceKernelIsAccessibleRange(3, registers, imgDevice->ui32RegSize);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 451,
				"sceKernelIsAccessibleRange(GPU control registers) returned 0x%08x\n",
				result);
		return (int)0x804c000c;
	}
	dump->socRevision = ScePervasiveForDriver_714EEFB7() & 0x1ffff;
	result = scePowerGetGpuClockFrequencyInternal(&dump->gpuClockFirst,
			&dump->gpuClockSecond);
	if (result < 0) {
		SCE_DBG_PRINTF(6, 463,
				"scePowerGetGpuClockFrequencyInternal() returned 0x%08x\n", result);
		dump->gpuClockSecond = 0;
		dump->gpuClockFirst = 0;
	}
	result = scePowerGetGpuXbarClockFrequency();
	if (result < 0) {
		dump->gpuXbarClock = 0;
		SCE_DBG_PRINTF(6, 470,
				"scePowerGetGpuXbarClockFrequency() returned 0x%08x\n", result);
	} else {
		dump->gpuXbarClock = result;
	}
	if (!dump->gpuXbarClock || !dump->gpuClockFirst || !dump->gpuClockSecond) {
		SCE_DBG_PRINTF(0, 478, "GPU not available?\n");
		return (int)0x804c000c;
	}
	if (flags & 1) {
		dump->masterRegisterCount = GPU_DUMP_MASTER_REGISTER_COUNT;
		for (index = 0; index < GPU_DUMP_MASTER_REGISTER_COUNT; index++) {
			offset = masterRegisterOffsets[index];
			dump->masterRegisters[index].registerOffset = offset;
			dmb();
			dump->masterRegisters[index].value = GPU_REGISTER(registers, offset);
		}
		dump->flags |= 1;
	}
	dmb();
	dump->coreCount = (GPU_REGISTER(registers, EUR_CR_MASTER_CORE) & 3) + 1;
	if (flags & 2) {
		dump->coreRegisterCount = GPU_DUMP_CORE_REGISTER_COUNT;
		for (index = 0; index < GPU_DUMP_CORE_REGISTER_COUNT; index++) {
			offset = coreRegisterOffsets[index];
			dump->coreRegisters[index].registerOffset = offset;
			for (core = 0; core < dump->coreCount; core++) {
				dmb();
				dump->coreRegisters[index].values[core] =
					GPU_REGISTER(registers, SGX_MP_CORE_SELECT(offset, core));
			}
		}
		dump->flags |= 2;
	}
	if (!(flags & 4))
		return 0;
	dump->perfRegisterCount = GPU_DUMP_PERF_REGISTER_COUNT;
	for (core = 0; core < dump->coreCount; core++) {
		dmb();
		savedEnable[core] = GPU_REGISTER(registers,
				SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_ENABLE, core));
		dmb();
		savedFirst[core] = GPU_REGISTER(registers,
				SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_SELECT_FIRST, core));
		dmb();
		savedSecond[core] = GPU_REGISTER(registers,
				SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_SELECT_SECOND, core));
		dmb();
		GPU_REGISTER(registers, SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_ENABLE, core)) = 1;
		dsb();
	}
	for (index = 0; index < GPU_DUMP_PERF_REGISTER_COUNT; index++) {
		selector = (unsigned char)perfRegisterSelectors[index];
		dump->perfRegisters[index].selector = selector;
		selector |= selector << 16;
		first = selector | SCE_SGX_COUNTER_FIRST_SOURCE;
		second = selector | SCE_SGX_COUNTER_SECOND_SOURCE;
		for (core = 0; core < dump->coreCount; core++) {
			dmb();
			GPU_REGISTER(registers, SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_SELECT_FIRST, core)) = first;
			dsb();
			dmb();
			GPU_REGISTER(registers, SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_SELECT_SECOND, core)) = second;
			dsb();
			dmb();
			dump->perfRegisters[index].values[core] = GPU_REGISTER(registers,
					SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_VALUE, core));
		}
	}
	for (core = 0; core < dump->coreCount; core++) {
		dmb();
		GPU_REGISTER(registers, SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_SELECT_FIRST, core)) = savedFirst[core];
		dsb();
		dmb();
		GPU_REGISTER(registers, SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_SELECT_SECOND, core)) = savedSecond[core];
		dsb();
		dmb();
		GPU_REGISTER(registers, SGX_MP_CORE_SELECT(SCE_SGX_COUNTER_ENABLE, core)) = savedEnable[core];
		dsb();
	}
	dump->flags |= 0x24;
	return 0;
}

int CheckRegisterDumpRegisterLists(void)
{
	if (sceRegisterDumpCheckSortedRegisterList(masterRegisterOffsets, 266))
		gpuDumpRegion.state.registerListsUnsorted = 1;
	if (sceRegisterDumpCheckSortedRegisterList(coreRegisterOffsets, 399))
		gpuDumpRegion.state.registerListsUnsorted = 1;
	if (sceRegisterDumpCheckSortedRegisterList(perfRegisterSelectors, 109))
		gpuDumpRegion.state.registerListsUnsorted = 1;
	return 0;
}

int ExtractRegisterDumpFrameInfo(const SceGpuRegisterDump *dump,
		unsigned *first, unsigned *second)
{
	if (!dump)
		return (int)0x804c000c;
	*first = dump->frameNumSceneIndex[0];
	*second = dump->frameNumSceneIndex[2];
	return 0;
}

int CollectRegisterDumpContexts(SceGpuRegisterDump *dump)
{
	unsigned core, bank, value, processUid;
	unsigned char index;
	const char *name;
	int result;

	if (!dump)
		return (int)0x804c0000;
	for (index = 0; index < 8; index++) {
		dump->contexts[index].processUid = -1;
		memset(dump->contexts[index].processName, 0,
				sizeof(dump->contexts[index].processName));
	}
	for (core = 0; core < 5; core++)
		for (bank = 0; bank < 8; bank++)
			dump->contextIndices[core][bank] = 0xff;
	for (core = 0; core < 5; core++) {
		for (bank = 0; bank < 8; bank++) {
			processUid = (unsigned)-1;
			if (core == 4)
				result = GetRegisterDumpMasterRegister(dump, &value,
						masterDirectoryRegisters[bank]);
			else
				result = GetRegisterDumpCoreRegister(dump, &value,
						coreDirectoryRegisters[bank], core);
			if (!result) {
				if (SceGpuEs4ForDriver_7AA79658(value, &processUid))
					processUid = (unsigned)-1;
				else if (!processUid)
					processUid = 0x10005;
			}
			if (processUid != (unsigned)-1) {
				index = 0;
				while ((unsigned)dump->contexts[index].processUid != processUid) {
					if (dump->contexts[index].processUid == -1) {
						dump->contexts[index].processUid = processUid;
						break;
					}
					if (++index == 8) {
						index = 0xff;
						break;
					}
				}
				dump->contextIndices[core][bank] = index;
			}
		}
	}
	for (index = 0; index < 8; index++) {
		if (dump->contexts[index].processUid != -1) {
			name = SceSysmemForDriver_E655852F(dump->contexts[index].processUid);
			if (name) {
				strncpy(dump->contexts[index].processName, name, 19);
				dump->contexts[index].processName[19] = 0;
			}
		}
	}
	dump->flags |= 8;
	return 0;
}
