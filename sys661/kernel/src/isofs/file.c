#include "isofs_int.h"

IsofsFile *sceIsofsFileAllocNfile(IsofsUnit *unit, SceIoIob *iob, IsoDirectory *isoDir, s32 *outRet) {
    if (unit == NULL || iob == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    IsofsFile *file = isofs_alloc_fd(unit, isoDir, outRet);
    if (file == NULL) {
        isofsClearFile(file);
        return NULL;
    }
    isofs_init_fd(file, isoDir, iob, outRet);
    s32 intr = sceKernelCpuSuspendIntr();
    if (unit->openState == 2) {
        sceKernelCpuResumeIntr(intr);
        isofsClearFile(file);
        return NULL;
    }
    __builtin_memcpy(file->umdId, unit->umdId, UMD_ID_SIZE);
    file->flags = 1;
    sceKernelCpuResumeIntr(intr);
    return file;
}

// 00000924
IsofsFile *sceIsofsFileAllocVfile(IsofsUnit *unit, SceIoIob *iob, u32 lbn, u32 size, s32 *outRet) {
    if (unit == NULL || iob == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    IsofsFile *file = isofs_alloc_vfd(unit, lbn, outRet);
    if (file == NULL) {
        isofsClearFile(file);
        return NULL;
    }
    isofs_init_vfd(file, iob, lbn, size, outRet);
    s32 intr = sceKernelCpuSuspendIntr();
    if (unit->openState == 2) {
        sceKernelCpuResumeIntr(intr);
        isofsClearFile(file);
        return NULL;
    }
    __builtin_memcpy(file->umdId, unit->umdId, UMD_ID_SIZE);
    file->flags = 1;
    sceKernelCpuResumeIntr(intr);
    return file;
}

// 00000a58
IsofsFile *isofs_init_fd(IsofsFile *file, IsoDirectory *isoDir, SceIoIob *iob, s32 *outRet) {
    if (iob == NULL || file == NULL || isoDir == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    *outRet = SCE_ERROR_OK;
    file->fpos = 0;
    file->openFlags = 2;
    file->iob = iob;
    file->usedFlag = 1;
    file->readFlags = 0;
    file->lbn = isoDir->lbn;
    file->size = isoDir->size;
    u16 year = isoDir->dateTime.yearsSince1900 + 1900;
    file->dateTime.year = year;
    file->modifyDateTime.year = year;
    file->dateTime.month = isoDir->dateTime.month;
    file->modifyDateTime.month = isoDir->dateTime.month;
    file->dateTime.day = isoDir->dateTime.day;
    file->modifyDateTime.day = isoDir->dateTime.day;
    file->dateTime.hour = isoDir->dateTime.hour;
    file->modifyDateTime.hour = isoDir->dateTime.hour;
    file->dateTime.minute = isoDir->dateTime.minute;
    file->modifyDateTime.minute = isoDir->dateTime.minute;
    file->dateTime.second = isoDir->dateTime.second;
    file->modifyDateTime.second = isoDir->dateTime.second;
    file->dateTime.microsecond = 0;
    file->modifyDateTime.microsecond = 0;
    return file;
}

// 00000b74
IsofsFile *isofs_init_vfd(IsofsFile *file, SceIoIob *iob, u32 lbn, u32 size, s32 *outRet) {
    if (iob == NULL || file == NULL) {
        *outRet = SCE_ERROR_ERRNO_INVALID_ARGUMENT;
        return NULL;
    }
    *outRet = SCE_ERROR_OK;
    file->iob = iob;
    file->usedFlag = 1;
    file->fpos = 0;
    file->openFlags = 3;
    file->lbn = lbn;
    file->readFlags = 0;
    file->size = size;
    file->dateTime.year = 0;
    file->modifyDateTime.year = 0;
    file->dateTime.month = 0;
    file->modifyDateTime.month = 0;
    file->dateTime.day = 0;
    file->modifyDateTime.day = 0;
    file->dateTime.hour = 0;
    file->modifyDateTime.hour = 0;
    file->dateTime.minute = 0;
    file->modifyDateTime.minute = 0;
    file->dateTime.second = 0;
    file->modifyDateTime.second = 0;
    file->dateTime.microsecond = 0;
    file->modifyDateTime.microsecond = 0;
    return file;
}

// 00000c20
s32 isofsClearFile(IsofsFile *file) {
    if (file == NULL || file->iob == NULL || file->usedFlag <= 0) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    file->size = 0;
    file->fpos = 0;
    file->iob = NULL;
    file->readFlags = 0;
    file->lbn = 0;
    file->flags = 0;
    file->openFlags = 0;
    file->usedFlag = 0;
    file->dateTime.year = 0;
    file->modifyDateTime.year = 0;
    file->dateTime.month = 0;
    file->modifyDateTime.month = 0;
    file->dateTime.day = 0;
    file->modifyDateTime.day = 0;
    file->dateTime.hour = 0;
    file->modifyDateTime.hour = 0;
    file->dateTime.minute = 0;
    file->modifyDateTime.minute = 0;
    file->dateTime.second = 0;
    file->modifyDateTime.second = 0;
    file->dateTime.microsecond = 0;
    file->modifyDateTime.microsecond = 0;
    file->cacheIndex = -1;
    return SCE_ERROR_OK;
}

// 00000cc0
s32 sceIsofsFileTerm(IsofsFile *files) {
    // Original binary dereferences files->fileDesc before checking files == NULL.
    IsofsFd *fileDesc = files->fileDesc;
    if (files == NULL || fileDesc == NULL) {
        return SCE_ERROR_ERRNO_INVALID_ARGUMENT;
    }
    return sceKernelFreeHeapMemory(fileDesc->heapId, fileDesc->files);
}

// Not in module. Inlined into isofs_alloc_fd and isofs_alloc_vfd.
static inline __attribute__((always_inline)) IsofsFile *_isofsFindUnusedFile(IsofsUnit *unit, u32 lbn, s32 *outRet) {
    *outRet = SCE_ERROR_OK;
    if (unit == NULL) {
        *outRet = SCE_ERROR_ERRNO_DEVICE_NOT_FOUND;
        return NULL;
    }
    if (unit->fdwCount == 1 && unit->totalSectors < lbn) {
        IsofsFd fileDesc = unit->fdw[1].fileDesc;
        u16 filesCount = fileDesc.filesCount;
        for (u16 i = 0; i < filesCount; i++) {
            IsofsFile *file = &fileDesc.files[i];
            if (file->iob == NULL && file->flags == 0) {
                return file;
            }
        }
        *outRet = SCE_ERROR_ERRNO_TOO_MANY_OPEN_SYSTEM_FILES;
        return NULL;
    }

    IsofsFd fileDesc = unit->fdw[0].fileDesc;
    u16 filesCount = fileDesc.filesCount;
    for (u16 i = 0; i < filesCount; i++) {
        IsofsFile *file = &fileDesc.files[i];
        if (file->iob == NULL && file->flags == 0) {
            return file;
        }
    }
    *outRet = SCE_ERROR_ERRNO_TOO_MANY_OPEN_SYSTEM_FILES;
    return NULL;
}

// 00000d04
IsofsFile *isofs_alloc_fd(IsofsUnit *unit, IsoDirectory *isoDir, s32 *outRet) {
    return _isofsFindUnusedFile(unit, isoDir->lbn, outRet);
}

// 00000e14
IsofsFile *isofs_alloc_vfd(IsofsUnit *unit, u32 lbn, s32 *outRet) {
    return _isofsFindUnusedFile(unit, lbn, outRet);
}

// 00000ef4
