/* SSLeay buffering BIO with the measured SSL-C changes in bf_buff_sce.patch. */
#include "bio.h"
#include "../memory.h"
#include "../string_inline.h"

/* SSL-C's inlined form of the upstream retry operation. */
#define BIO_clear_retry_flags(b) do { \
	if (b) \
		(b)->flags &= ~(BIO_FLAGS_RWS | BIO_FLAGS_SHOULD_RETRY | \
				 SCE_BIO_RETRY_EXTENSION); \
} while (0)

#define Malloc(size) sslAllocate(size)
#define Free(ptr) sslFree(ptr)
#define memcpy sslCopyMemory
#define strlen sslStringLengthInline
#include "bf_buff_sce.inc"
