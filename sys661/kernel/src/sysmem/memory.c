#include <sysmem_kdebug.h>
#include <sysmem_kernel.h>
#include <sysmem_sysclib.h>

#include "intr.h"
#include "misc.h"
#include "sysmem.h"

#include "memory.h"

s32 block_do_initialize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_delete(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_delete_super(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_resize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_sizelock(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_querymeminfo(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_queryblkinfo(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);
s32 block_do_getheadaddr(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap);

// 13FE0
SceSysmemUidCB *g_MemBlockType;

s32 sceKernelResizeMemoryBlock(SceUID id, s32 leftShift, s32 rightShift)
{
    u32 rightSegs = 0;
    u32 leftSegs = 0;
    if (leftShift != 0)
        leftSegs = pspMax(leftShift, -leftShift) / 256;
    // 4DFC
    if (rightShift != 0)
        rightSegs = pspMax(rightShift, -rightShift) / 256;
    // 4E20
    if (leftSegs == 0 && rightSegs == 0)
        return 0;
    // 4E5C
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    // 4E90
    SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock);
    SceSysmemMemoryPartition *part = memBlock->part;
    SceSysmemSeg *seg = AddrToSeg(part, memBlock->addr);
    if (seg == NULL || seg->used == 0) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_FAILED_RESIZE_MEMBLOCK;
    }
    if (seg->sizeLocked) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INHIBITED_RESIZE_MEMBLOCK;
    }
    u32 newLeftOff;
    if (leftShift <= 0) {
        newLeftOff = seg->offset + leftSegs;
    } else {
        newLeftOff = seg->offset - leftSegs;
    }
    u32 newLeftAddr = part->addr + (newLeftOff << 8);
    u32 newRightOff;
    if (rightShift <= 0) {
        newRightOff = seg->offset + seg->size - rightSegs;
    } else
        newRightOff = seg->offset + seg->size + rightSegs;
    if (newLeftAddr >= part->addr + (newRightOff << 8) ||
        newLeftAddr < part->addr ||
        newLeftAddr >= part->addr + part->size) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_FAILED_RESIZE_MEMBLOCK;
    }
    SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk*)(((u32)seg >> 8) << 8);
    SceSysmemSeg *prevSeg = &ctlBlk->segs[seg->prev];
    if (seg->prev == 0x3F) {
        prevSeg = NULL;
        if (ctlBlk->prev != NULL)
            prevSeg = &ctlBlk->prev->segs[ctlBlk->prev->lastSeg];
    }
    SceSysmemSeg *nextSeg = &ctlBlk->segs[seg->next];
    if (seg->next == 0x3F) {
        nextSeg = NULL;
        if (ctlBlk->next != NULL)
            nextSeg = &ctlBlk->next->segs[ctlBlk->next->firstSeg];
    }
    if (leftShift > 0 && (prevSeg == NULL || prevSeg->used != 0 || prevSeg->size < leftSegs)) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_FAILED_RESIZE_MEMBLOCK;
    }
    if (rightShift > 0 && (nextSeg == NULL || nextSeg->used != 0 || nextSeg->size < rightSegs)) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_FAILED_RESIZE_MEMBLOCK;
    }
    // 4FF0
    seg->checkOverflow = 0;
    if (leftSegs != 0) {
        memBlock->addr = (void *)newLeftAddr;
        if (leftShift <= 0) {
            // 524C
            memBlock->size -= (leftSegs << 8);
            seg->offset += leftSegs;
            seg->size -= leftSegs;
            if (prevSeg == NULL || prevSeg->used != 0) {
                // 52A8
                SceSysmemSeg *newPrevSeg = &ctlBlk->segs[ctlBlk->freeSeg];
                u32 newPrev = ctlBlk->freeSeg;
                ctlBlk->freeSeg = newPrevSeg->next;
                newPrevSeg->next = (((u32)seg & 0xFF) - 16) / 8;
                newPrevSeg->prev = seg->prev;
                if (newPrevSeg->prev == 0x3F) {
                    // 5370
                    ctlBlk->firstSeg = newPrev;
                } else
                    ctlBlk->segs[seg->prev].next = newPrev;
                // 5320
                seg->prev = newPrev;
                ctlBlk->segCount++;
                newPrevSeg->offset = seg->offset - leftSegs;
                newPrevSeg->size = leftSegs;
                // 523C dup
                updateSmemCtlBlk(part, (SceSysmemCtlBlk*)((u32)newPrevSeg & 0xFFFFFF00));
            } else {
                // 504C dup
                prevSeg->size += leftSegs;
            }
        } else {
            memBlock->size += leftSegs << 8;
            seg->offset -= leftSegs;
            seg->size += leftSegs;
            if (prevSeg->size == leftSegs) {
                // 5224
                _ReturnSegBlankList(prevSeg);
                // 523C dup
                updateSmemCtlBlk(part, (SceSysmemCtlBlk*)((u32)prevSeg & 0xFFFFFF00));
            } else {
                // 504C dup
                prevSeg->size -= leftSegs;
            }
        }
    }
    // 5054
    if (rightSegs != 0) {
        if (rightShift <= 0) {
            // 50EC
            memBlock->size -= rightSegs << 8;
            seg->size -= rightSegs;
            if (nextSeg == NULL || nextSeg->used != 0) {
                // 514C
                SceSysmemSeg *newNextSeg = &ctlBlk->segs[ctlBlk->freeSeg];
                u32 newNext = ctlBlk->freeSeg;
                ctlBlk->freeSeg = newNextSeg->next;
                newNextSeg->next = seg->next;
                newNextSeg->prev = (((u32)seg & 0xFF) - 16) >> 3;
                if (newNextSeg->next == 0x3F)
                    ctlBlk->lastSeg = newNext;
                else
                    ctlBlk->segs[seg->next].prev = newNext;
                // 51D0
                seg->next = newNext;
                ctlBlk->segCount++;
                newNextSeg->offset = seg->offset + seg->size;
                newNextSeg->size = rightSegs;
                updateSmemCtlBlk(part, (SceSysmemCtlBlk*)((u32)newNextSeg & 0xFFFFFF00));
            } else {
                nextSeg->size += rightSegs;
                nextSeg->offset -= rightSegs;
            }
        } else {
            memBlock->size += (rightSegs << 8);
            seg->size += rightSegs;
            if (nextSeg->size != rightSegs) {
                nextSeg->size -= rightSegs;
                nextSeg->offset += rightSegs;
            } else {
                // 50C0
                _ReturnSegBlankList(nextSeg);
                updateSmemCtlBlk(part, (SceSysmemCtlBlk*)((u32)nextSeg & 0xFFFFFF00));
            }
        }
    }
    resumeIntr(oldIntr);
    return 0;
}

s32 sceKernelJointMemoryBlock(SceUID id1, SceUID id2)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid1, *uid2;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id1, g_MemBlockType, &uid1);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    ret = sceKernelGetUIDcontrolBlockWithType(id2, g_MemBlockType, &uid2);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    SceSysmemMemoryBlock *memBlock1 = UID_CB_TO_DATA(uid1, g_MemBlockType, SceSysmemMemoryBlock);
    SceSysmemSeg *seg1 = AddrToSeg(memBlock1->part, memBlock1->addr);
    SceSysmemMemoryBlock *memBlock2 = UID_CB_TO_DATA(uid2, g_MemBlockType, SceSysmemMemoryBlock);
    SceSysmemSeg *seg2 = AddrToSeg(memBlock2->part, memBlock2->addr);
    if (memBlock1->part != memBlock2->part || seg1->offset + seg1->size != seg2->offset) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_MEMBLOCK_FRAGMENTED;
    }
    if (seg1->sizeLocked || seg2->sizeLocked) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INHIBITED_RESIZE_MEMBLOCK;
    }
    if (uid1->attr != uid2->attr) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_MEMBLOCK_CANNOT_JOINT;
    }
    SceSysmemUidCB *newUid;
    ret = sceKernelCreateUID(g_MemBlockType, uid1->name, (uid1->attr | ((s32)pspGetK1() >> 31)) & 0xFF, &newUid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    seg1->size += seg2->size;
    SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk*)((u32)seg2 & 0xFFFFFF00);
    ctlBlk->usedSeg--;
    SceSysmemMemoryBlock *newMemBlock = UID_CB_TO_DATA(newUid, g_MemBlockType, SceSysmemMemoryBlock);
    newMemBlock->addr = memBlock1->addr;
    newMemBlock->size = seg1->size << 8;
    newMemBlock->part = memBlock1->part;
    if (sceKernelIsToolMode() != 0 && sceKernelDipsw(24) == 1) {
        if (newMemBlock->part == MpidToCB(2)) {
            newMemBlock->size -= 256;
            s32 i;
            u32 *guard = (u32 *)((u8 *)newMemBlock->addr + newMemBlock->size);
            for (i = 0; i < 64; i++)
                guard[i] = (u32)newMemBlock->addr - 1;
            seg1->checkOverflow = 1;
        }
    }
    _ReturnSegBlankList(seg2);
    updateSmemCtlBlk(newMemBlock->part, ctlBlk);
    sceKernelCallUIDObjFunction(uid1, 0x2430FC6B);
    sceKernelCallUIDObjFunction(uid2, 0x2430FC6B);
    resumeIntr(oldIntr);
    return newUid->uid ^ gSysmemRandomValue;
}

s32 sceKernelSeparateMemoryBlock(SceUID id, u32 cutBefore, u32 size)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock);
    SceSysmemSeg *seg = AddrToSeg(memBlock->part, memBlock->addr);
    SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk*)((u32)seg & 0xFFFFFF00);
    if (seg->sizeLocked) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_INHIBITED_RESIZE_MEMBLOCK;
    }
    u32 cutSegSize = (size + 0xFF) >> 8;
    if (cutSegSize >= seg->size) {
        resumeIntr(oldIntr);
        return SCE_ERROR_KERNEL_MEMBLOCK_CANNOT_SEPARATE;
    }
    SceSysmemUidCB *newUid;
    ret = sceKernelCreateUID(g_MemBlockType, uid->name, (uid->attr | ((s32)pspGetK1() >> 31)) & 0xFF, &newUid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    SceSysmemSeg *freeSeg;
    if (cutBefore) {
        u32 freeId = ctlBlk->freeSeg;
        freeSeg = &ctlBlk->segs[freeId];
        ctlBlk->freeSeg = freeSeg->next;
        freeSeg->next = (((u32)seg & 0xFF) - 16) >> 3;
        freeSeg->prev = seg->prev;
        if (freeSeg->prev == 0x3F) {
            ctlBlk->firstSeg = freeId;
        } else
            ctlBlk->segs[seg->prev].next = freeId;
        seg->prev = freeId;
        ctlBlk->segCount++;
        freeSeg->offset = seg->offset;
        seg->offset = freeSeg->offset + seg->size - cutSegSize;
    } else {
        u32 freeId = ctlBlk->freeSeg;
        freeSeg = &ctlBlk->segs[freeId];
        ctlBlk->freeSeg = freeSeg->next;
        freeSeg->next = seg->next;
        freeSeg->prev = (((u32)seg & 0xFF) - 16) >> 3;
        if (freeSeg->next == 0x3F)
            ctlBlk->lastSeg = freeId;
        else
            ctlBlk->segs[seg->next].prev = freeId;
        seg->next = freeId;
        ctlBlk->segCount++;
        freeSeg->offset = seg->offset + cutSegSize;
    }
    freeSeg->size = seg->size - cutSegSize;
    freeSeg->used = 1;
    seg->size = cutSegSize;
    seg->checkOverflow = 0;
    memBlock->addr = (void *)(memBlock->part->addr + (seg->offset << 8));
    memBlock->size = cutSegSize << 8;
    ctlBlk->usedSeg++;
    SceSysmemMemoryBlock *newMemBlock = UID_CB_TO_DATA(newUid, g_MemBlockType, SceSysmemMemoryBlock);
    newMemBlock->addr = (void *)(memBlock->part->addr + (freeSeg->offset << 8));
    newMemBlock->size = freeSeg->size << 8;
    newMemBlock->part = memBlock->part;
    if (sceKernelIsToolMode() != 0) {
        if (sceKernelDipsw(24) == 1) {
            if (newMemBlock->part == MpidToCB(2)) {
                newMemBlock->size -= 256;
                s32 i;
                u32 *guard = (u32 *)((u8 *)newMemBlock->addr + newMemBlock->size);
                for (i = 0; i < 64; i++)
                    guard[i] = (u32)newMemBlock->addr - 1;
                freeSeg->checkOverflow = 1;
            }
        }
    }
    updateSmemCtlBlk(newMemBlock->part, ctlBlk);
    resumeIntr(oldIntr);
    return newUid->uid ^ gSysmemRandomValue;
}

s32 sceKernelQueryMemoryInfoForUser(u32 address, SceUID *partitionId, SceUID *memoryBlockId)
{
    s32 oldK1 = pspShiftK1();
    if (!pspK1PtrOk((void *)address) || !pspK1PtrOk(partitionId) || !pspK1PtrOk(memoryBlockId)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    }
    s32 oldIntr = suspendIntr();
    s32 partId;
    SceUID memBlockId = 0;
    SceUID *memBlockPtr = &memBlockId;
    if (memoryBlockId == NULL) {
        SceSysmemMemoryPartition *part = AddrToCB(address);
        SceSysmemUidCB *curUid = g_PartType->PARENT0;
        partId = -1;
        while (curUid != g_PartType) {
            if (UID_CB_TO_DATA(curUid, g_PartType, SceSysmemMemoryPartition) == part) {
                partId = curUid->uid ^ gSysmemRandomValue;
                break;
            }
            curUid = curUid->PARENT0;
        }
    } else {
        SceSysmemMemoryPartition *part = AddrToCB(address);
        SceSysmemUidCB *curUid = g_PartType->PARENT0;
        partId = -1;
        while (curUid != g_PartType) {
            if (UID_CB_TO_DATA(curUid, g_PartType, SceSysmemMemoryPartition) == part) {
                partId = curUid->uid ^ gSysmemRandomValue;
                break;
            }
            curUid = curUid->PARENT0;
        }
        if (memBlockPtr != NULL) {
            curUid = g_MemBlockType->PARENT0;
            SceUID foundId = -1;
            while (curUid != g_MemBlockType) {
                SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(curUid, g_MemBlockType, SceSysmemMemoryBlock);
                if (address >= (u32)memBlock->addr && address < (u32)memBlock->addr + memBlock->size) {
                    foundId = curUid->uid ^ gSysmemRandomValue;
                    break;
                }
                curUid = curUid->PARENT0;
            }
            *memBlockPtr = foundId;
        }
    }
    // 5BA8
    if (partId < 0) {
        // 5C48
        resumeIntr(oldIntr);
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    if (memoryBlockId != NULL) {
        SceSysmemUidCB *uid;
        s32 ret = sceKernelGetUIDcontrolBlockWithType(memBlockId, g_MemBlockType, &uid);
        if (ret != 0) {
            // 5C34
            resumeIntr(oldIntr);
            pspSetK1(oldK1);
            return ret;
        }
        if (pspK1IsUserMode() && (uid->attr & 1) == 0) {
            // 5C20
            resumeIntr(oldIntr);
            pspSetK1(oldK1);
            return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        *memoryBlockId = memBlockId;
    }
    // 5BF8
    if (partitionId != NULL)
        *partitionId = CanoniMpid(partId);
    // 5C0C
    pspSetK1(oldK1);
    resumeIntr(oldIntr);
    return 0;
}

s32 sceKernelQueryMemoryBlockInfo(SceUID id, SceSysmemMemoryBlockInfo *infoPtr)
{
    SceSysmemMemoryBlockInfo info;
    if (infoPtr == NULL)
        return SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    if (infoPtr->size != sizeof(SceSysmemMemoryBlockInfo))
        return SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock);
    SceSysmemSeg *seg = AddrToSeg(memBlock->part, memBlock->addr);
    u32 *infoWords = (u32 *)&info;
    u32 i;
    for (i = 0; i < sizeof(info) / 4; i++)
        infoWords[i] = 0;
    info.size = sizeof info;
    if (uid->name != 0) {
        strncpy(info.name, uid->name, 31);
    }
    info.attr = uid->attr;
    info.sizeLocked = seg->sizeLocked;
    info.addr = (u32)memBlock->addr;
    info.memSize = memBlock->size;
    info.used = seg->used;
    *infoPtr = info;
    resumeIntr(oldIntr);
    return 0;
}

static inline __attribute__((always_inline)) void allocSplitTailSeg(SceSysmemSeg *curSeg, u32 numSegs)
{
    SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk *)((u32)curSeg & 0xFFFFFF00);
    u32 freeSegId = ctlBlk->freeSeg;
    SceSysmemSeg *firstSeg = &ctlBlk->segs[freeSegId];
    ctlBlk->freeSeg = firstSeg->next;
    u32 curSegId = (((u32)curSeg & 0xFF) - 16) / 8;
    firstSeg->next = curSeg->next;
    firstSeg->prev = curSegId;
    if (firstSeg->next == 0x3F)
        ctlBlk->lastSeg = freeSegId;
    else
        ctlBlk->segs[curSeg->next].prev = freeSegId;
    curSeg->next = freeSegId;
    ctlBlk->segCount++;
    firstSeg->offset = curSeg->offset + numSegs;
    firstSeg->size = curSeg->size - numSegs;
    curSeg->size = numSegs;
}

static inline __attribute__((always_inline)) void *allocFinishSeg(SceSysmemMemoryPartition *part, SceSysmemCtlBlk *curCtlBlk,
                                   u32 addr, u32 numSegs, SceSysmemCtlBlk **ctlBlkOut)
{
    if (sceKernelIsToolMode() && sceKernelDipsw(24) == 1 && part == MpidToCB(2)) {
        u32 i;
        s32 *ptr = (s32 *)(addr + (numSegs << 8) - 0x100);
        for (i = 0; i < 64; i++)
            ptr[i] = addr - 1;
    }
    if (ctlBlkOut == NULL) {
        updateSmemCtlBlk(part, curCtlBlk);
    } else
        *ctlBlkOut = curCtlBlk;
    return (void *)addr;
}

static inline __attribute__((always_inline)) void *allocCommitAddr(SceSysmemMemoryPartition *part, SceSysmemCtlBlk *curCtlBlk,
                                    SceSysmemSeg *curSeg, u32 blockOff, u32 numSegs,
                                    SceSysmemCtlBlk **ctlBlkOut)
{
    u32 addr = part->addr + (blockOff << 8);
    curCtlBlk->usedSeg++;
    if (curSeg->offset < blockOff)
    {
        SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk *)((u32)curSeg & 0xFFFFFF00);
        u16 freeSegId = ctlBlk->freeSeg;
        SceSysmemSeg *freeSeg = &ctlBlk->segs[freeSegId];
        ctlBlk->freeSeg = freeSeg->next;
        freeSeg->next = (u32)(((u32)curSeg & 0xFF) - 16) / 8;
        freeSeg->prev = curSeg->prev;
        if (curSeg->prev == 0x3F) {
            ctlBlk->firstSeg = freeSegId;
        } else
            ctlBlk->segs[curSeg->prev].next = freeSegId;
        curSeg->prev = freeSegId;
        ctlBlk->segCount++;
        freeSeg->offset = curSeg->offset;
        freeSeg->size = blockOff - curSeg->offset;
        curSeg->offset = blockOff;
        curSeg->size -= freeSeg->size;
    }
    curSeg->used = 1;
    curSeg->isProtected = 0;
    curSeg->sizeLocked = 0;
    if (sceKernelIsToolMode() && sceKernelDipsw(24) == 1 && part == MpidToCB(2))
        curSeg->checkOverflow = 1;
    if (numSegs < curSeg->size)
        allocSplitTailSeg(curSeg, numSegs);
    return allocFinishSeg(part, curCtlBlk, addr, numSegs, ctlBlkOut);
}

void *_allocSysMemory(SceSysmemMemoryPartition *part, int type, u32 size, u32 addr, SceSysmemCtlBlk **ctlBlkOut)
{
    u32 numSegs = (size + 0xFF) >> 8;
    if (numSegs == 0)
        return 0;
    if (sceKernelIsToolMode()) {
        if (sceKernelDipsw(24) == 1 && part == MpidToCB(2) &&
            (type != 0 || part->firstCtlBlk->segs[part->firstCtlBlk->firstSeg].used == 1))
            numSegs++;
    }
    SceSysmemCtlBlk *curCtlBlk;
    SceSysmemSeg *curSeg;
    u32 blockOff;
    int doLow = 0;
    int doHigh = 0;
    switch (type)
    {
    case 0:
        curCtlBlk = part->firstCtlBlk;
        doLow = 1;
        break;

    case 1:
        curCtlBlk = part->lastCtlBlk;
        doHigh = 1;
        break;

    case 2:
        if (addr < part->addr || addr >= part->addr + part->size) {
            Kprintf("ADDR: 0x%08x doesn't reside in this partition.\n", addr);
            return 0;
        }
        {
            u32 off = (addr & 0x1FFFFFFF) - (part->addr & 0x1FFFFFFF);
            if ((off & 0xFF) != 0)
            {
                Kprintf("%s: Illegal alloc address or size, offset=0x%x\n", "SCE_KERNEL_SMEM_ADDR", off);
                return 0;
            }
            curCtlBlk = part->firstCtlBlk;
            blockOff = off >> 8;
        }
        while (curCtlBlk != NULL) {
            SceSysmemSeg *firstSeg = &curCtlBlk->segs[curCtlBlk->firstSeg];
            SceSysmemSeg *lastSeg = &curCtlBlk->segs[curCtlBlk->lastSeg];
            if (blockOff >= firstSeg->offset && blockOff < (u32)lastSeg->offset + lastSeg->size) {
                curSeg = firstSeg;
                u32 i;
                for (i = 0; i < curCtlBlk->segCount; i++) {
                    if (blockOff >= curSeg->offset && blockOff < (u32)curSeg->offset + curSeg->size) {
                        if (curSeg->used) {
                            Kprintf("ADDR: relevant block is already used, can not alloc\n");
                            Kprintf("ADDR: request nblocks %u ,addr 0x%08x\n", numSegs, addr);
                            return 0;
                        }
                        if (numSegs > (u32)curSeg->offset + curSeg->size - blockOff) {
                            Kprintf("ADDR: relevant block is smaller than requested, can not alloc\n");
                            Kprintf("ADDR: request nblocks %u, addr 0x%08x\n", numSegs, addr);
                            return 0;
                        }
                        return allocCommitAddr(part, curCtlBlk, curSeg, blockOff, numSegs, ctlBlkOut);
                    }
                    curSeg = &curCtlBlk->segs[curSeg->next];
                }
            }
            curCtlBlk = curCtlBlk->next;
        }
        return 0;

    case 3:
        curCtlBlk = part->firstCtlBlk;
        if (addr < 0x101) {
            doLow = 1;
            break;
        }
        while (curCtlBlk != NULL) {
            curSeg = &curCtlBlk->segs[curCtlBlk->firstSeg];
            u32 i = 0, j = 0;
            while (i < curCtlBlk->segCount && j < (u32)curCtlBlk->segCount - curCtlBlk->usedSeg) {
                if (curSeg->used == 0) {
                    if (curSeg->size >= numSegs) {
                        u32 freeSize = (part->addr + (curSeg->offset << 8)) % addr;
                        blockOff = (u32)(addr - freeSize) >> 8;
                        if (freeSize == 0)
                            blockOff = 0;
                        if (curSeg->size >= numSegs + blockOff) {
                            blockOff += curSeg->offset;
                            return allocCommitAddr(part, curCtlBlk, curSeg, blockOff, numSegs, ctlBlkOut);
                        }
                    }
                    j++;
                }
                i++;
                curSeg = &curCtlBlk->segs[curSeg->next];
            }
            curCtlBlk = curCtlBlk->next;
        }
        return 0;

    case 4:
        curCtlBlk = part->lastCtlBlk;
        if (addr < 0x101) {
            doHigh = 1;
            break;
        }
        while (curCtlBlk != NULL) {
            curSeg = &curCtlBlk->segs[curCtlBlk->lastSeg];
            u32 i = 0, j = 0;
            while (i < curCtlBlk->segCount && j < (u32)curCtlBlk->segCount - curCtlBlk->usedSeg) {
                if (curSeg->used == 0) {
                    j++;
                    if (curSeg->size >= numSegs) {
                        u32 endOff = curSeg->offset + curSeg->size;
                        u32 hi = (part->addr + ((endOff - numSegs) << 8)) % addr;
                        u32 segSize = numSegs + (hi >> 8);
                        if (curSeg->size >= segSize) {
                            blockOff = endOff - segSize;
                            return allocCommitAddr(part, curCtlBlk, curSeg, blockOff, numSegs, ctlBlkOut);
                        }
                    }
                }
                i++;
                curSeg = &curCtlBlk->segs[curSeg->prev];
            }
            curCtlBlk = curCtlBlk->prev;
        }
        return 0;

    default:
        return 0;
    }

    if (doLow) {
        while (curCtlBlk != NULL) {
            curSeg = &curCtlBlk->segs[curCtlBlk->firstSeg];
            u32 i = 0, j = 0;
            while (j < (u32)curCtlBlk->segCount - curCtlBlk->usedSeg && i < curCtlBlk->segCount) {
                if (curSeg->used == 0) {
                    j++;
                    if (curSeg->size >= numSegs) {
                        curCtlBlk->usedSeg++;
                        curSeg->used = 1;
                        addr = part->addr + (curSeg->offset << 8);
                        curSeg->isProtected = 0;
                        curSeg->sizeLocked = 0;
                        if (sceKernelIsToolMode() && sceKernelDipsw(24) == 1 &&
                            part == MpidToCB(2) && curSeg != &part->firstCtlBlk->segs[part->firstCtlBlk->firstSeg])
                            curSeg->checkOverflow = 1;
                        if (curSeg->size != numSegs)
                            allocSplitTailSeg(curSeg, numSegs);
                        return allocFinishSeg(part, curCtlBlk, addr, numSegs, ctlBlkOut);
                    }
                }
                i++;
                curSeg = &curCtlBlk->segs[curSeg->next];
            }
            curCtlBlk = curCtlBlk->next;
        }
        return 0;
    }

    if (doHigh) {
        while (curCtlBlk != NULL) {
            SceSysmemSeg *hiSeg = &curCtlBlk->segs[curCtlBlk->lastSeg];
            u32 i = 0, j = 0;
            while (i < curCtlBlk->segCount && j < (u32)curCtlBlk->segCount - curCtlBlk->usedSeg) {
                if (hiSeg->used == 0) {
                    j++;
                    if (hiSeg->size >= numSegs) {
                        curCtlBlk->usedSeg++;
                        hiSeg->used = 1;
                        addr = part->addr + ((hiSeg->offset + hiSeg->size - numSegs) << 8);
                        hiSeg->isProtected = 0;
                        hiSeg->sizeLocked = 0;
                        if (sceKernelIsToolMode() && sceKernelDipsw(24) == 1 && part == MpidToCB(2))
                            hiSeg->checkOverflow = 1;
                        if (hiSeg->size != numSegs) {
                            SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk *)((u32)hiSeg & 0xFFFFFF00);
                            u16 freeSegId = ctlBlk->freeSeg;
                            u32 curSegId = (((u32)hiSeg & 0xFF) - 16) / 8;
                            SceSysmemSeg *freeSeg = &ctlBlk->segs[freeSegId];
                            ctlBlk->freeSeg = freeSeg->next;
                            freeSeg->next = curSegId;
                            freeSeg->prev = hiSeg->prev;
                            if (hiSeg->prev == 0x3F) {
                                ctlBlk->firstSeg = freeSegId;
                            } else
                                ctlBlk->segs[hiSeg->prev].next = freeSegId;
                            hiSeg->prev = freeSegId;
                            ctlBlk->segCount++;
                            freeSeg->offset = hiSeg->offset;
                            freeSeg->size = hiSeg->size - numSegs;
                            hiSeg->offset += hiSeg->size - numSegs;
                            hiSeg->size = numSegs;
                        }
                        return allocFinishSeg(part, curCtlBlk, addr, numSegs, ctlBlkOut);
                    }
                }
                i++;
                hiSeg = &curCtlBlk->segs[hiSeg->prev];
            }
            curCtlBlk = curCtlBlk->prev;
        }
    }
    return 0;
}

s32 FreeUsedSeg(SceSysmemMemoryPartition *part, SceSysmemSeg *seg)
{
    SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk *)(((u32)seg >> 8) << 8);
    if (ctlBlk->segCount < 2)
        return SCE_ERROR_KERNEL_ERROR;
    ctlBlk->usedSeg--;
    seg->isProtected = 0;
    seg->sizeLocked = 0;
    seg->checkOverflow = 0;
    seg->used = 0;
    SceSysmemSeg *nextSeg = &ctlBlk->segs[seg->next];
    if (seg->next == 0x3F) {
        // 6B5C
        nextSeg = NULL;
        if (ctlBlk->next != 0)
            nextSeg = &ctlBlk->segs[ctlBlk->next->firstSeg];
    }
    // 693C
    SceSysmemSeg *prevSeg = &ctlBlk->segs[seg->prev];
    if (seg->prev == 0x3F) {
        // 6B40
        prevSeg = NULL;
        if (ctlBlk->prev != NULL)
            prevSeg = &ctlBlk->segs[ctlBlk->prev->lastSeg];
    }
    // 6960
    if (prevSeg != NULL && nextSeg != NULL)
    {
        // 6A1C
        if (prevSeg->used != 0 && nextSeg->used != 0)
            return 0;
        // 6A3C
        if (prevSeg->used == 0 || nextSeg->used != 0) {
            // 6A9C
            if (prevSeg->used == 0 && nextSeg->used != 0) {
                // 69C4 dup
                prevSeg->size = nextSeg->size + seg->size;
                // 69DC dup
                _ReturnSegBlankList(seg);
            } else {
                // 6AB8
                if (seg->next == 0x3F) {
                    // 6AF8
                    nextSeg->offset = prevSeg->offset;
                    nextSeg->size += prevSeg->size + seg->size;
                    _ReturnSegBlankList(prevSeg);
                    // 69DC dup
                    _ReturnSegBlankList(seg);
                } else {
                    prevSeg->size += seg->size + nextSeg->size;
                    _ReturnSegBlankList(seg);
                    // 69DC dup
                    _ReturnSegBlankList(nextSeg);
                }
            }
        } else if (seg->next != 0x3F) {
            // 69FC dup
            seg->size += nextSeg->size;
            // 69DC dup
            _ReturnSegBlankList(nextSeg);
        } else {
            nextSeg->offset = seg->offset;
            nextSeg->size += seg->size;
            // 69DC dup
            _ReturnSegBlankList(seg);
        }
    } else if (prevSeg == NULL) {
        // 69EC
        if (nextSeg->used != 0)
            return 0;
        // 69FC dup
        seg->size += nextSeg->size;
        // 69DC dup
        _ReturnSegBlankList(nextSeg);
    } else if (nextSeg == NULL) {
        // 69B0
        if (prevSeg->used != 0)
            return 0;
        // 69C4 dup
        prevSeg->size += seg->size;
        // 69DC dup
        _ReturnSegBlankList(seg);
    }
    // 6984
    updateSmemCtlBlk(part, ctlBlk);
    return 0;
}

void SeparateSmemCtlBlk(SceSysmemMemoryPartition *part, SceSysmemCtlBlk *ctlBlk)
{
    SceSysmemCtlBlk *allocedCtlBlk;
    SceSysmemMemoryPartition *kernel = MpidToCB(1);
    SceSysmemCtlBlk *newCtlBlk = _allocSysMemory(kernel, 3, 256, 256, &allocedCtlBlk);
    if (newCtlBlk == NULL)
        return;
    {
        u32 *words = (u32 *)newCtlBlk;
        s32 k;
        for (k = 0; k < 64; k++)
            words[k] = 0;
    }
    SceSysmemSeg *curSeg = &newCtlBlk->segs[0];
    // 6BEC
    s32 i;
    for (i = 0; i < 30; i++) {
        curSeg->next = i + 1;
        curSeg++;
    }
    newCtlBlk->lastSeg = 0x3F;
    u16 newSegCount = (((ctlBlk->segCount * 2) / 3) >> 1) & 0xFFFF;
    newCtlBlk->firstSeg = 0;
    newCtlBlk->freeSeg = newSegCount;
    newCtlBlk->lastSeg = newSegCount - 1;
    curSeg = &newCtlBlk->segs[newSegCount];
    newCtlBlk->segs[newSegCount & 0xFF].next = 0x3F;
    i = newSegCount;
    SceSysmemSeg *lastSeg = &ctlBlk->segs[ctlBlk->lastSeg];
    // 6C78
    while (i > 0) {
        if (lastSeg->used != 0) {
            ctlBlk->usedSeg--;
            newCtlBlk->usedSeg++;
        }
        // 6CA0
        curSeg->used = lastSeg->used;
        curSeg->isProtected = lastSeg->isProtected;
        curSeg->sizeLocked = lastSeg->sizeLocked;
        curSeg->checkOverflow = lastSeg->checkOverflow;
        curSeg->offset = lastSeg->offset;
        curSeg->size = lastSeg->size;
        _ReturnSegBlankList(lastSeg);
        curSeg->prev = i - 2;
        i--;
        curSeg--;
        lastSeg = &ctlBlk->segs[ctlBlk->lastSeg];
    }
    // 6D34
    newCtlBlk->prev = ctlBlk;
    newCtlBlk->segs[0].next = 0x3F;
    newCtlBlk->next = ctlBlk->next;
    if (ctlBlk->next == NULL)
        part->lastCtlBlk = newCtlBlk;
    else
        ctlBlk->next->prev = newCtlBlk;
    // 6D5C
    newCtlBlk->segCount = newSegCount;
    part->ctlBlkCount++;
    ctlBlk->next = newCtlBlk;
    if (allocedCtlBlk == NULL || allocedCtlBlk == ctlBlk)
        return;
    if (allocedCtlBlk->segCount >= 27) {
        // 6DB8
        SeparateSmemCtlBlk(kernel, allocedCtlBlk);
    }
}

void updateSmemCtlBlk(SceSysmemMemoryPartition *part, SceSysmemCtlBlk *ctlBlk)
{
    if (part->ctlBlkCount != 1 && ctlBlk->segCount < 6) {
        // 6E34
        JointSmemCtlBlk(part, ctlBlk);
    }

    if (ctlBlk->segCount >= 27) {
        // 6E24
        SeparateSmemCtlBlk(part, ctlBlk);
    }
}

// 13514
SceSysmemUidLookupFunc MemBlockFuncs[] = {
    { 0xD310D2D9, block_do_initialize },
    { 0x87089863, block_do_delete },
    { 0x2430FC6B, block_do_delete_super },
    { 0xBB0E0136, block_do_resize },
    { 0x0927ECA3, block_do_sizelock },
    { 0x6CD3F455, block_do_querymeminfo },
    { 0x691F945E, block_do_queryblkinfo },
    { 0x9A8F243F, block_do_getheadaddr },
    { 0, NULL }
};

void MemoryBlockServiceInit(void)
{
    sceKernelCreateUIDtype("SceSysMemMemoryBlock", 12, MemBlockFuncs, 0, &g_MemBlockType);
}

void InitSmemCtlBlk(SceSysmemCtlBlk *ctlBlk)
{
    int i;
    u32 *words = (u32 *)ctlBlk;
    for (i = 0; i < 64; i++)
        words[i] = 0;
    for (i = 0; i < 30; i++)
        ctlBlk->segs[i].next = i + 1;
    ctlBlk->lastSeg = 0x3F;
    ctlBlk->firstSeg = 0x3F;
}

void *_AllocPartitionMemory(SceSysmemMemoryPartition *part, int type, int size, int addr)
{
    if (type == 2)
    {
        // 6EF4
        int startAddr = addr & 0xFFFFFF00;
        int endAddr = (addr + size + 0xFF) & 0xFFFFFF00;
        if ((addr & 0xFF) != 0) {
            size = endAddr - startAddr;
            addr = startAddr;
        }
    }
    // 6EE0
    return _allocSysMemory(part, type, size, addr, NULL);
}

s32 sceKernelSizeLockMemoryBlock(SceUID id)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    // 6F7C
    SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock);
    SceSysmemSeg *seg = AddrToSeg(memBlock->part, memBlock->addr);
    seg->sizeLocked = 1;
    resumeIntr(oldIntr);
    return 0;
}

s32 _FreePartitionMemory(void *addr)
{
    SceSysmemMemoryPartition *part = AddrToCB((u32)addr);
    if (part == MpidToCB(1)) {
        // 7004
        SceSysmemCtlBlk *curCtlBlk = part->firstCtlBlk;
        // 7010
        while (curCtlBlk != NULL) {
            if (addr == curCtlBlk)
                return SCE_ERROR_KERNEL_ERROR;
            curCtlBlk = curCtlBlk->next;
        }
    }
    // 6FE8
    return _freeSysMemory(part, addr);
}

s32 sceKernelFreePartitionMemory(SceUID id)
{
    SceSysmemUidCB *uid;
    s32 oldIntr = suspendIntr();
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid);
    if (ret == 0)
        ret = sceKernelDeleteUID(id);
    // 707C
    resumeIntr(oldIntr);
    return ret;
}

s32 sceKernelFreePartitionMemoryForUser(SceUID id)
{
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    s32 ret = sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid);
    if (ret == 0) {
        u32 perm = uid->attr & 0x10;
        if (!pspK1IsUserMode())
            perm = 0x10;
        if (perm == 0) {
            resumeIntr(oldIntr);
            pspSetK1(oldK1);
            return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
        }
        ret = sceKernelDeleteUID(id);
    }
    // 711C
    resumeIntr(oldIntr);
    pspSetK1(oldK1);
    return ret;
}

s32 sceKernelQueryMemoryInfo(u32 address, SceUID *partitionId, SceUID *memoryBlockId)
{
    s32 oldIntr = suspendIntr();
    _QueryMemoryInfo(address, partitionId, memoryBlockId);
    resumeIntr(oldIntr);
    return 0;
}

void *sceKernelGetBlockHeadAddr(SceUID id)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid) != 0) {
        // 721C
        resumeIntr(oldIntr);
        return 0;
    }
    void *addr = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock)->addr;
    // 721C
    resumeIntr(oldIntr);
    return addr;
}

u32 SysMemForKernel_CC31DEAD(SceUID id)
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid) != 0) {
        resumeIntr(oldIntr);
        return 0;
    }
    u32 size = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock)->size;
    resumeIntr(oldIntr);
    return size;
}

void *sceKernelGetBlockHeadAddrForUser(SceUID id)
{
    s32 oldK1 = pspShiftK1();
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid;
    if (sceKernelGetUIDcontrolBlockWithType(id, g_MemBlockType, &uid) != 0) {
        resumeIntr(oldIntr);
        pspSetK1(oldK1);
        return 0;
    }
    u32 perm = uid->attr & 1;
    if (!pspK1IsUserMode())
        perm = 1;
    if (perm == 0) {
        resumeIntr(oldIntr);
        pspSetK1(oldK1);
        return 0;
    }
    void *addr = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock)->addr;
    resumeIntr(oldIntr);
    pspSetK1(oldK1);
    return addr;
}

void sceKernelProtectMemoryBlock(SceSysmemMemoryPartition *part, void *addr)
{
    AddrToSeg(part, addr)->isProtected = 1;
}

s32 _freeSysMemory(SceSysmemMemoryPartition *part, void *addr)
{
    if (((u32)addr & 0xFF) != 0)
        return -1;
    s32 offset = ((u32)addr & 0x1FFFFFFF) - (part->addr & 0x1FFFFFFF);
    s32 segOff = offset / 256;
    SceSysmemCtlBlk *curCtlBlk = part->firstCtlBlk;
    // 73F8
    while (curCtlBlk != NULL) {
        SceSysmemSeg *curSeg = &curCtlBlk->segs[curCtlBlk->firstSeg];
        if (segOff >= curSeg->offset && segOff <= curCtlBlk->segs[curCtlBlk->lastSeg].offset) {
            // 7448
            s32 i;
            for (i = 0; i < curCtlBlk->segCount; i++) {
                if (curSeg->size != 0) {
                    // 75F0
                    if (curSeg->offset == segOff) {
                        // 74B8
                        if (curSeg->used == 0)
                            return SCE_ERROR_KERNEL_ERROR;
                        if (curSeg->isProtected != 0) {
                            // 7570
                            SceSysmemUidCB *curUid = g_MemBlockType->PARENT0;
                            SceUID foundId = -1;
                            while (curUid != g_MemBlockType) {
                                SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(curUid, g_MemBlockType, SceSysmemMemoryBlock);
                                if (addr >= memBlock->addr && addr < memBlock->addr + memBlock->size) {
                                    foundId = curUid->uid ^ gSysmemRandomValue;
                                    break;
                                }
                                curUid = curUid->PARENT0;
                            }
                            // 75C8
                            char name[32];
                            sceKernelGetUIDname(foundId, 32, name);
                            AddrToSeg(MpidToCB(1), addr);
                            return SCE_ERROR_KERNEL_ERROR;
                        }
                        if (curSeg->checkOverflow != 0) {
                            u32 *curPtr = (u32*)(addr + (curSeg->size - 1) * 256);
                            // 74EC
                            s32 i;
                            for (i = 0; i < 64; i++) {
                                if (*curPtr != (u32)addr - 1) {
                                    // 751C
                                    Kprintf("\n<< Memory block overflow detected >>\n");
                                    Kprintf(" Overflowed block: 0x%08x - 0x%08x\n", addr, addr + (curSeg->size - 1) * 256 - 1);
                                    Kprintf(" (address 0x%08x): expected value is 0x%08x, but actual value is 0x%08x\n", curPtr, addr - 1, *curPtr);
                                    pspBreak(0);
                                    break;
                                }
                                curPtr++;
                            }
                        }
                        // 7508
                        return FreeUsedSeg(part, curSeg);
                    }
                }
                // 746C
                curSeg = &curCtlBlk->segs[curSeg->next];
            }
        }
        // 7480
        curCtlBlk = curCtlBlk->next;
    }
    return SCE_ERROR_KERNEL_ERROR;
}

s32 block_do_initialize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap)
{
    sceKernelCallUIDObjCommonFunction(uid, uidWithFunc, funcId, ap);
    SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock);
    memBlock->size = 0;
    memBlock->part = NULL;
    memBlock->addr = 0;
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_delete(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId, va_list ap)
{
    void *addr = UID_CB_TO_DATA(uid, g_MemBlockType, SceSysmemMemoryBlock)->addr;
    if (addr != NULL) {
        SceSysmemMemoryPartition *part = AddrToCB((u32)addr);
        if (part == MpidToCB(1))
        {
            SceSysmemCtlBlk *curCtlBlk = part->firstCtlBlk;
            while (curCtlBlk != NULL) {
                if (addr == curCtlBlk) {
                    return SCE_ERROR_KERNEL_ERROR;
                }
                curCtlBlk = curCtlBlk->next;
            }
        }
        s32 ret = _freeSysMemory(part, addr);
        if (ret != 0)
            return ret;
    }
    sceKernelCallUIDObjCommonFunction(uid, uidWithFunc, funcId, ap);
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_delete_super(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc, s32 funcId __attribute__((unused)), va_list ap)
{
    sceKernelCallUIDObjCommonFunction(uid, uidWithFunc, 0x87089863, ap);
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_resize(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    s32 ret = sceKernelResizeMemoryBlock(uid->uid ^ gSysmemRandomValue, va_arg(ap, s32), va_arg(ap, s32));
    if (ret != 0)
        return ret;
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_sizelock(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap __attribute__((unused)))
{
    s32 oldIntr = suspendIntr();
    SceSysmemUidCB *uid2;
    s32 ret = sceKernelGetUIDcontrolBlockWithType((SceUID)uid, g_MemBlockType, &uid2);
    if (ret != 0) {
        resumeIntr(oldIntr);
        return ret;
    }
    SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(uid2, g_MemBlockType, SceSysmemMemoryBlock);
    AddrToSeg(memBlock->part, memBlock->addr)->sizeLocked = 1;
    resumeIntr(oldIntr);
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_querymeminfo(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    _QueryMemoryInfo(va_arg(ap, u32), va_arg(ap, SceUID*), va_arg(ap, SceUID*));
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_queryblkinfo(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    s32 ret = sceKernelQueryMemoryBlockInfo(uid->uid ^ gSysmemRandomValue, va_arg(ap, SceSysmemMemoryBlockInfo *));
    if (ret != 0)
        return ret;
    return uid->uid ^ gSysmemRandomValue;
}

s32 block_do_getheadaddr(SceSysmemUidCB *uid, SceSysmemUidCB *uidWithFunc __attribute__((unused)), s32 funcId __attribute__((unused)), va_list ap)
{
    void **addrPtr = va_arg(ap, void**);
    SceSysmemUidCB *uid2;
    void *addr = NULL;
    if (sceKernelGetUIDcontrolBlockWithType(uid->uid ^ gSysmemRandomValue, g_MemBlockType, &uid2) == 0)
        addr = UID_CB_TO_DATA(uid2, g_MemBlockType, SceSysmemMemoryBlock)->addr;
    *addrPtr = addr;
    return uid->uid ^ gSysmemRandomValue;
}

void _ReturnSegBlankList(SceSysmemSeg *seg)
{
    SceSysmemCtlBlk *ctlBlk = (SceSysmemCtlBlk *)((u32)seg & 0xFFFFFF00);
    u32 segId = (((u32)seg & 0xFF) - 16) >> 3;
    if (ctlBlk->segCount < 2)
        return;
    if (seg->next == 0x3F) {
        // 7A10
        ctlBlk->segs[seg->prev].next = seg->next;
        ctlBlk->lastSeg = seg->prev;
    } else if (seg->prev == 0x3F) {
        // 79F4
        ctlBlk->segs[seg->next].prev = seg->prev;
        ctlBlk->firstSeg = seg->next;
    } else {
        ctlBlk->segs[seg->prev].next = seg->next;
        ctlBlk->segs[seg->next].prev = seg->prev;
    }
    {
        u32 *segWords = (u32 *)seg;
        segWords[0] = 0;
        segWords[1] = 0;
    }
    seg->next = ctlBlk->freeSeg;
    ctlBlk->freeSeg = segId;
    ctlBlk->segCount--;
}

void _QueryMemoryInfo(u32 address, SceUID *partitionId, SceUID *memoryBlockId)
{
    if (partitionId != NULL) {
        SceSysmemMemoryPartition *part = AddrToCB(address);
        SceSysmemUidCB *curUid = g_PartType->PARENT0;
        SceUID foundId = -1;
        while (curUid != g_PartType) {
            if (UID_CB_TO_DATA(curUid, g_PartType, SceSysmemMemoryPartition) == part) {
                foundId = curUid->uid ^ gSysmemRandomValue;
                break;
            }
            curUid = curUid->PARENT0;
        }
        *partitionId = foundId;
    }
    if (memoryBlockId != NULL) {
        SceSysmemUidCB *curUid = g_MemBlockType->PARENT0;
        SceUID foundId = -1;
        while (curUid != g_MemBlockType) {
            SceSysmemMemoryBlock *memBlock = UID_CB_TO_DATA(curUid, g_MemBlockType, SceSysmemMemoryBlock);
            if (address >= (u32)memBlock->addr && address < (u32)memBlock->addr + memBlock->size) {
                foundId = curUid->uid ^ gSysmemRandomValue;
                break;
            }
            curUid = curUid->PARENT0;
        }
        *memoryBlockId = foundId;
    }
}

void JointSmemCtlBlk(SceSysmemMemoryPartition *part, SceSysmemCtlBlk *ctlBlk)
{
    SceSysmemMemoryPartition *kernelPart = MpidToCB(1);
    SceSysmemCtlBlk *nextCtlBlk = ctlBlk->next;
    SceSysmemCtlBlk *prevCtlBlk = ctlBlk->prev;
    if (part->ctlBlkCount == 1)
        return;
    if (prevCtlBlk != NULL && (nextCtlBlk == NULL || nextCtlBlk->segCount >= prevCtlBlk->segCount)) {
        // 7EF4
        if (prevCtlBlk->segCount < 16) {
            // 8090
            SceSysmemSeg *curSrcSeg = &ctlBlk->segs[ctlBlk->firstSeg];
            SceSysmemSeg *prevDstSeg = &prevCtlBlk->segs[prevCtlBlk->lastSeg];
            s32 i;
            // 80BC
            for (i = 0; i < ctlBlk->segCount; i++) {
                SceSysmemCtlBlk *newPrevCtlBlk = (SceSysmemCtlBlk *)((u32)prevDstSeg & 0xFFFFFF00);
                u32 newId = newPrevCtlBlk->freeSeg;
                SceSysmemSeg *curDstSeg = &newPrevCtlBlk->segs[newId];
                newPrevCtlBlk->freeSeg = curDstSeg->next;
                curDstSeg->next = prevDstSeg->next;
                curDstSeg->prev = (((u32)prevDstSeg & 0xFF) - 16) >> 3;
                if (curDstSeg->next == 0x3F)
                    newPrevCtlBlk->lastSeg = newId;
                else
                    newPrevCtlBlk->segs[prevDstSeg->next].prev = newId;
                // 813C
                prevDstSeg->next = newId;

                newPrevCtlBlk->segCount++;
                curDstSeg->used = curSrcSeg->used;
                curDstSeg->isProtected = curSrcSeg->isProtected;
                curDstSeg->sizeLocked = curSrcSeg->sizeLocked;;
                curDstSeg->checkOverflow = curSrcSeg->checkOverflow;
                curDstSeg->offset = curSrcSeg->offset;
                curDstSeg->size = curSrcSeg->size;
                curSrcSeg = &ctlBlk->segs[curSrcSeg->next];
                prevDstSeg = curDstSeg;
            }
            // 81E8
            prevCtlBlk->next = ctlBlk->next;
            prevCtlBlk->usedSeg += ctlBlk->usedSeg;
            if (nextCtlBlk != NULL)
                nextCtlBlk->prev = prevCtlBlk;
            // 8204
            _freeSysMemory(kernelPart, ctlBlk);
            part->ctlBlkCount--;
            if (part->lastCtlBlk == ctlBlk)
                part->lastCtlBlk = prevCtlBlk;
        } else {
            u16 joinCount = ((prevCtlBlk->segCount * 2) / 3) >> 1;
            SceSysmemSeg *curSrcSeg = &prevCtlBlk->segs[prevCtlBlk->lastSeg];
            SceSysmemSeg *prevDstSeg = &ctlBlk->segs[ctlBlk->firstSeg];
            // 7F44
            s32 i;
            for (i = 0; i < joinCount; i++) {
                SceSysmemCtlBlk *newPrevCtlBlk = (SceSysmemCtlBlk *)((u32)prevDstSeg & 0xFFFFFF00);
                u32 newId = newPrevCtlBlk->freeSeg;
                SceSysmemSeg *curDstSeg = &newPrevCtlBlk->segs[newId];
                newPrevCtlBlk->freeSeg = curDstSeg->next;
                curDstSeg->next = (((u32)prevDstSeg & 0xFF) - 16) >> 3;
                curDstSeg->prev = prevDstSeg->prev;
                if (curDstSeg->prev == 0x3F) {
                    // 8088
                    newPrevCtlBlk->firstSeg = newId;
                } else
                    newPrevCtlBlk->segs[prevDstSeg->prev].next = newId;
                // 7BF8
                prevDstSeg->prev = newId;
                newPrevCtlBlk->segCount++;
                if (curSrcSeg->used != 0) {
                    ctlBlk->usedSeg++;
                    prevCtlBlk->usedSeg--;
                }
                // 8000
                curDstSeg->used = curSrcSeg->used;
                curDstSeg->isProtected = curSrcSeg->isProtected;
                curDstSeg->sizeLocked = curSrcSeg->sizeLocked;
                curDstSeg->checkOverflow = curSrcSeg->checkOverflow;
                curDstSeg->offset = curSrcSeg->offset;
                curDstSeg->size = curSrcSeg->size;
                _ReturnSegBlankList(curSrcSeg);
                curSrcSeg = &prevCtlBlk->segs[prevCtlBlk->lastSeg];
                prevDstSeg = curDstSeg;
            }
        }
    } else {
        // 7B9C
        if (nextCtlBlk->segCount < 16) {
            // 7D58
            SceSysmemSeg *curSrcSeg = &ctlBlk->segs[ctlBlk->lastSeg];
            SceSysmemSeg *prevDstSeg = &nextCtlBlk->segs[nextCtlBlk->firstSeg];
            // 7D84
            s32 i;
            for (i = 0; i < ctlBlk->segCount; i++) {
                SceSysmemCtlBlk *newPrevCtlBlk = (SceSysmemCtlBlk *)((u32)prevDstSeg & 0xFFFFFF00);
                u32 newId = newPrevCtlBlk->freeSeg;
                SceSysmemSeg *curDstSeg = &newPrevCtlBlk->segs[newId];
                newPrevCtlBlk->freeSeg = curDstSeg->next;
                curDstSeg->next = (((u32)prevDstSeg & 0xFF) - 16) >> 3;
                curDstSeg->prev = prevDstSeg->prev;
                if (curDstSeg->prev == 0x3F) {
                    // 7EEC
                    newPrevCtlBlk->firstSeg = newId;
                } else
                    newPrevCtlBlk->segs[prevDstSeg->prev].next = newId;
                // 7DF8
                prevDstSeg->prev = newId;
                newPrevCtlBlk->segCount++;
                curDstSeg->used = curSrcSeg->used;
                curDstSeg->isProtected = curSrcSeg->isProtected;
                curDstSeg->sizeLocked = curSrcSeg->sizeLocked;
                curDstSeg->checkOverflow = curSrcSeg->checkOverflow;
                curDstSeg->offset = curSrcSeg->offset;
                curDstSeg->size = curSrcSeg->size;
                curSrcSeg = &ctlBlk->segs[curSrcSeg->prev];
                prevDstSeg = curDstSeg;
            }
            // 7EA8
            nextCtlBlk->prev = ctlBlk->prev;
            nextCtlBlk->usedSeg += ctlBlk->usedSeg;
            if (prevCtlBlk != NULL)
                prevCtlBlk->next = nextCtlBlk;
            // 7EC4
            part->ctlBlkCount--;
            _freeSysMemory(kernelPart, ctlBlk);
            if (part->firstCtlBlk == ctlBlk)
                part->firstCtlBlk = nextCtlBlk;
        } else {
            u16 joinCount = ((nextCtlBlk->segCount * 2) / 3) >> 1;
            SceSysmemSeg *curSrcSeg = &nextCtlBlk->segs[nextCtlBlk->firstSeg];
            SceSysmemSeg *prevDstSeg = &ctlBlk->segs[ctlBlk->lastSeg];
            // 7BEC
            s32 i;
            for (i = 0; i < joinCount; i++) {
                SceSysmemCtlBlk *newPrevCtlBlk = (SceSysmemCtlBlk *)((u32)prevDstSeg & 0xFFFFFF00);
                u32 newId = newPrevCtlBlk->freeSeg;
                SceSysmemSeg *curDstSeg = &newPrevCtlBlk->segs[newId];
                newPrevCtlBlk->freeSeg = curDstSeg->next;
                curDstSeg->next = prevDstSeg->next;
                curDstSeg->prev = (((u32)prevDstSeg & 0xFF) - 16) >> 3;
                if (curDstSeg->next == 0x3F)
                    newPrevCtlBlk->lastSeg = newId;
                else
                    newPrevCtlBlk->segs[prevDstSeg->next].prev = newId;
                // 7C6C
                prevDstSeg->next = newId;
                newPrevCtlBlk->segCount++;
                if (curSrcSeg->used != 0) {
                    ctlBlk->usedSeg++;
                    nextCtlBlk->usedSeg--;
                }
                // 7CB4
                curDstSeg->used = curSrcSeg->used;
                curDstSeg->isProtected = curSrcSeg->isProtected;
                curDstSeg->sizeLocked = curSrcSeg->sizeLocked;
                curDstSeg->checkOverflow = curSrcSeg->checkOverflow;
                curDstSeg->offset = curSrcSeg->offset;
                curDstSeg->size = curSrcSeg->size;
                _ReturnSegBlankList(curSrcSeg);
                curSrcSeg = &nextCtlBlk->segs[nextCtlBlk->firstSeg];
                prevDstSeg = curDstSeg;
            }
        }
    }
}

/* Allocates blocks in this format:
 *   [ Free space ] [ Allocated space (8-byte aligned) ] [ Padding blocks (for alignment) ]
 * Returns NULL when no fitting block exists in this lowheap. */

static inline __attribute__((always_inline)) void checkLowheapBounds(SceSysmemLowheap *lowh, const void *ptr)
{
    if ((u32)ptr - (u32)lowh - sizeof(*lowh) >= lowh->size) {
        if (!sceKernelIsToolMode()) {
            for (;;)
                ;
        }
        Kprintf("Heap memory is in illegal memory partition\n");
        pspBreak(0);
    }
}

void *hmalloc(SceSysmemHeap *heap __attribute__((unused)), SceSysmemLowheap *lowh, u32 size, u32 align)
{
    if (lowh == NULL || lowh->addr != (u32)lowh - 1)
        return NULL;
    SceSysmemLowheapBlock *startBlock = lowh->firstBlock;
    if (size < 8)
        size = 8;
    u32 allocBlocks = (size >> 3) + 1;
    if ((size & 7) != 0)
        allocBlocks = (size >> 3) + 2;
    checkLowheapBounds(lowh, startBlock);

    SceSysmemLowheapBlock *curBlock = startBlock->next;
    SceSysmemLowheapBlock *prevBlock = startBlock;
    SceSysmemLowheapBlock *prevFreeBlock = NULL;
    u32 paddingBlocks = 0;
    u32 reservedBlockCount = 0;
    int needsResize = 0;
    for (;;)
    {
        if ((u32)curBlock - (u32)lowh - sizeof(*lowh) >= lowh->size) {
            if (sceKernelIsToolMode() != 0) {
                Kprintf("Heap memory is in illegal memory partition\n");
                pspBreak(0);
            }
            for (;;)
                ;
        }
        if (curBlock->count >= allocBlocks) {
            if (curBlock->count == allocBlocks) {
                if (align == 0 || ((u32)(curBlock + 1) % align) == 0) {
                    checkLowheapBounds(lowh, curBlock->next);
                    prevBlock->next = curBlock->next;
                    needsResize = 0;
                    break;
                }
            } else {
                prevFreeBlock = NULL;
                paddingBlocks = 0;
                if (align == 0) {
                    reservedBlockCount = allocBlocks;
                    needsResize = 1;
                    break;
                }
                SceSysmemLowheapBlock *firstAllocBlock = curBlock + curBlock->count - allocBlocks + 1;
                u32 misalign = (u32)firstAllocBlock % align;
                if (misalign == 0) {
                    reservedBlockCount = allocBlocks;
                    needsResize = 1;
                    break;
                }
                paddingBlocks = misalign >> 3;
                reservedBlockCount = allocBlocks + paddingBlocks;
                if (curBlock->count >= reservedBlockCount) {
                    prevFreeBlock = curBlock;
                    if (curBlock->count == reservedBlockCount) {
                        prevFreeBlock = prevBlock;
                        checkLowheapBounds(lowh, curBlock->next);
                        prevBlock->next = curBlock->next;
                    }
                    needsResize = 1;
                    break;
                }
            }
        }
        if (curBlock == startBlock)
            return NULL;
        prevBlock = curBlock;
        curBlock = curBlock->next;
    }

    if (needsResize) {
        u32 remain = curBlock->count - reservedBlockCount;
        hcheck(lowh, curBlock, NULL, remain);
        curBlock->count = remain;
        curBlock += remain;
        curBlock->count = allocBlocks;
        hcheck(lowh, curBlock, NULL, allocBlocks);
        if (paddingBlocks != 0) {
            SceSysmemLowheapBlock *paddingBlock = curBlock + curBlock->count;
            checkLowheapBounds(lowh, paddingBlock);
            paddingBlock->count = paddingBlocks;
            checkLowheapBounds(lowh, prevFreeBlock->next);
            paddingBlock->next = prevFreeBlock->next;
            prevFreeBlock->next = paddingBlock;
        }
    }

    lowh->firstBlock = prevBlock;
    curBlock->next = (SceSysmemLowheapBlock *)lowh;
    lowh->busyBlocks += curBlock->count;
    hcheck(lowh, prevBlock, prevBlock->next, prevBlock->count);
    hcheck(lowh, curBlock, curBlock->next, curBlock->count);
    return curBlock + 1;
}

s32 hfree(SceSysmemHeap *heap __attribute__((unused)), SceSysmemLowheap *lowh, void *ptr)
{
    SceSysmemLowheapBlock *allocatedBlock = (SceSysmemLowheapBlock *)ptr - 1;
    if (lowh == NULL || lowh->addr != (u32)lowh - 1)
        return -4;
    if ((u32)allocatedBlock < (u32)(lowh + 1) || (u32)allocatedBlock >= (u32)lowh + lowh->size)
        return -2;
    if (ptr == NULL || allocatedBlock->next != (SceSysmemLowheapBlock *)lowh || allocatedBlock->count <= 0)
        return -1;
    hcheck(lowh, allocatedBlock, allocatedBlock->next, allocatedBlock->count);
    SceSysmemLowheapBlock *curBlock = lowh->firstBlock;
    for (;;) {
        hcheck(lowh, curBlock, curBlock->next, curBlock->count);
        if (curBlock < allocatedBlock && allocatedBlock < curBlock->next)
            break;
        if (allocatedBlock == curBlock)
            return -3;
        if (curBlock >= curBlock->next && (allocatedBlock < curBlock->next || curBlock < allocatedBlock))
            break;
        curBlock = curBlock->next;
    }
    if (allocatedBlock < curBlock->next && curBlock->next < allocatedBlock + allocatedBlock->count)
        return -3;
    if (curBlock < allocatedBlock && allocatedBlock < curBlock + curBlock->count)
        return -3;
    lowh->busyBlocks -= allocatedBlock->count;
    SceSysmemLowheapBlock *nextBlock = curBlock->next;
    hcheck(lowh, nextBlock, nextBlock->next, nextBlock->count);
    if (allocatedBlock + allocatedBlock->count == nextBlock && nextBlock->count > 0) {
        allocatedBlock->count += nextBlock->count;
        allocatedBlock->next = nextBlock->next;
    } else {
        allocatedBlock->next = nextBlock;
    }
    hcheck(lowh, allocatedBlock, allocatedBlock->next, allocatedBlock->count);
    curBlock->next = allocatedBlock;
    if (allocatedBlock == curBlock + curBlock->count) {
        curBlock->count += allocatedBlock->count;
        curBlock->next = allocatedBlock->next;
    }
    lowh->firstBlock = curBlock;
    return 0;
}

void initheap(SceSysmemLowheap *lowh, u32 size)
{
    u32 count = (size - 16) / 8;
    SceSysmemLowheapBlock *start = (SceSysmemLowheapBlock*)(lowh + 1);
    SceSysmemLowheapBlock *end = start + count - 1;
    if (lowh != NULL && size >= 41) {
        lowh->addr = (u32)lowh - 1;
        lowh->size = size;
        lowh->firstBlock = start;
        lowh->busyBlocks = 0;
        start->next = end;
        start->count = count - 1;
        end->next = start;
        end->count = 0;
    }
}

s32 hcheckblock(SceSysmemLowheap *lowh, void *ptr)
{
    SceSysmemLowheapBlock *block = (SceSysmemLowheapBlock *)ptr - 1;
    if (lowh == NULL || lowh->addr != (u32)lowh - 1)
        return -4;
    if ((u32)block < (u32)(lowh + 1) || (u32)block >= (u32)lowh + lowh->size)
        return -2;
    if (ptr == NULL || block->next != (SceSysmemLowheapBlock *)lowh || (s32)block->count <= 0)
        return -1;
    return 0;
}

s32 checkheapnouse(SceSysmemLowheap *lowh)
{
    if (lowh == NULL || lowh->addr != (u32)lowh - 1)
        return 0;
    return (lowh->busyBlocks == 0);
}

u32 htotalfreesize(SceSysmemLowheap *lowh)
{
    u32 numBlocks = (lowh->size - 16) / 8;
    u32 freeBlocks = numBlocks - lowh->busyBlocks;
    return (freeBlocks - 1) * 8;
}

s32 hmaxfreesize(SceSysmemHeap *heap, SceSysmemLowheap *lowh)
{
    SceSysmemLowheapBlock *firstBlock = lowh->firstBlock;
    u32 maxfreeblocks = 0;
    if ((u32)firstBlock - heap->partAddr >= heap->partSize)
        return -1;
    SceSysmemLowheapBlock *curBlock = firstBlock->next;
    for (;;) {
        if ((u32)curBlock - heap->partAddr >= heap->partSize)
            return -1;
        if (curBlock->count > maxfreeblocks)
            maxfreeblocks = curBlock->count;
        if (curBlock == firstBlock)
            return (maxfreeblocks - 1) * 8;
        curBlock = curBlock->next;
    }
}

void hcheck(SceSysmemLowheap *lowh, SceSysmemLowheapBlock *block, SceSysmemLowheapBlock *next, u32 units)
{
    if ((u32)block - (u32)lowh - sizeof(*lowh) >= lowh->size) {
        if (!sceKernelIsToolMode()) {
            for (;;)
                ;
        }
        Kprintf("Heap memory is in illegal memory partition\n");
        pspBreak(0);
    }
    if (next != (SceSysmemLowheapBlock *)lowh && next != NULL &&
        (u32)next - (u32)lowh - sizeof(*lowh) >= lowh->size) {
        if (!sceKernelIsToolMode()) {
            for (;;)
                ;
        }
        Kprintf("Heap memory (next) is in illegal memory partition\n");
        pspBreak(0);
    }
    if (units > 0x800000) {
        if (!sceKernelIsToolMode()) {
            for (;;)
                ;
        }
        Kprintf("Heap memory (units) is in illegal memory partition\n");
        pspBreak(0);
    }
    if ((u32)(block + units) - (u32)lowh - sizeof(*lowh) > lowh->size) {
        if (!sceKernelIsToolMode()) {
            for (;;)
                ;
        }
        Kprintf("Heap memory (pn) is in illegal memory partition\n");
        pspBreak(0);
    }
}

void *memset(void *s, int c, u32 n)
{
    if (s != NULL)
        return sceKernelMemset(s, c, n);
    return s;
}

void *sceKernelMemset(void *src, s8 c, u32 size)
{
    void *end = src + size;
    if (size >= 8) {
        u8 b = (u8)c;
        u32 value = b | (b << 8) | (b << 16) | (b << 24);
        void *alignedSrc = (void *)(((u32)src + 3) & 0xFFFFFFFC);
        if (alignedSrc != src) {
            asm("swr %0, 0(%1)" : : "r" (value), "r" (src));
            size -= alignedSrc - src;
        }
        if (((u32)end & 3) != 0)
            asm("swl %0, -1(%1)" : : "r" (value), "r" (end));
        sceKernelMemset32(alignedSrc, value, size);
    } else {
        s8 *cur = src;
        while (cur != end)
            *(cur++) = c;
    }
    return src;
}

void *sceKernelMemset32(void *src, s32 c, u32 size)
{
    size = size & 0xFFFFFFFC;
    if (((u32)src & 3) != 0)
        return NULL;
    if (((0x2C >> (((u32)src >> 29) & 0x7)) & 1) != 0) {
        return wmemset(src, c, size);
    }
    if (size < 257) {
        return wmemset(src, c, size);
    }
    void *alignedSrc = (void *)(((u32)src + 0x3F) & 0xFFFFFFC0);
    s32 *end = (void*)src + size;
    if (alignedSrc != src)
        wmemset(src, c, alignedSrc - src);
    if (((u32)end & 0x3F) != 0) {
        u32 tail = (u32)end & 0x3F;
        end = wmemset((void *)end - tail, c, tail);
    }
    s32 *curSrc = alignedSrc;
    while (curSrc != end) {
        pspCache(0x18, curSrc);
        curSrc[0] = curSrc[1] = curSrc[2] = curSrc[3] = c;
        curSrc[4] = curSrc[5] = curSrc[6] = curSrc[7] = c;
        curSrc[8] = curSrc[9] = curSrc[10] = curSrc[11] = c;
        curSrc[12] = curSrc[13] = curSrc[14] = curSrc[15] = c;
        curSrc += 16;
    }
    return src;
}

void *sceKernelMemmove(void *dst, void *src, u32 size)
{
    void *realDst = (void*)((u32)dst & 0x1FFFFFFF);
    void *realSrc = (void*)((u32)src & 0x1FFFFFFF);
    if (realDst == realSrc)
        return dst;
    u32 dist = realDst - realSrc;
    if (((0x2C >> (((u32)dst >> 29) & 7)) & 1) != 0) {
        // 8B0C
        return memmove(dst, src, size);
    }
    if (dist + 63 < 126) {
        // 8B0C
        return memmove(dst, src, size);
    }
    if (realDst < realSrc) {
        // 8B1C
        return sceKernelMemcpy(dst, src, size);
    }
    u32 dstAlign = (-(u32)dst) & 0x3F;
    if (dstAlign >= size) {
        // 8B1C
        return sceKernelMemcpy(dst, src, size);
    }
    if ((dist & 3) != 0) {
        // 8B0C
        return memmove(dst, src, size);
    }
    void *dstEnd = dst + size;
    u32 dstEndAlign = (u32)dstEnd & 0x3F;
    if (dstEndAlign != 0) {
        size -= dstEndAlign;
        // 8AF8
        memmove(dst + size, src + size, dstEndAlign);
    }
    size -= dstAlign;
    // 8A04
    if (size != 0) {
        s32 *alignedDst = dst + dstAlign;
        s32 *curDst = (void*)alignedDst + size - 64;
        s32 *curSrc = src + dstAlign + size - 64;
        // 8A28
        while (curDst >= alignedDst) {
            pspCache(0x18, curDst);
            curDst[ 0] = curSrc[ 0];
            curDst[ 1] = curSrc[ 1];
            curDst[ 2] = curSrc[ 2];
            curDst[ 3] = curSrc[ 3];
            curDst[ 4] = curSrc[ 4];
            curDst[ 5] = curSrc[ 5];
            curDst[ 6] = curSrc[ 6];
            curDst[ 7] = curSrc[ 7];
            curDst[ 8] = curSrc[ 8];
            curDst[ 9] = curSrc[ 9];
            curDst[10] = curSrc[10];
            curDst[11] = curSrc[11];
            curDst[12] = curSrc[12];
            curDst[13] = curSrc[13];
            curDst[14] = curSrc[14];
            curDst[15] = curSrc[15];
            curDst -= 16;
            curSrc -= 16;
        }
    }
    // 8ABC
    if (dstAlign != 0) {
        // 8AE4
        memmove(dst, src, dstAlign);
    }
    return dst;
}

void *sceKernelMemmoveWithFill(void *dst, void *src, u32 size, s32 fill)
{
    void *realDst = (void*)((u32)dst & 0x1FFFFFFF);
    void *realSrc = (void*)((u32)src & 0x1FFFFFFF);
    if (realDst == realSrc)
        return dst;
    u32 dist = realDst - realSrc;
    if ((dist & 3) != 0) {
        // 8ED0
        return memmove(dst, src, size);
    }
    if (((0x2C >> (((u32)dst >> 29) & 7)) & 1) != 0) {
        // 8ED0
        return memmove(dst, src, size);
    }
    if (dist + 63 < 126) {
        // 8ED0
        return memmove(dst, src, size);
    }
    u32 dstAlign = (((u32)dst / 64) * 64) - (u32)dst;
    void *dstEnd = dst + size;
    if (dstAlign >= size) {
        // 8EC0
        memmove(dst, src, size);
        return dst;
    }
    u32 dstEndAlign = (u32)dstEnd & 0x3F;
    if (realDst >= realSrc) {
        // 8D5C
        void *srcEnd = src + size;
        if (dstEndAlign != 0) {
            // 8E94
            memmove(dstEnd - dstEndAlign, srcEnd - dstEndAlign, dstEndAlign);
            sceKernelMemset(srcEnd - dstEndAlign, fill, dstEndAlign);
        }
        // 8D68
        if (size != dstAlign + dstEndAlign) {
            u32 alignedSize = size - dstAlign - dstEndAlign;
            s32 *curDst = dst + dstAlign + alignedSize - 64;
            s32 *curSrc = src + dstAlign + alignedSize - 64;
            // 8D94
            while ((void *)curDst >= dst + dstAlign) {
                pspCache(0x18, curDst);
                curDst[ 0] = curSrc[ 0];
                curDst[ 1] = curSrc[ 1];
                curDst[ 2] = curSrc[ 2];
                curDst[ 3] = curSrc[ 3];
                curDst[ 4] = curSrc[ 4];
                curDst[ 5] = curSrc[ 5];
                curDst[ 6] = curSrc[ 6];
                curDst[ 7] = curSrc[ 7];
                curDst[ 8] = curSrc[ 8];
                curDst[ 9] = curSrc[ 9];
                curDst[10] = curSrc[10];
                curDst[11] = curSrc[11];
                curDst[12] = curSrc[12];
                curDst[13] = curSrc[13];
                curDst[14] = curSrc[14];
                curDst[15] = curSrc[15];
                pspCache(0x18, curSrc);
                curSrc[ 0] = curSrc[ 1] = curSrc[ 2] = curSrc[ 3] = fill;
                curSrc[ 4] = curSrc[ 5] = curSrc[ 6] = curSrc[ 7] = fill;
                curSrc[ 8] = curSrc[ 9] = curSrc[10] = curSrc[11] = fill;
                curSrc[12] = curSrc[13] = curSrc[14] = curSrc[15] = fill;
                curDst -= 16;
                curSrc -= 16;
            }
        }
        // 8E6C
        if (dstAlign == 0)
            return dst;
        memmove(dst, src, dstAlign);
        sceKernelMemset(src, fill, dstAlign);
    } else {
        if (dstAlign != 0) {
            // 8D3C
            memmove(dst, src, dstAlign);
            sceKernelMemset(src, fill, dstAlign);
        }
        // 8BDC
        if (size != dstAlign + dstEndAlign) {
            s32 *curDst = dst + dstAlign;
            s32 *curSrc = src + dstAlign;
            // 8BFC
            while ((void *)curDst < dst + size - dstEndAlign) {
                pspCache(0x18, curDst);
                curDst[ 0] = curSrc[ 0];
                curDst[ 1] = curSrc[ 1];
                curDst[ 2] = curSrc[ 2];
                curDst[ 3] = curSrc[ 3];
                curDst[ 4] = curSrc[ 4];
                curDst[ 5] = curSrc[ 5];
                curDst[ 6] = curSrc[ 6];
                curDst[ 7] = curSrc[ 7];
                curDst[ 8] = curSrc[ 8];
                curDst[ 9] = curSrc[ 9];
                curDst[10] = curSrc[10];
                curDst[11] = curSrc[11];
                curDst[12] = curSrc[12];
                curDst[13] = curSrc[13];
                curDst[14] = curSrc[14];
                curDst[15] = curSrc[15];
                pspCache(0x18, curSrc);
                curSrc[ 0] = curSrc[ 1] = curSrc[ 2] = curSrc[ 3] = fill;
                curSrc[ 4] = curSrc[ 5] = curSrc[ 6] = curSrc[ 7] = fill;
                curSrc[ 8] = curSrc[ 9] = curSrc[10] = curSrc[11] = fill;
                curSrc[12] = curSrc[13] = curSrc[14] = curSrc[15] = fill;
                curDst += 16;
                curSrc += 16;
            }
        }
        // 8CD4
        if (dstEndAlign != 0) {
            // 8D0C
            memmove(dstEnd - dstEndAlign, src + size - dstEndAlign, dstEndAlign);
            sceKernelMemset(src + size - dstEndAlign, fill, dstEndAlign);
        }
    }
    return dst;
}

void *sceKernelFillBlock64(void *dst, s32 c, u32 size)
{
    void *end = dst + size;
    s32 *curDst = dst;
    // 8EEC
    while (curDst != end) {
        pspCache(0x18, curDst);
        curDst[ 0] = curDst[ 1] = curDst[ 2] = curDst[ 3] = c;
        curDst[ 4] = curDst[ 5] = curDst[ 6] = curDst[ 7] = c;
        curDst[ 8] = curDst[ 9] = curDst[10] = curDst[11] = c;
        curDst[12] = curDst[13] = curDst[14] = curDst[15] = c;
        curDst += 16;
    }
    return dst;
}

