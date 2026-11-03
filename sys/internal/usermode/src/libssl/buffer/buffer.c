/* SSLeay buffer management with measured SSL-C changes in buffer_sce.patch. */
#include "buffer.h"
#include "../memory.h"

#define Malloc sslAllocate
#define Free sslFree
#define memset sslFillMemory
#include "buffer_sce.inc"
