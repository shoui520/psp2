#ifndef SYSMEM_H
#define SYSMEM_H

#include <sysmem_kernel.h>

/*

'->' means it uses a chained list ('->' for single-chained, '<->' for double-chained)

                    +-------+
          +---------| Heap  | A heap. Has an UID, name "SceSysmemHeap"
          |         +-------+ The main one is named "SceSystemMemoryManager". Allocated in a partition.
          |             |
          |             v
          |        +---------+   +-------+
          |        |  First  |   |       |
          |  ...<->|  Heap   |<->| Heap  |<->... SceSysmemHeapBlock (note: this may be the "Lowheap" from Sony)
          |        |  Block  |   | Block |       These blocks are created as soon as the others can't hold something else from _AllocHeapMemory()
          |        +---------+   +-------+
          |        | Lowheap |
          |        |  with   | SceSysmemLowheap
          |        | the info|
          |        +---------+
          |        | First   | SceSysmemLowheapBlock
          |    +-->| lowheap | (patterns may be different in complex cases)
          |    |   | block   |
          |    |   +---------+
          |    |        .
          |    |        .
          |    |        .
          |    |   +---------+
          |    |   | Last    |
          |    +-->| lowheap |
          |        | block   |
          |        +---------+
          |
          +---------+ (no pointer here, but partition id + address + size)
                    v
   +-----------+  +-----------+  +-----------+
   |           |  |           |  |           |
...| Partition |->| Partition |->| Partition |... partitions created through sceKernelCreateMemoryPartition() (SceSysmemMemoryPartition)
   |           |  |           |  |           |    Most created by the kernel. Has an UID, name "SceSysmemMemoryPartition"
   +-----------+  +-----------+  +-----------+
                    ^   |
    +---------------+   |
    |                   v
    |          +-----------------+   +---------------+
    |          |     (First)     |   |               |
    |    ...<->|  Control block  |<->| Control block |<-> ... SceSysmemCtlBlk
    |          |   30 segments   |   |  30 segments  |
    |          +-----------------+   +---------------+
    |                   |
    |                   v
    |        +---------------------+
    |        |                     |
    |        | 1 of the 30 segments| SceSysmemSeg
    |        |                     |
    |        +---------------------+
    |
    |       +------------------------+
    |       |                        |
    +-------|        Block           | blocks created through sceKernelAllocPartitionMemory() (SceSysmemMemoryBlock)
            | Only holds size & addr | Has an UID, name "SceSysMemMemoryBlock"
            +------------------------+ Some kind of "interface" to the SceSysmemSeg, without the internal information

*/

typedef struct {
    u32 addr;
    u32 size;
} SceSysmemPartInfo;

typedef struct {
    u32 memSize;
    u32 kernelPartMode;
    u32 userPartMode;
    SceSysmemPartInfo other1; // 12
    SceSysmemPartInfo other2; // 20
    SceSysmemPartInfo vshell; // 28
    SceSysmemPartInfo scUser; // 36
    SceSysmemPartInfo meUser; // 44
    SceSysmemPartInfo extSc2Kernel; // 52
    SceSysmemPartInfo extScKernel; // 60
    SceSysmemPartInfo extMeKernel; // 68
    SceSysmemPartInfo extVshell; // 76
} SceSysmemPartTable;

typedef struct {
    u32 addr; // 0
    u32 size; // 4
    u32 attr; // 8
    SceUID partId; // 12
    u32 pad[3]; // 16..27 (stride 28 bytes)
} SceMemoryProtect;

typedef struct {
    u32 headerSize; // 0
    u32 memAddr; // 4
    u32 kernelPartSize; // 8
    u32 kernelPartMode; // 12
    u32 userPartMode; // 16
    u32 model; // 20
    u32 memLayoutMode; // 24
    u32 bootParams[8]; // 28
    u32 dipsLo; // 60
    u32 dipsHi; // 64
    u32 cpTime; // 68
    u32 nBlocks; // 72
    SceMemoryProtect *memBlocks; // 76
    void (*debugPutchar)(short*, int); // 80
    u32 putcharParam; // 84
    SysMemThreadConfig *threadConf; // 88
    u32 threadConfParam; // 92
    u32 realMemSize; // 96
    u32 randomValue; // 100
} SysMemConfig;

s32 *wmemset(s32 *src, s32 c, u32 n);

#endif

