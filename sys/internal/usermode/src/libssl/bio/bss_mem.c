/* SSLeay memory BIO with measured SSL-C changes in bss_mem_sce.patch. */
#include "bio.h"
#include "../buffer/buffer_dispatch.h"
#include "../string_inline.h"

/* SSL-C's inlined forms of the upstream retry operations. */
#define BIO_clear_retry_flags(b) do { \
	if (b) \
		(b)->flags &= ~(BIO_FLAGS_RWS | BIO_FLAGS_SHOULD_RETRY | \
				 SCE_BIO_RETRY_EXTENSION); \
} while (0)
#define BIO_set_retry_read(b) do { \
	if (b) \
		(b)->flags |= BIO_FLAGS_READ | BIO_FLAGS_SHOULD_RETRY; \
} while (0)

#define memcpy sslCopyMemory
#define memset sslFillMemory
#define strlen sslStringLengthInline
#include "bss_mem_sce.inc"

BIO *SceSslInternal_B8E6E249(void)
{
	return BIO_new(BIO_s_mem());
}
