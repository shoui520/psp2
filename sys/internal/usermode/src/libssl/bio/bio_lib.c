/* SSLeay BIO, with measured SSL-C changes in bio_lib_sce.patch. */
#include "bio.h"
#include "../memory.h"
#include "../runtime.h"
#include "../err/err.h"

#undef Malloc
#undef Free
#undef CRYPTO_add
#define Malloc(size) sslAllocate(size)
#define Free(ptr) sslFree(ptr)
#define CRYPTO_add(ptr, amount, index) \
	sslAddReference(ptr, amount, index, \
		"S:\\work\\MES\\common\\bio/bio_lib.c", 190)
#define BIOerr(function, reason) sslPushError(32, function, reason, 0, 0)
#define ERR_R_MALLOC_FAILURE 33

/* Descriptive names for SSL-C values not named upstream are ours. */
#define SCE_BIO_AUTO_FLUSH 16
#define SCE_BIO_BAD_REFERENCE_COUNT 42

#include "bio_lib_sce.inc"

STACK *sslGetBioExDataMethods(void)
{
	return bio_meth;
}

STACK *sslSetBioExDataMethods(STACK *methods)
{
	bio_meth = methods;
	return methods;
}
