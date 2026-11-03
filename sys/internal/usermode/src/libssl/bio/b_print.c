/* SSLeay BIO formatting with measured SSL-C changes in b_print_sce.patch. */
#include "bio.h"
#include "../memory.h"
#include "../format.h"

#define strlen sslStringLength
#include "b_print_sce.inc"
