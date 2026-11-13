/* Filename and descriptive names are ours: CRYPTOCME's SHA-2 digests on the
 * SDK's block functions (libsha224, libsha256, libsha384, libsha512).
 */
#include <string.h>
#include <libsha224.h>
#include <libsha256.h>
#include <libsha384.h>
#include <libsha512.h>
#include "ll_obj.h"
#include "../../../../memory.h"

/* The outer state of a digest on the SDK's functions: nothing it uses */
typedef struct SslSdkDigestState {
	unsigned char reserved[4];
} SslSdkDigestState;

static SceUInt32 sslTransferSha256Context(SslDigestContext *context,
					  unsigned char *buffer,
					  SceUInt32 length, int save);
static SceUInt32 sslTransferSha512Context(SslDigestContext *context,
					  unsigned char *buffer,
					  SceUInt32 length, int save);

static const SslDigestCore sslSha256Core = {
	(int (*)(void *))sceSha256BlockInit,
	(int (*)(void *, const void *, SceUInt32))sceSha256BlockUpdate,
	(int (*)(void *, unsigned char *))sceSha256BlockResult,
	NULL,
	0,
	sizeof(SceSha256Context),
	0, 8, 4, 4,
	sslTransferSha256Context,
	0
};

/* nistAlgorithms hashAlgs 1 */
static const unsigned char sslSha256Oid[] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
};

static const SslDigestDescriptor sslSha256DigestDescriptor = {
	"sceSha256",
	{ sizeof(sslSha256Oid), sslSha256Oid },
	32, 0, 64,
	&sslSha256SdkImplementation,
	&sslSha256Core,
	NULL,
	2,
	1,
	0
};

static const SslDigestCore sslSha224Core = {
	(int (*)(void *))sceSha224BlockInit,
	(int (*)(void *, const void *, SceUInt32))sceSha256BlockUpdate,
	(int (*)(void *, unsigned char *))sceSha224BlockResult,
	NULL,
	0,
	sizeof(SceSha224Context),
	0, 8, 4, 4,
	sslTransferSha256Context,
	0
};

/* nistAlgorithms hashAlgs 4 */
static const unsigned char sslSha224Oid[] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04
};

static const SslDigestDescriptor sslSha224DigestDescriptor = {
	"sceSha224",
	{ sizeof(sslSha224Oid), sslSha224Oid },
	28, 0, 64,
	&sslSha256SdkImplementation,
	&sslSha224Core,
	NULL,
	2,
	1,
	0
};

static const SslDigestCore sslSha512Core = {
	(int (*)(void *))sceSha512BlockInit,
	(int (*)(void *, const void *, SceUInt32))sceSha512BlockUpdate,
	(int (*)(void *, unsigned char *))sceSha512BlockResult,
	NULL,
	0,
	sizeof(SceSha512Context),
	0, 8, 8, 4,
	sslTransferSha512Context,
	0
};

/* SCE: SHA-512 carries SHA-256's OID */
static const SslDigestDescriptor sslSha512DigestDescriptor = {
	"sceSha512",
	{ sizeof(sslSha256Oid), sslSha256Oid },
	64, 0, 128,
	&sslSha512SdkImplementation,
	&sslSha512Core,
	NULL,
	2,
	1,
	0
};

static const SslDigestCore sslSha384Core = {
	(int (*)(void *))sceSha384BlockInit,
	(int (*)(void *, const void *, SceUInt32))sceSha512BlockUpdate,
	(int (*)(void *, unsigned char *))sceSha384BlockResult,
	NULL,
	0,
	sizeof(SceSha384Context),
	0, 8, 8, 4,
	sslTransferSha512Context,
	0
};

/* nistAlgorithms hashAlgs 2 */
static const unsigned char sslSha384Oid[] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02
};

static const SslDigestDescriptor sslSha384DigestDescriptor = {
	"sceSha384",
	{ sizeof(sslSha384Oid), sslSha384Oid },
	48, 0, 128,
	&sslSha512SdkImplementation,
	&sslSha384Core,
	NULL,
	2,
	1,
	0
};

const SslDigestDescriptor *sslGetSha256SdkDigestDescriptor(void)
{
	return &sslSha256DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha256DigestDescriptor(void)
{
	return &sslSha256DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha224SdkDigestDescriptor(void)
{
	return &sslSha224DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha224DigestDescriptor(void)
{
	return &sslSha224DigestDescriptor;
}

/* The SDK context is the chaining values and the buffer, saved whole */
static SceUInt32 sslTransferSha256Context(SslDigestContext *context,
					  unsigned char *buffer,
					  SceUInt32 length, int save)
{
	if (buffer != NULL) {
		if (save)
			sslCopyMemory(buffer, context->innerState,
				      sizeof(SceSha256Context));
		else
			sslCopyMemory(context->innerState, buffer,
				      sizeof(SceSha256Context));
	}
	return sizeof(SceSha256Context);
}

SceUInt32 sslTransferSha256State(SslDigestContext *context,
				    unsigned char *buffer, SceUInt32 length,
				    int save)
{
	SslSdkDigestState *state;

	if (buffer != NULL) {
		state = context->state;
		if (save)
			memcpy(buffer, state, sizeof(*state));
		else
			memcpy(state, buffer, sizeof(*state));
	}
	return sizeof(SslSdkDigestState);
}

const SslDigestDescriptor *sslGetSha512SdkDigestDescriptor(void)
{
	return &sslSha512DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha512DigestDescriptor(void)
{
	return &sslSha512DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha384SdkDigestDescriptor(void)
{
	return &sslSha384DigestDescriptor;
}

const SslDigestDescriptor *sslGetSha384DigestDescriptor(void)
{
	return &sslSha384DigestDescriptor;
}

static SceUInt32 sslTransferSha512Context(SslDigestContext *context,
					  unsigned char *buffer,
					  SceUInt32 length, int save)
{
	if (buffer != NULL) {
		if (save)
			sslCopyMemory(buffer, context->innerState,
				      sizeof(SceSha512Context));
		else
			sslCopyMemory(context->innerState, buffer,
				      sizeof(SceSha512Context));
	}
	return sizeof(SceSha512Context);
}

SceUInt32 sslTransferSha512State(SslDigestContext *context,
				       unsigned char *buffer,
				       SceUInt32 length, int save)
{
	SslSdkDigestState *state;

	if (buffer != NULL) {
		state = context->state;
		if (save)
			memcpy(buffer, state, sizeof(*state));
		else
			memcpy(state, buffer, sizeof(*state));
	}
	return sizeof(SslSdkDigestState);
}
