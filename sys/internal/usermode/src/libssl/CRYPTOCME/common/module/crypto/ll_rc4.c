/* Filename and descriptive names are ours: CRYPTOCME's RC4, SSLeay 0.9.1b's
 * rc4_skey.c and rc4_enc.c as RSA changed them: the key schedule refuses an
 * empty key, and the stream runs a byte at a time.
 */
#include "ll_ciph.h"
#include "../../object.h"
#include "../../../../memory.h"

/* An RC4 key: the two indexes and the permutation */
typedef struct SslRc4Key {
	unsigned char x;
	unsigned char y;
	unsigned char data[256];
} SslRc4Key;

/* The state of RC4: the key in use, the schedule, then a copy of the key,
 * inline up to 16 bytes, longer keys in a larger state
 */
typedef struct SslRc4State {
	SslRc4Key *key;
	SslRc4Key schedule;
	unsigned char ready;
	unsigned char reserved;
	int keyCapacity;
	unsigned char keyData[16];
} SslRc4State;

_Static_assert(sizeof(SslRc4State) == 0x11c, "SslRc4State");

#define SSL_RC4_STATE_HEADER_SIZE 268

int sslSetRc4CipherKey(SslCipherCore *core, const void *key,
		       const unsigned char *iv, int flags);
void sslRc4Stream(void *key, SceUInt32 length, const unsigned char *input,
		  unsigned char *output);

static const unsigned char sslRc4Oid[] = {
	0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x03, 0x04
};

/* The saved state: the key and the permutation */
static const SceUInt32 sslRc4StateSchema[] = {
	0x00406042, 0x004080ca, 0x0000a004, 0x00404005,
	0x00401202, 0x00401602, 0x00441a02, 0x40001a01,
	0x00000000
};

static SslCipherDescriptor sslRc4CipherDescriptor = {
	"rc4 C small",
	{ sizeof(sslRc4Oid), sslRc4Oid },
	255, 16, 1, 1, 0,
	sizeof(SslRc4State), 0, 1,
	sslSetRc4CipherKey,
	sslProcessStreamCipher,
	NULL,
	NULL,
	SSL_CIPHER_SAVABLE,
	(const void *)sslRc4Stream,
	sslRc4StateSchema,
	NULL
};

#define SK_LOOP(n)							\
	{								\
		tmp = d[(n)];						\
		id2 = (data[id1] + tmp + id2) & 0xff;			\
		if (++id1 == length)					\
			id1 = 0;					\
		d[(n)] = d[id2];					\
		d[id2] = tmp;						\
	}

int sslSetRc4Key(SslRc4Key *key, int length, const unsigned char *data)
{
	unsigned char *d = key->data;
	unsigned int tmp;
	int id1, id2;
	int i;

	key->x = 0;
	key->y = 0;
	for (i = 0; i < 256; i++)
		d[i] = i;
	if (length == 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	id1 = id2 = 0;
	for (i = 0; i < 256; i += 4) {
		SK_LOOP(i + 0);
		SK_LOOP(i + 1);
		SK_LOOP(i + 2);
		SK_LOOP(i + 3);
	}
	return 0;
}

int sslSetRc4CipherKey(SslCipherCore *core, const void *key,
		       const unsigned char *iv, int flags)
{
	SslRc4State *state = core->state;
	int keep = flags & SSL_CIPHER_KEEP_IV;
	int keyLength = core->keyLength;
	SslRc4State *larger;
	void *memory;
	SceUInt32 coreFlags;
	int ret;

	if (keep && key == NULL)
		key = core->savedKey;
	if (keyLength > 256 || keyLength <= 0)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	if (key != NULL) {
		if (keyLength > 16 && state->keyCapacity < keyLength) {
			ret = sslAllocateDirectMemory(
				&memory,
				keyLength + SSL_RC4_STATE_HEADER_SIZE,
				core->dmem, 256);
			if (ret != 0)
				return ret;
			larger = memory;
			larger->keyCapacity = keyLength;
			larger->key = &larger->schedule;
			if (keep)
				sslCopyMemory(&larger->schedule,
					      &state->schedule,
					      sizeof(SslRc4Key) + 1);
			coreFlags = core->flags;
			if (coreFlags & SSL_CIPHER_CORE_STATE_ALLOCATED) {
				sslFreeDirectMemory(state, core->dmem);
				coreFlags = core->flags;
			}
			core->state = larger;
			state = larger;
			core->flags = coreFlags |
				      SSL_CIPHER_CORE_STATE_ALLOCATED;
		}
		sslCopyMemory(state->keyData, key, keyLength);
	}
	state->key = &state->schedule;
	core->schedule = &state->schedule;
	state->ready = 1;
	if (!keep)
		sslSetRc4Key(&state->schedule, keyLength, state->keyData);
	return 0;
}

const SslCipherDescriptor *sslGetRc4StreamCipherDescriptor(void)
{
	return &sslRc4CipherDescriptor;
}

const SslCipherDescriptor *sslGetRc4CipherDescriptor(void)
{
	return &sslRc4CipherDescriptor;
}

void sslRc4Stream(void *key, SceUInt32 length, const unsigned char *input,
		  unsigned char *output)
{
	SslRc4Key *rc4 = key;
	unsigned char *d = rc4->data;
	unsigned int x, y, tx, ty;

	if (length == 0)
		return;
	y = rc4->y;
	x = (rc4->x + 1) & 0xff;
	tx = d[x];
	do {
		unsigned char in = *input++;

		y = (tx + y) & 0xff;
		ty = d[y];
		d[x] = ty;
		d[y] = tx;
		ty = d[(tx + ty) & 0xff];
		x = (x + 1) & 0xff;
		tx = d[x];
		*output++ = in ^ ty;
	} while (--length != 0);
	rc4->y = y;
	rc4->x = x - 1;
}
