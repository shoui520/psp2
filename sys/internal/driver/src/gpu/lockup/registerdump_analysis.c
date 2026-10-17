/* Sony register-dump analysis; hardware field names come from pristine IMG. */
#include "../../gpu_es4/srvkm/registerdump_sce.h"
#include <sysmem/debug.h>
#include "../../../../../../contrib/img-sgx-ddk-1.7.862890/services4/srvkm/hwdefs/sgx543_v1.164defs.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "../../../../driver/src/gpu/lockup/registerdump_analysis.c"

int GetRegisterDumpCoreRegister(const SceGpuRegisterDump *dump,
		unsigned *value, unsigned address, unsigned core);
int GetRegisterDumpMasterRegister(const SceGpuRegisterDump *dump,
		unsigned *value, unsigned address);
int SceGpuEs4ForDriver_7AA79658(unsigned physical, unsigned *pid);
int SceGpuRegisterDumpCoreExtendedPerfRegisters(const SceGpuRegisterDump *dump,
		unsigned *value, unsigned selector, unsigned core);

/* SCE fault-request classification, identical in both TOOL originals. */
static const unsigned char faultRequestKinds[8] = { 2, 2, 5, 3, 3, 3, 3, 3 };

static const unsigned analysisCoreDirectoryRegisters[8] = {
	EUR_CR_BIF_DIR_LIST_BASE0, EUR_CR_BIF_DIR_LIST_BASE1,
	EUR_CR_BIF_DIR_LIST_BASE2, EUR_CR_BIF_DIR_LIST_BASE3,
	EUR_CR_BIF_DIR_LIST_BASE4, EUR_CR_BIF_DIR_LIST_BASE5,
	EUR_CR_BIF_DIR_LIST_BASE6, EUR_CR_BIF_DIR_LIST_BASE7,
};

static const unsigned analysisMasterDirectoryRegisters[8] = {
	EUR_CR_BIF_DIR_LIST_BASE0 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE1 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE2 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE3 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE4 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE5 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE6 + (1U << 14),
	EUR_CR_BIF_DIR_LIST_BASE7 + (1U << 14),
};

int _sceGpuRegisterDumpAnalysisGetCid(const SceGpuRegisterDump *dump,
		unsigned *cid, int requestor)
{
	/* SCE inlined _sceGpuRegisterDumpAnalysisGetBank mapping. */
	const unsigned banks[4] = { 0, 0xdeadc0de, 0xdeadc0de, 1 };
	const int bankErrors[4] = { 0, (int)0x804c000c, (int)0x804c000c, 0 };
	unsigned selection, bank = 0, registerOffset, coreValue, masterValue;
	int result;

	if (!cid)
		return (int)0x804c0000;
	*cid = 7;
	if (!dump)
		return (int)0x804c0000;
	result = GetRegisterDumpCoreRegister(dump, &selection, EUR_CR_BIF_BANK_SET, 0);
	if (result) {
		result = (int)0x804c0000;
		goto bank_error;
	}
	switch (requestor) {
	case 2: /* TA */
		selection = (selection & EUR_CR_BIF_BANK_SET_SELECT_TA_MASK) >>
			EUR_CR_BIF_BANK_SET_SELECT_TA_SHIFT;
		goto mapped_bank;
	case 3: /* 3D */
		selection = (selection & EUR_CR_BIF_BANK_SET_SELECT_3D_MASK) >>
			EUR_CR_BIF_BANK_SET_SELECT_3D_SHIFT;
mapped_bank:
		bank = banks[selection];
		result = bankErrors[selection];
		if (result < 0)
			goto bank_error;
		break;
	case 4: /* EDM */
		bank = (selection & EUR_CR_BIF_BANK_SET_SELECT_EDM_MASK) >>
			EUR_CR_BIF_BANK_SET_SELECT_EDM_SHIFT;
		break;
	case 5: /* PTLA uses bank zero. */
		break;
	default:
		result = (int)0x804c0000;
		goto bank_error;
	}
	if (bank == 0)
		registerOffset = EUR_CR_BIF_BANK0;
	else if (bank == 1)
		registerOffset = EUR_CR_BIF_BANK1;
	else
		return (int)0x804c0000;
	result = GetRegisterDumpCoreRegister(dump, &coreValue, registerOffset, 0);
	if (result)
		return result;
	/* Pristine SGX MP master register bank has index one and stride 1 << 14. */
	result = GetRegisterDumpMasterRegister(dump, &masterValue, registerOffset + (1U << 14));
	if (result)
		return result;
	switch (requestor) {
	case 2:
		*cid = (coreValue & EUR_CR_BIF_BANK0_INDEX_TA_MASK) >> EUR_CR_BIF_BANK0_INDEX_TA_SHIFT;
		break;
	case 3:
		*cid = (coreValue & EUR_CR_BIF_BANK0_INDEX_3D_MASK) >> EUR_CR_BIF_BANK0_INDEX_3D_SHIFT;
		break;
	case 4:
		*cid = (coreValue & EUR_CR_BIF_BANK0_INDEX_EDM_MASK) >> EUR_CR_BIF_BANK0_INDEX_EDM_SHIFT;
		break;
	case 5:
		*cid = (masterValue & EUR_CR_BIF_BANK0_INDEX_PTLA_MASK) >> EUR_CR_BIF_BANK0_INDEX_PTLA_SHIFT;
		break;
	default:
		return (int)0x804c0000;
	}
	return 0;

bank_error:
	SCE_DBG_PRINTF(6, 261, "_sceGpuRegisterDumpAnalysisGetBank() returned 0x%08x\n", result);
	return result;
}

int _sceGpuRegisterDumpGetContextPid(const SceGpuRegisterDump *dump,
		unsigned device, unsigned context)
{
	unsigned value, processUid = (unsigned)-1;
	unsigned core;
	int result;

	if (!dump)
		return processUid;
	if (context > 7)
		return processUid;
	if (device > 4)
		return processUid;
	switch (device) {
	case 1:
		core = 0;
		break;
	case 2:
		core = 1;
		break;
	case 3:
		core = 2;
		break;
	case 4:
		core = 3;
		break;
	default:
		result = GetRegisterDumpMasterRegister(dump, &value,
				analysisMasterDirectoryRegisters[context]);
		goto lookup_pid;
	}
	result = GetRegisterDumpCoreRegister(dump, &value,
			analysisCoreDirectoryRegisters[context], core);
lookup_pid:
	if (!result) {
		if (SceGpuEs4ForDriver_7AA79658(value, &processUid)) {
			SCE_DBG_PRINTF(6, 382, "MMU_FindPidFromPD failed \n");
			processUid = (unsigned)-1;
		} else if (!processUid) {
			processUid = 0x10005;
		}
	}
	return processUid;
}

int sceGpuRegisterDumpAnalysis(const SceGpuRegisterDump *dump,
		unsigned *analysis, int *pid, unsigned *detail)
{
	unsigned faultContexts[5] = { -1, -1, -1, -1, -1 };
	unsigned faultKinds[5] = { 0, 0, 0, 0, 0 };
	unsigned coreDevices[4] = { 1, 2, 3, 4 };
	unsigned edmCid, taCid, renderCid, ptlaCid, value;
	unsigned core, device, selectedDevice, faultCount = 0, request;
	unsigned faultCid, overview = 0, otherCid, activityMask, registerOffset;
	int allSame = 1, faultPid, otherPid, result;

	if (!dump)
		return (int)0x804c0000;
	if (!analysis)
		return (int)0x804c0000;
	if (!pid)
		return (int)0x804c0000;
	*analysis = 0;
	*pid = -1;
	if (!GetRegisterDumpMasterRegister(dump, &value, EUR_CR_BIF_INT_STAT + (1U << 14))) {
		if (value & EUR_CR_BIF_INT_STAT_FAULT_TYPE_MASK) {
			if (((value >> EUR_CR_BIF_INT_STAT_FAULT_TYPE_SHIFT) & 3) == 3)
				faultKinds[0] = 4;
			else {
				request = (value & EUR_CR_BIF_INT_STAT_FAULT_REQ_MASK) - 2;
				faultKinds[0] = request > 7 ? 1 : faultRequestKinds[request];
			}
			faultCount = 1;
		}
		if (!GetRegisterDumpMasterRegister(dump, &value, EUR_CR_BIF_FAULT + (1U << 14)))
			faultContexts[0] = value & EUR_CR_BIF_FAULT_CID_MASK;
	}
	for (core = 0; core < 4; core++) {
		device = coreDevices[core];
		if (!GetRegisterDumpCoreRegister(dump, &value, EUR_CR_BIF_INT_STAT, core) &&
				(value & EUR_CR_BIF_INT_STAT_FAULT_TYPE_MASK)) {
			faultCount++;
			if (((value >> EUR_CR_BIF_INT_STAT_FAULT_TYPE_SHIFT) & 3) == 3)
				faultKinds[device] = 4;
			else {
				request = (value & EUR_CR_BIF_INT_STAT_FAULT_REQ_MASK) - 2;
				faultKinds[device] = request > 7 ? 1 : faultRequestKinds[request];
			}
		}
		if (!GetRegisterDumpCoreRegister(dump, &value, EUR_CR_BIF_FAULT, core))
			faultContexts[device] = value & EUR_CR_BIF_FAULT_CID_MASK;
	}
	if (!faultCount) {
		ptlaCid = 0;
		value = 0;
		*detail = (unsigned)-1;
		for (core = 0; core < 4; core++) {
			if (!SceGpuRegisterDumpCoreExtendedPerfRegisters(dump, &ptlaCid, 0x25, core) &&
					(ptlaCid & 0x10)) {
				if (!SceGpuRegisterDumpCoreExtendedPerfRegisters(dump, &value, 0x26, core))
					*detail = value & 0x1fff;
				*analysis = 4;
				if (!GetRegisterDumpCoreRegister(dump, &value, EUR_CR_BIF_BANK_SET, core)) {
					registerOffset = ((value >> EUR_CR_BIF_BANK_SET_SELECT_TA_SHIFT) & 3) == 3 ?
						EUR_CR_BIF_BANK1 : EUR_CR_BIF_BANK0;
					if (!GetRegisterDumpCoreRegister(dump, &value, registerOffset, core))
						*pid = _sceGpuRegisterDumpGetContextPid(dump, coreDevices[core],
								(value & EUR_CR_BIF_BANK0_INDEX_TA_MASK) >> EUR_CR_BIF_BANK0_INDEX_TA_SHIFT);
				}
				break;
			}
		}
		return 0;
	}
	selectedDevice = (unsigned)-1;
	for (device = 0; device < 5; device++) {
		if (faultContexts[device] != (unsigned)-1 && faultKinds[device]) {
			if (selectedDevice == (unsigned)-1)
				selectedDevice = device;
			else if (faultContexts[device] != faultContexts[selectedDevice])
				allSame = 0;
		}
	}
	if (allSame) {
		if (_sceGpuRegisterDumpAnalysisGetCid(dump, &edmCid, 4) < 0 ||
				_sceGpuRegisterDumpAnalysisGetCid(dump, &taCid, 2) < 0 ||
				_sceGpuRegisterDumpAnalysisGetCid(dump, &renderCid, 3) < 0 ||
				_sceGpuRegisterDumpAnalysisGetCid(dump, &ptlaCid, 5) < 0) {
			SCE_DBG_PRINTF(0, 490, "_sceGpuRegisterDumpAnalysisGetCid() failed -> SCE_GPU_REGISTERDUMP_ANALYSIS_SYSTEM_UNRECOVERABLE\n");
			goto system_unrecoverable;
		}
		if (faultKinds[selectedDevice] == 4) {
			SCE_DBG_PRINTF(0, 507, "EDM page fault -> SCE_GPU_REGISTERDUMP_ANALYSIS_SYSTEM_UNRECOVERABLE\n");
			goto system_unrecoverable;
		}
		faultCid = faultContexts[selectedDevice];
		faultPid = _sceGpuRegisterDumpGetContextPid(dump, selectedDevice, faultCid);
		if (faultCid == taCid || faultCid == renderCid) {
			if (taCid == renderCid) {
				if (faultPid == -1 || faultPid == 0x10005) {
					SCE_DBG_PRINTF(0, 537, "Single process page fault but pid=0x%08x -> SCE_GPU_REGISTERDUMP_ANALYSIS_SYSTEM_UNRECOVERABLE\n", faultPid);
					goto system_unrecoverable;
				}
				SCE_DBG_PRINTF(0, 531, "Single process page fault with pid=0x%08x -> SCE_GPU_REGISTERDUMP_ANALYSIS_PROCESS_UNRECOVERABLE\n", faultPid);
				goto process_unrecoverable;
			}
			for (core = 0; core < 4; core++) {
				result = SceGpuRegisterDumpCoreExtendedPerfRegisters(dump, &value, 0, core);
				if (result < 0) {
					SCE_DBG_PRINTF(0, 554, "SceGpuRegisterDumpCoreExtendedPerfRegisters(PERF_OVERVIEW_0) returned 0x%08x\n", result);
					goto unhandled_fault;
				}
				overview |= value;
			}
			if (!result) {
				if (faultCid == taCid) {
					otherCid = renderCid;
					activityMask = 2;
				} else if (faultCid == renderCid) {
					otherCid = taCid;
					activityMask = 1;
				} else {
					goto unhandled_fault;
				}
				otherPid = _sceGpuRegisterDumpGetContextPid(dump, selectedDevice, otherCid);
				if (activityMask & overview) {
					SCE_DBG_PRINTF(0, 582, "Multi-process page fault on pid=0x%08x but pid=0x%08x stil active-> SCE_GPU_REGISTERDUMP_ANALYSIS_SYSTEM_UNRECOVERABLE\n", faultPid, otherPid);
					goto system_unrecoverable;
				}
				SCE_DBG_PRINTF(0, 588, "Multi-process TA page fault with pid=0x%08x and pid=0x%08x not active  -> SCE_GPU_REGISTERDUMP_ANALYSIS_PROCESS_UNRECOVERABLE\n", faultPid, otherPid);
				goto process_unrecoverable;
			}
		}
	}
unhandled_fault:
	SCE_DBG_PRINTF(0, 603, "Unhandled page fault type -> SCE_GPU_REGISTERDUMP_ANALYSIS_SYSTEM_UNRECOVERABLE\n\n");
system_unrecoverable:
	*analysis = 1;
	*pid = -1;
	return 0;
process_unrecoverable:
	*analysis = 2;
	*pid = faultPid;
	return 0;
}
