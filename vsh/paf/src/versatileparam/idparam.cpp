/* paf::IDParam: an ID's hash is the first four bytes of its name's SHA-1, big-endian. */
#include <stdint.h>
#include <versatileparam/idparam.h>
extern "C" {
#include "../hash/sha1.h"
}

namespace paf {

uint32_t IDParam::ID2IDHash(const string &id)
{
	SHA1Context ctx;
	uint8_t digest[SHA1HashSize];

	SHA1Reset(&ctx);
	SHA1Input(&ctx, (const uint8_t *)id.c_str(), id.size());
	SHA1Result(&ctx, digest);
	return (uint32_t)digest[0] << 24 | (uint32_t)digest[1] << 16 |
	       (uint32_t)digest[2] << 8 | digest[3];
}

}
