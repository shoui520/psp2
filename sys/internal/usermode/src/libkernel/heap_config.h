/* SCE: host allocation is replaced by the native kernel memblock boundary. */
#include <stddef.h>
#define LACKS_SYS_TYPES_H 1
#define LACKS_SYS_PARAM_H 1
#define MSPACES 1
#define ONLY_MSPACES 1
#define HAVE_MORECORE 0
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define MMAP_CLEARS 0
#define USE_LOCKS 0
#define MALLOC_ALIGNMENT 16
#define MALLOC_FAILURE_ACTION
#define mspace_malloc sceClibMspaceMalloc
#define mspace_calloc sceClibMspaceCalloc
#define mspace_free sceClibMspaceFree
#define mspace_realloc sceClibMspaceRealloc
#define mspace_memalign sceClibMspaceMemalign
#define mspace_malloc_stats sceClibMspaceMallocStats
#define HeapAbort abort
