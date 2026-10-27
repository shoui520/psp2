#include "hmac_internal.h"

HMAC_ALGORITHM(hmacSha256Algorithm, 256, sceSha256Digest, sceSha256BlockInit,
	       sceSha256BlockUpdate, sceSha256BlockResult);
HMAC_ALGORITHM(hmacSha224Algorithm, 224, sceSha224Digest, sceSha224BlockInit,
	       sceSha224BlockUpdate, sceSha224BlockResult);
HMAC_ALGORITHM(hmacSha1Algorithm, 1, sceSha1Digest, sceSha1BlockInit,
	       sceSha1BlockUpdate, sceSha1BlockResult);
