/* SSLeay ex-data, with the measured SSL-C changes in ex_data_sce.patch. */
#include <stdio.h>
#include "crypto.h"
#include "memory.h"
#include "err/err.h"

#undef Malloc
#undef Free
#define Malloc(size) sslAllocate(size)
#define Free(ptr) sslFree(ptr)
#define MemCheck_off()
#define MemCheck_on()
#undef CRYPTO_F_CRYPTO_SET_EX_DATA
#define CRYPTO_F_CRYPTO_SET_EX_DATA 101
#define ERR_R_MALLOC_FAILURE 33
#define CRYPTOerr(function, reason) sslPushError(38, function, reason, 0, 0)

#include "ex_data_sce.inc"
