/* SSLeay 0.9.1b crypto/pem/pem_lib.c with the measured CRYPTOCME changes in pem_lib_sce.patch. */
#include <stdio.h>
#include "pem.h"
#include "../buffer/buffer.h"
#include "../evp/evp.h"
#include "../memory.h"
#include "../format.h"

#undef Malloc
#undef Free
#define Malloc(size) sslAllocCallback(size)
#define Free(ptr) sslFreeCallback(ptr)
#define strlen sslStringLength
#define strcmp sslCompareCString
#define strncmp sslCompareStrings
#define strncpy sslCopyString
#define BIO_write SceSslInternal_33AC7A16

#include "pem_lib_sce.inc"
