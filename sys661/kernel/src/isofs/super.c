#include "isofs_int.h"

s32 sceIsofsSbInit(IsofsFdw *fdw, u32 idx, u32 filesCount) {
    fdw->fileDesc.idx = idx;
    fdw->fileDesc.fdw = fdw;
    fdw->fileDesc.filesCount = (u16) filesCount;
    return sceIsofsSbTerm(&fdw->fileDesc, filesCount);
}

// 00000650
s32 isofsFreeFdFiles(IsofsFd *fileDesc) {
    s32 ret = sceIsofsFileTerm(fileDesc->files);
    fileDesc->files = NULL;
    return ret;
}

// 0000067c
void isofsClearFdwFiles(IsofsFdw *fdw, s32 fdwCount) {
    IsofsFile *files = fdw->fileDesc.files;
    for (int i = 0; i < fdwCount / 2; i++) {
        IsofsFile *file = &files[i];
        file->openFlags = 0;
        file->flags = 0;
        file->reservedByte = 0;
        file->fileDesc = &fdw->fileDesc;
        file->size = 0;
        file->fpos = 0;
        file->reservedHalf = 0;
        file->cacheIndex = -1;
        file->readFlags = 0;
        file->lbn = 0;
        file->reservedLbnPad[0] = 0;
        file->reservedLbnPad[1] = 0;
        file->iob = NULL;
    }
}

// 00000718
s32 sceIsofsSbTerm(IsofsFd *fileDesc, u32 filesCount) {
    IsofsMgr *mgr = isofsGetMgr();

    fileDesc->heapId = mgr->heapId;
    fileDesc->files = (IsofsFile *) sceKernelAllocHeapMemory(mgr->heapId, filesCount * sizeof(IsofsFile));
    if (fileDesc->files == NULL) {
        return SCE_ERROR_ERRNO_NO_MEMORY;
    }
    for (u32 i = 0; i < filesCount; i++) {
        IsofsFile *file = &fileDesc->files[i];
        file->openFlags = 0;
        file->flags = 0;
        file->usedFlag = 0;
        file->reservedByte = 0;
        file->fileDesc = fileDesc;
        file->size = 0;
        file->fpos = 0;
        file->reservedHalf = 0;
        file->cacheIndex = -1;
        file->readFlags = 0;
        file->lbn = 0;
        file->reservedLbnPad[0] = 0;
        file->reservedLbnPad[1] = 0;
        file->iob = NULL;
    }
    return SCE_ERROR_OK;
}

// 000007fc
