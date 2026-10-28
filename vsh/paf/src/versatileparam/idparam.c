#include "idparam.h"
#include "../hash/sha1.h"

uint32_t IDParamID2IDHash(IDParam *self, const PafString *id)
{
	SHA1Context ctx;
	uint8_t digest[SHA1HashSize];

	SHA1Reset(&ctx);
	SHA1Input(&ctx, (const uint8_t *)id->ptr, id->count);
	SHA1Result(&ctx, digest);
	return (uint32_t)digest[0] << 24 | (uint32_t)digest[1] << 16 |
	       (uint32_t)digest[2] << 8 | digest[3];
}
