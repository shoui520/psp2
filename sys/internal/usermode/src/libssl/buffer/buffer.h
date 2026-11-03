/* The original twelve-byte BUF_MEM agrees with the upstream layout. */
#ifndef LIBSSL_BUFFER_H
#define LIBSSL_BUFFER_H

#include <stddef.h>
#include <scetypes.h>
#include "buffer_upstream.h"

_Static_assert(sizeof(BUF_MEM) == 12, "BUF_MEM");
_Static_assert(offsetof(BUF_MEM, data) == 4, "data");
_Static_assert(offsetof(BUF_MEM, max) == 8, "max");

#endif
