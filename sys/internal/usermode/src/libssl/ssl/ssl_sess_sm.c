/* Native filename recovered from SSL-C's session-release reference record. */
#include "ssl_locl.h"
#include "ssl_context.h"
#include "ssl_ext.h"
#include "../memory.h"
#include "../runtime.h"
#include "../time.h"
#include <libssl/object.h>

#define Free sslFree
#define memset sslFillMemory
#define CRYPTO_add(value, amount, index) \
	sslAddReference(value, amount, index, \
		"S:\\work\\MES\\sslc\\ssl/ssl_sess_sm.c", 141)
#define X509_free SceSslInternal_144FA354

#include "ssl_sess_sm_sce.inc"
