#include "isofs_int.h"

// 00000000
s32 sceIsofsLbInit() {
    IsofsMgr *mgr = isofsGetMgr();
    sceKernelWaitSema(mgr->semaId, 1, NULL);
    g_isofsUnits = (IsofsUnit *) sceKernelAllocHeapMemory(mgr->heapId, mgr->maxUnits * sizeof(IsofsUnit));
    if (g_isofsUnits == NULL) {
        sceKernelSignalSema(mgr->semaId, 1);
        return SCE_ERROR_ERRNO_NO_MEMORY;
    }
    s32 ret = SCE_ERROR_OK;
    for (s32 i = 0; i < mgr->maxUnits; ++i) {
        IsofsUnit *unit = &g_isofsUnits[i];
        unit->fdwCount = i; // weird
        unit->sectorSize = 0;
        unit->pathTableSize = 0;
        unit->rootDirLbn = 0;
        unit->rootDirSize = 0;
        unit->nextLbn = 0;
        unit->lastLbn = 0;
        unit->remainingLbns = 0;
        unit->pathTableLbn = 0;
        unit->totalSectors = 0;
        unit->cachedCurrentLbn = 0;
        unit->activeFlag = 0;
        unit->heapId = mgr->heapId;
        unit->cacheSlot = -1;
        unit->unitNum = -1;
        unit->mountFlags = 0;
        unit->defaultFlag = 1;
        unit->pathTableEntryCount = 0;
        unit->mountState = 0;
        unit->fdw = NULL;
        unit->paths = NULL;
        unit->pathTable = NULL;
        unit->primaryVolumeDescriptor = NULL;
        unit->openState = 0;
        _setUmdId(unit->umdId, 0);
        ret = isofsInitUnitFdw(unit, mgr->fdwCount, mgr->maxFiles / mgr->fdwCount);
        if (ret != 0) {
            break;
        }
    }
    sceKernelSignalSema(mgr->semaId, 1);
    return ret;
}

// 0000019c
s32 sceIsofsLbTerm() {
    IsofsMgr *mgr = isofsGetMgr();
    sceKernelWaitSema(mgr->semaId, 1, NULL);
    for (s32 i = 0; i < mgr->maxUnits; ++i) {
        IsofsUnit *unit = &g_isofsUnits[i];
        isofsFreeUnitFdw(unit);
        sceKernelDeleteSema(unit->semaId);
    }
    sceKernelFreeHeapMemory(mgr->heapId, g_isofsUnits);
    g_isofsUnits = NULL;
    sceKernelSignalSema(mgr->semaId, 1);
    return SCE_ERROR_OK;
}

// 00000254
IsofsUnit *isofsGetUnit(s32 unitNum) {
    IsofsMgr *mgr = isofsGetMgr();
    if (((u32) unitNum >> 0x1f | (mgr == NULL)) != 0 || unitNum >= mgr->maxUnits) {
        return NULL;
    }
    return &g_isofsUnits[unitNum];
}

// 000002bc
s32 isofsInitUnitFdw(IsofsUnit *unit, u32 fdwCount, u16 filesCount) {
    unit->fdw = (IsofsFdw *) sceKernelAllocHeapMemory(unit->heapId, fdwCount * sizeof(IsofsFdw));
    if (unit->fdw == NULL) {
        return SCE_ERROR_ERRNO_NO_MEMORY;
    }
    memset(unit->fdw, 0, fdwCount * sizeof(IsofsFdw));
    for (u32 i = 0; i < fdwCount; i++) {
        s32 ret = sceIsofsSbInit(&unit->fdw[i], i, filesCount);
        if (ret != 0) {
            sceKernelFreeHeapMemory(unit->heapId, unit->fdw);
            return ret;
        }
        unit->fdw[i].idx = i;
        unit->fdw[i].fileDesc.unit = unit;
        unit->fdw[i].reserved = 0;
    }
    unit->fdwCount = (u8) fdwCount;
    return SCE_ERROR_OK;
}

// 000003c4
void isofsFreeUnitFdw(IsofsUnit *unit) {
    for (u8 i = 0; i < unit->fdwCount; i++) {
        isofsFreeFdFiles(&unit->fdw[i].fileDesc);
    }
    sceKernelFreeHeapMemory(unit->heapId, unit->fdw);
}

// 00000434
s32 isofsClearUnitFilesFdw(IsofsUnit *unit, s32 fdwCount) {
    isofsClearFdwFiles(unit->fdw, fdwCount);
    return SCE_ERROR_OK;
}

// 00000454
