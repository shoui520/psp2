/* SSLeay BIO dump with measured SSL-C changes in b_dump_sce.patch. */
#include "bio.h"
#include "../memory.h"
#include "../format.h"

#define strlen sslStringLength
#include "b_dump_sce.inc"
