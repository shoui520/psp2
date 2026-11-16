/*
 * md5_digest.c: MD5, L. Peter Deutsch's md5.c (contrib/libmd5-rfc-2002-04-13) built here
 * little-endian, its copies made by the library's own sceClibMemcpy; and the digest of a
 * buffer, which the PSARC dearchiver finds an archive's files by.
 */
#define ARCH_IS_BIG_ENDIAN	0
#define memcpy			sceClibMemcpy
#include <md5.c>
#undef memcpy

md5_byte_t *fiosMD5(const void *pData, int size, md5_byte_t *pDigest)
{
	md5_state_t state;

	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)pData, size);
	md5_finish(&state, pDigest);
	return pDigest;
}
