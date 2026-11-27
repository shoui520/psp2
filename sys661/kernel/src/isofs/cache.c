#include "isofs_int.h"

s32 sceIsofsCacheCheckData() {
    IsofsMgr *mgr = isofsGetMgr();
    g_isofsCache = (IsofsCacheEntry *) sceKernelAllocHeapMemory(mgr->heapId,
                                                                sizeof(IsofsCacheEntry) * ISOFS_CACHE_ENTRIES);
    if (g_isofsCache == NULL) {
        return SCE_ERROR_ERRNO_NO_MEMORY;
    }
    int fplId = sceKernelCreateFpl("SceIsofsCacheData", 1, 0, 0x1000, 8, NULL);
    if (fplId < 0) {
        sceKernelFreeHeapMemory(mgr->heapId, g_isofsCache);
        return fplId;
    }
    mgr->fplId = fplId;
    for (u32 i = 0; i < ISOFS_CACHE_ENTRIES; i++) {
        IsofsCacheEntry *entry = &g_isofsCache[i];
        entry->used = 0;
        entry->headerWords[0] = 0;
        entry->headerWords[1] = 0;
        entry->headerWords[2] = 0;
        entry->headerWords[3] = 0;
        entry->headerWords[4] = 0;
        entry->headerWords[5] = 0;
        entry->headerWords[6] = 0;
        entry->devctlExt = 0;
        entry->padHalf = 0;
        entry->tailWords[0] = 0;
        entry->tailWords[1] = 0;
        entry->memory = NULL;
    }
    return SCE_ERROR_OK;
}

// 00000550
s32 sceIsofsCacheDeleteData() {
    IsofsMgr *mgr = isofsGetMgr();
    for (u32 i = 0; i < ISOFS_CACHE_ENTRIES; i++) {
        IsofsCacheEntry *entry = &g_isofsCache[i];
        if (entry->used == 1) {
            sceKernelFreeHeapMemory(mgr->fplId, entry->memory);
        }
    }
    return SCE_ERROR_OK;
}

// 000005dc
s32 isofsFreeCache() {
    IsofsMgr *mgr = isofsGetMgr();
    sceIsofsCacheDeleteData();
    sceKernelFreeHeapMemory(mgr->heapId, g_isofsCache);
    sceKernelDeleteFpl(mgr->fplId);
    return SCE_ERROR_OK;
}

// 00000620
