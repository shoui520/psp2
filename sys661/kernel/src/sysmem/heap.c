#include <sysmem_kdebug.h>
#include <sysmem_kernel.h>
#include <sysmem_sysclib.h>

#include "intr.h"
#include "block.h"
#include "misc.h"
#include "partition.h"

#include "heap.h"

s32 heap_do_initialize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 heap_do_delete(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 heap_do_alloc(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 heap_do_free(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 heap_do_totalfreesize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);

s32 _TotalFreeSize(SceSysmemHeap *heap);
s32 _DeleteHeap(SceSysmemHeap *heap);

// 145A4
SceSysmemUidCB *g_HeapType;

SceUID sceKernelCreateHeap(SceUID mpid, SceSize size, int flag, const char *name)
{
    int oldIntr = suspendIntr();
    SceSysmemMemoryPartition *part = MpidToCB(mpid);
    SceSysmemUidCB *uid;
    if (part == NULL) {
        // 300C
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_ILLEGAL_PARTITION_ID;
    }
    s32 ret = sceKernelCreateUID(g_HeapType, name, ((s32)pspGetK1() >> 31) & 0xFF, &uid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    u32 alignedSize = (size + 7) & 0xFFFFFFF8;
    int realSize = 0;
    if ((flag & 1) != 0)
        realSize = alignedSize;
    SceSysmemHeap *heap = UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap);
    SceSysmemPartitionInfo partInfo;
    heap->size = realSize | ((flag & 2) != 0);
    heap->partId = mpid;
    partInfo.size = 16;
    sceKernelQueryMemoryPartitionInfo(mpid, &partInfo);
    heap->partAddr = partInfo.startAddr;
    heap->partSize = partInfo.memSize;
    SceSysmemHeapBlock *block = _AllocPartitionMemory(part, (flag & 2) != 0, alignedSize, 0);
    if (block == NULL) {
        sceKernelDeleteUID(uid->uid ^ gSysmemRandomValue);
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_FAILED_ALLOC_HEAPBLOCK;
    }
    block->next = block;
    block->prev = block;
    initheap((SceSysmemLowheap*)(block + 1), alignedSize - 8);
    heap->firstBlock = block;
    resumeIntr(oldIntr);
    return uid->uid ^ gSysmemRandomValue;
}

void *_AllocHeapMemory(SceSysmemHeap *heap, u32 size, u32 align)
{
    if (size > 0x20000000)
        return NULL;
    if (align != 0 && ((align & 3) != 0 || align > 0x80 || ((align - 1) & align) != 0))
        return NULL;
    SceSysmemHeapBlock *block = heap->firstBlock->next;
    for (;;) {
        if ((u32)block - heap->partAddr >= heap->partSize) {
            if (sceKernelIsToolMode() != 0) {
                Kprintf("Heap memory is in illegal memory partition\n");
                pspBreak(0);
            }
            for (;;)
                ;
        }
        void *ret = hmalloc(heap, (SceSysmemLowheap*)(block + 1), size, align);
        if (ret != NULL)
            return ret;
        if (block == heap->firstBlock)
            break;
        block = block->next;
    }
    if (heap->size < 4)
        return NULL;
    u32 newSize = heap->size & 0xFFFFFFFE;
    if (align == 0) {
        if (newSize - 40 < size) {
            size = UPALIGN8(size);
            newSize = size + 40;
        }
    } else {
        size = UPALIGN8(size);
        u32 shift = align - (32 % align);
        if ((32 % align) == 0)
            shift = 0;
        if (newSize - 40 < size + shift)
            newSize = size + shift + 40;
    }
    block = _AllocPartitionMemory(MpidToCB(heap->partId), 0, newSize, 0);
    if (block == NULL)
        return NULL;
    block->next = block;
    block->prev = block;
    initheap((SceSysmemLowheap*)(block + 1), newSize - 8);
    SceSysmemHeapBlock *headNext = heap->firstBlock->next;
    block->next = headNext;
    block->prev = headNext->prev;
    headNext->prev = block;
    block->prev->next = block;
    return hmalloc(heap, (SceSysmemLowheap*)(block + 1), size, align);
}

s32 _FreeHeapMemory(SceSysmemHeap *heap, void *addr)
{
    SceSysmemHeapBlock *block = heap->firstBlock->next;
    for (;;) {
        if ((u32)block - heap->partAddr >= heap->partSize) {
            if (sceKernelIsToolMode() != 0) {
                Kprintf("Heap memory is in illegal memory partition\n");
                pspBreak(0);
            }
            for (;;)
                ;
        }
        SceSysmemLowheap *lowh = (SceSysmemLowheap*)(block + 1);
        if (hfree(heap, lowh, addr) == 0) {
            if (block != heap->firstBlock && checkheapnouse(lowh) != 0) {
                lowh->addr = 0;
                block->next->prev = block->prev;
                block->prev->next = block->next;
                _FreePartitionMemory(block);
            }
            return 0;
        }
        if (block == heap->firstBlock)
            return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
        block = block->next;
    }
}

int sceKernelQueryHeapInfo(SceUID id, SceSysmemHeapInfo *info)
{
    if (info->size < 64)
        return SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    int oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    int ret = sceKernelGetUIDcontrolBlockWithType(id, g_HeapType, &uid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    SceSysmemHeap *heap = UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap);
    int i;
    for (i = 0; i < 8; i++)
        ((int*)info->name)[i] = 0;
    if (uid->name != NULL) {
        strncpy(info->name, uid->name, 31);
    }
    info->perm = uid->attr;
    info->heapSize = heap->size & 0xFFFFFFFE;
    if (heap->size >= 4) {
        if ((heap->size & 1) == 0)
            info->attr = 1;
        else
            info->attr = 3;
    } else
        info->attr = 0;
    int totalSize = 0;
    int totalFreeSize = 0;
    SceSysmemHeapBlock *curBlock = heap->firstBlock->next;
    int maxFreeSize = 0;
    int numHeaps = 0;
    for (;;)
    {
        if ((u32)curBlock - heap->partAddr >= heap->partSize)
            break;
        SceSysmemLowheap *cur = (SceSysmemLowheap*)(curBlock + 1);
        numHeaps++;
        totalSize += ((cur->size - 16) & 0xFFFFFFF8) - 8;
        totalFreeSize += htotalfreesize(cur);
        maxFreeSize = pspMax(maxFreeSize, hmaxfreesize(heap, cur));
        if (curBlock == heap->firstBlock)
            break;
        curBlock = curBlock->next;
    }
    info->totalsize = totalSize;
    u32 size = 64;
    info->totalfreesize = totalFreeSize;
    info->maxfreesize = maxFreeSize;
    info->numheaps = numHeaps;
    curBlock = heap->firstBlock->next;
    SceSysmemHeapBlock **heaps = (SceSysmemHeapBlock **)&info->heaps;
    if (info->size >= 68)
    {
        do
        {
            if ((u32)curBlock - heap->partAddr >= heap->partSize)
                break;
            size += 4;
            *(heaps++) = curBlock;
            if (curBlock == heap->firstBlock)
                break;
            curBlock = curBlock->next;
        } while (size + 4 <= info->size);
    }
    info->size = size;
    resumeIntr(oldIntr);
    return 0;
}

s32 sceKernelQueryLowheapInfo(SceSysmemHeapBlock *block, SceSysmemLowheapInfo *info)
{
    u32 maxInfoSize = info->size;
    if (maxInfoSize < 24)
        return SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    SceSysmemLowheap *lowh = (SceSysmemLowheap *)(block + 1);
    if (lowh->addr != (u32)lowh - 1)
        return SCE_ERROR_KERNEL_ILLEGAL_CHUNK_ID;
    SceSysmemLowheapBlock *cur = (SceSysmemLowheapBlock *)(lowh + 1);
    u32 infoSize = 24;
    SceSysmemLowheapBlock *last = (void*)lowh + lowh->size - 8;
    u32 blockCount = 0;
    u32 usedCount = 0;
    u32 freeCount = 0;
    u32 maxFreeCount = 0;
    SceSysmemLowheapInfoBlock *infoBlock = info->infoBlocks;
    SceSysmemLowheapBlock *first = cur;
    // 35B0
    while (cur != last) {
        if (cur < first || cur > last)
            break;
        if (infoSize + 8 >= maxInfoSize)
            infoBlock = NULL;
        SceSysmemLowheapBlock *block;
        if (cur->next == (void *)lowh) { // used
            // 3670
            block = (SceSysmemLowheapBlock *)((u32)cur | 0x1);
            usedCount += cur->count - 1;
        } else {
            block = cur;
            freeCount += cur->count - 1;
            if (cur->count - 1 > maxFreeCount)
                maxFreeCount = cur->count - 1;
        }
        // 35EC
        blockCount++;
        if (infoBlock != NULL) {
            infoSize += 8;
            infoBlock->block = block;
            infoBlock->offset = (cur->count - 1) * 8;
            infoBlock++;
        }
        // 3618
        cur += cur->count;
    }
    // 3628
    info->size = infoSize;
    info->heapSize = ((lowh->size - 16) & 0xFFFFFFF8) - 8;
    info->usedSize = usedCount << 3;
    info->freeSize = freeCount << 3;
    info->maxFreeSize = maxFreeCount << 3;
    info->blockCount = blockCount;
    return blockCount;
}

// 13480
SceSysmemUidLookupFunc HeapFuncs[] =
{
    { 0xD310D2D9, heap_do_initialize },
    { 0x87089863, heap_do_delete },
    { 0x0DE3B1BD, heap_do_alloc },
    { 0xA9CE362D, heap_do_free },
    { 0x01DB36E1, heap_do_totalfreesize },
    { 0, NULL}
};

void HeapInit(void)
{
    sceKernelCreateUIDtype("SceSysmemHeap", sizeof(SceSysmemHeap), HeapFuncs, NULL, &g_HeapType);
}

int _CreateHeap(void *partition, int size, int attr, SceSysmemHeapBlock **out)
{
    SceSysmemHeapBlock *ptr;
    if ((attr & 2) == 0)
        ptr = _AllocPartitionMemory(partition, 0, size, 0);
    else
        ptr = _AllocPartitionMemory(partition, 1, size, 0);
    // 36F0
    if (ptr == NULL)
        return SCE_ERROR_KERNEL_FAILED_ALLOC_HEAPBLOCK;
    *out = ptr;
    ptr->next = ptr;
    ptr->prev = ptr;
    initheap((SceSysmemLowheap*)(ptr + 1), size - 8);
    return 0;
}

int sceKernelDeleteHeap(SceUID id)
{
    int oldIntr = suspendIntr();
    int ret = sceKernelDeleteUID(id);
    resumeIntr(oldIntr);
    return ret;
}

void *sceKernelAllocHeapMemoryWithOption(SceUID id, int size, SceSysmemHeapAllocOption *opt)
{
    int oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_HeapType, &uid) != 0) {
        // 382C
        resumeIntr(oldIntr);
        return 0;
    }
    // 37F4
    void *ret = _AllocHeapMemory(UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap), size, (opt != NULL) ? opt->align : 0);
    resumeIntr(oldIntr);
    return ret;
}

void *sceKernelAllocHeapMemory(SceUID id, int size)
{
    int oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_HeapType, &uid) != 0) {
        resumeIntr(oldIntr);
        return 0;
    }
    // 38B0
    void *ret = _AllocHeapMemory(UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap), size, 0);
    resumeIntr(oldIntr);
    return ret;
}

s32 sceKernelFreeHeapMemory(SceUID id, void *addr)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_HeapType, &uid);
    if (ret == 0)
        ret = _FreeHeapMemory(UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap), addr);
    // 3948
    resumeIntr(oldIntr);
    return ret;
}

s32 _CheckHeapMemory(SceSysmemHeap *heap, void *addr)
{
    if ((u32)addr - heap->partAddr >= heap->partSize) {
        if (!sceKernelIsToolMode()) {
            for (;;)
                ;
        }
        Kprintf("Heap memory is in illegal memory partition\n");
        pspBreak(0);
    }
    SceSysmemHeapBlock *block = heap->firstBlock->next;
    for (;;) {
        if ((u32)block - heap->partAddr >= heap->partSize) {
            if (sceKernelIsToolMode() != 0) {
                Kprintf("Heap memory is in illegal memory partition\n");
                pspBreak(0);
            }
            for (;;)
                ;
        }
        if (hcheckblock((SceSysmemLowheap *)(block + 1), addr) == 0)
            return 0;
        if (block == heap->firstBlock) {
            if (sceKernelIsToolMode() != 0)
                pspBreak(0);
            for (;;)
                ;
        }
        block = block->next;
    }
}

s32 sceKernelHeapTotalFreeSize(SceUID id)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_HeapType, &uid);
    if (ret == 0)
        ret = _TotalFreeSize(UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap));
    resumeIntr(oldIntr);
    return ret;
}

s32 heap_do_initialize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap)
{
    sceKernelCallUIDObjCommonFunction(uid, uidWithFunc, funcId, ap);
    SceSysmemHeap *heap = UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap);
    heap->size = 0;
    heap->partId = 0;
    heap->firstBlock = NULL;
    return uid->uid ^ gSysmemRandomValue;
}

s32 heap_do_delete(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap)
{
    SceSysmemHeap *heap = UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap);
    if (heap->firstBlock != NULL)
        _DeleteHeap(heap);
    sceKernelCallUIDObjCommonFunction(uid, uidWithFunc, funcId, ap);
    return uid->uid ^ gSysmemRandomValue;
}

s32 heap_do_alloc(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    u32 size = va_arg(ap, u32);
    SceSysmemHeapAllocOption *opt = va_arg(ap, SceSysmemHeapAllocOption *);
    void **retPtr = va_arg(ap, void**);
    SceSysmemHeap *heap = UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap);
    *retPtr = _AllocHeapMemory(heap, size, (opt != NULL) ? opt->align : 0);
    return uid->uid ^ gSysmemRandomValue;
}

s32 heap_do_free(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    s32 ret = _FreeHeapMemory(UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap), va_arg(ap, void*));
    if (ret != 0)
        return ret;
    return uid->uid ^ gSysmemRandomValue;
}

s32 heap_do_totalfreesize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    s32 *outSize = va_arg(ap, s32 *);
    *outSize = _TotalFreeSize(UID_CB_TO_DATA(uid, g_HeapType, SceSysmemHeap));
    return uid->uid ^ gSysmemRandomValue;
}

s32 _DeleteHeap(SceSysmemHeap *heap)
{
    SceSysmemHeapBlock *firstBlock = heap->firstBlock;
    SceSysmemHeapBlock *cur = firstBlock->next;
    if (cur != firstBlock) {
        do {
            if ((u32)cur - heap->partAddr >= heap->partSize) {
                if (sceKernelIsToolMode() != 0) {
                    Kprintf("Heap memory is in illegal memory partition\n");
                    pspBreak(0);
                }
                for (;;)
                    ;
            }
            SceSysmemHeapBlock *next = cur->next;
            ((SceSysmemLowheap*)(cur + 1))->addr = 0;
            _FreePartitionMemory(cur);
            cur = next;
        } while (cur != firstBlock);
    }
    ((SceSysmemLowheap*)(firstBlock + 1))->addr = 0;
    return _FreePartitionMemory(firstBlock);
}

s32 _TotalFreeSize(SceSysmemHeap *heap)
{
    s32 size = 0;
    SceSysmemHeapBlock *cur = heap->firstBlock->next;
    for (;;) {
        if ((u32)cur - heap->partAddr >= heap->partSize) {
            if (sceKernelIsToolMode() != 0) {
                Kprintf("Heap memory is in illegal memory partition\n");
                pspBreak(0);
            }
            for (;;)
                ;
        }
        size += htotalfreesize((SceSysmemLowheap*)(cur + 1));
        if (cur == heap->firstBlock)
            return size;
        cur = cur->next;
    }
}

