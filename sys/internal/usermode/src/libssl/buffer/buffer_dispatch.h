/* Share the adapted upstream bodies with native inlined buffer operations.
 * The binding names are ours, not recovered Sony names.
 */
#ifndef LIBSSL_BUFFER_DISPATCH_H
#define LIBSSL_BUFFER_DISPATCH_H

#include "buffer.h"
#include "../memory.h"

#define SCE_BUF_DECL static inline
#define SCE_BUF_NO_STRDUP
#define BUF_MEM_new sslBufferNewInline
#define BUF_MEM_free sslBufferFreeInline
#define BUF_MEM_grow sslBufferGrowInline
#define Malloc sslAllocate
#define Free sslFree
#define memset sslFillMemory
#include "buffer_sce.inc"
#undef memset
#undef Free
#undef Malloc
#undef SCE_BUF_NO_STRDUP
#undef SCE_BUF_DECL

#endif
