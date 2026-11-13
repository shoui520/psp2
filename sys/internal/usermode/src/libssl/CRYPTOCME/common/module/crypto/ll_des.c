/* Filename and descriptive names are ours: CRYPTOCME's triple DES in CBC
 * mode. The key schedule and the rounds are SSLeay 0.9.1b's libdes
 * (set_key.c, des_enc.c) as RSA changed them: each schedule names its
 * SPtrans table, the key is not checked, the rounds leave the rotation to
 * their caller, and decryption runs on schedules in reverse.
 */
#include "ll_ciph.h"
#include "../../object.h"

#define DES_LONG SceUInt32
#include "sk.h"
#include "spr.h"

#define ROTATE(a, n) (((a) >> (n)) | ((a) << (32 - (n))))

#define PERM_OP(a, b, t, n, m)						\
	((t) = ((((a) >> (n)) ^ (b)) & (m)), (b) ^= (t),		\
	 (a) ^= ((t) << (n)))

#define HPERM_OP(a, t, n, m)						\
	((t) = ((((a) << (16 - (n))) ^ (a)) & (m)),			\
	 (a) = (a) ^ (t) ^ (t >> (16 - (n))))

#define IP(l, r)							\
	{								\
		DES_LONG tt;						\
		PERM_OP(r, l, tt, 4, 0x0f0f0f0fL);			\
		PERM_OP(l, r, tt, 16, 0x0000ffffL);			\
		PERM_OP(r, l, tt, 2, 0x33333333L);			\
		PERM_OP(l, r, tt, 8, 0x00ff00ffL);			\
		PERM_OP(r, l, tt, 1, 0x55555555L);			\
	}

#define FP(l, r)							\
	{								\
		DES_LONG tt;						\
		PERM_OP(l, r, tt, 1, 0x55555555L);			\
		PERM_OP(r, l, tt, 8, 0x00ff00ffL);			\
		PERM_OP(l, r, tt, 2, 0x33333333L);			\
		PERM_OP(r, l, tt, 16, 0x0000ffffL);			\
		PERM_OP(l, r, tt, 4, 0x0f0f0f0fL);			\
	}

#define D_ENCRYPT(LL, R, S)						\
	{								\
		u = R ^ s[S];						\
		t = R ^ s[S + 1];					\
		t = ROTATE(t, 4);					\
		LL ^= sp[0][(u >> 2L) & 0x3f] ^			\
		      sp[2][(u >> 10L) & 0x3f] ^			\
		      sp[4][(u >> 18L) & 0x3f] ^			\
		      sp[6][(u >> 26L) & 0x3f] ^			\
		      sp[1][(t >> 2L) & 0x3f] ^				\
		      sp[3][(t >> 10L) & 0x3f] ^			\
		      sp[5][(t >> 18L) & 0x3f] ^			\
		      sp[7][(t >> 26L) & 0x3f];				\
	}

/* A DES key schedule */
typedef struct SslDesSchedule {
	const DES_LONG (*spTrans)[64];
	DES_LONG subkeys[16][2];
} SslDesSchedule;

/* A DES block as bytes */
typedef struct SslDesBlock {
	unsigned char bytes[8];
} SslDesBlock;

/* The state of triple DES in CBC mode */
typedef struct SslDes3CbcState {
	SslDesBlock iv;
	SslDesBlock initialIv;
	SceUInt32 reserved;
	SslDesSchedule schedule[3];
} SslDes3CbcState;

_Static_assert(sizeof(SslDes3CbcState) == 416, "SslDes3CbcState");

int sslDes3Encrypt(DES_LONG *data, const SslDesSchedule *schedule);
int sslSetDes3CbcKey(SslCipherCore *core, const void *key,
		     const unsigned char *iv, int flags);

static const SslCipherBlockFunction sslDes3CbcEngine[2] = {
	(SslCipherBlockFunction)sslDes3Encrypt,
	(SslCipherBlockFunction)sslDes3Encrypt
};

static const unsigned char sslDes3CbcOid[] = {
	0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x03, 0x07
};

/* The saved state: the key and the chaining value */
static const SceUInt32 sslDes3CbcStateSchema[] = {
	0x00406042, 0x02003003, 0x02002003, 0x0040c0da,
	0x004080ca, 0x0000a004, 0x00000000
};

static SslCipherDescriptor sslDes3CbcCipherDescriptor = {
	"des3-cbc C small",
	{ sizeof(sslDes3CbcOid), sslDes3CbcOid },
	24, 24, 24, 8, 8,
	sizeof(SslDes3CbcState), 0, 1,
	sslSetDes3CbcKey,
	sslProcessCbcCipher,
	NULL,
	NULL,
	SSL_CIPHER_SAVABLE | SSL_CIPHER_CHAINED,
	sslDes3CbcEngine,
	sslDes3CbcStateSchema,
	NULL
};

void sslReverseDesSchedule(SslDesSchedule *schedule)
{
	DES_LONG *a = schedule->subkeys[0];
	DES_LONG *b = schedule->subkeys[15];
	DES_LONG t0, t1;
	int i;

	for (i = 8; i != 0; i--) {
		t0 = a[0];
		a[0] = b[0];
		t1 = a[1];
		a[1] = b[1];
		a += 2;
		b[0] = t0;
		b[1] = t1;
		b -= 2;
	}
}

/* Turns encryption into decryption: E3(D2(E1)) runs as D1(E2(D3)) */
void sslReverseDes3Schedule(SslDesSchedule *schedule)
{
	DES_LONG *a, *b;
	DES_LONG t0, t1;
	int i;

	sslReverseDesSchedule(&schedule[1]);
	a = schedule[0].subkeys[0];
	b = schedule[2].subkeys[15];
	for (i = 16; i != 0; i--) {
		t0 = a[0];
		a[0] = b[0];
		t1 = a[1];
		a[1] = b[1];
		a += 2;
		b[0] = t0;
		b[1] = t1;
		b -= 2;
	}
}

int sslSetDesKey(SslDesSchedule *schedule, const unsigned char *key)
{
	static const int shifts2[16] = {
		0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0
	};
	DES_LONG c, d, t, s, t2;
	const unsigned char *in;
	DES_LONG *k;
	int i;

	schedule->spTrans = des_SPtrans;
	k = schedule->subkeys[0];
	in = key;

	c = (DES_LONG)in[0] | ((DES_LONG)in[1] << 8) |
	    ((DES_LONG)in[2] << 16) | ((DES_LONG)in[3] << 24);
	d = (DES_LONG)in[4] | ((DES_LONG)in[5] << 8) |
	    ((DES_LONG)in[6] << 16) | ((DES_LONG)in[7] << 24);

	PERM_OP(d, c, t, 4, 0x0f0f0f0fL);
	HPERM_OP(c, t, -2, 0xcccc0000L);
	HPERM_OP(d, t, -2, 0xcccc0000L);
	PERM_OP(d, c, t, 1, 0x55555555L);
	PERM_OP(c, d, t, 8, 0x00ff00ffL);
	PERM_OP(d, c, t, 1, 0x55555555L);
	d = (((d & 0x000000ffL) << 16L) | (d & 0x0000ff00L) |
	     ((d & 0x00ff0000L) >> 16L) | ((c & 0xf0000000L) >> 4L));
	c &= 0x0fffffffL;

	for (i = 0; i < 16; i++) {
		if (shifts2[i]) {
			c = ((c >> 2L) | (c << 26L));
			d = ((d >> 2L) | (d << 26L));
		} else {
			c = ((c >> 1L) | (c << 27L));
			d = ((d >> 1L) | (d << 27L));
		}
		c &= 0x0fffffffL;
		d &= 0x0fffffffL;
		s = des_skb[0][(c)&0x3f] |
		    des_skb[1][((c >> 6) & 0x03) | ((c >> 7L) & 0x3c)] |
		    des_skb[2][((c >> 13) & 0x0f) | ((c >> 14L) & 0x30)] |
		    des_skb[3][((c >> 20) & 0x01) | ((c >> 21L) & 0x06) |
			       ((c >> 22L) & 0x38)];
		t = des_skb[4][(d)&0x3f] |
		    des_skb[5][((d >> 7L) & 0x03) | ((d >> 8L) & 0x3c)] |
		    des_skb[6][(d >> 15L) & 0x3f] |
		    des_skb[7][((d >> 21L) & 0x0f) | ((d >> 22L) & 0x30)];

		t2 = ((t << 16L) | (s & 0x0000ffffL)) & 0xffffffffL;
		*(k++) = ROTATE(t2, 30) & 0xffffffffL;

		t2 = ((s >> 16L) | (t & 0xffff0000L));
		*(k++) = ROTATE(t2, 26) & 0xffffffffL;
	}
	return 0;
}

/* Two keys (k1, k2, k1) or three; the middle one decrypts */
int sslSetDes3Key(SslDesSchedule *schedule, const unsigned char *key,
		  SceUInt32 keyLength)
{
	const unsigned char *third;

	if (keyLength != 16 && keyLength != 24)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	third = key;
	if (keyLength != 16)
		third = key + 16;
	sslSetDesKey(&schedule[0], key);
	sslSetDesKey(&schedule[1], key + 8);
	sslReverseDesSchedule(&schedule[1]);
	sslSetDesKey(&schedule[2], third);
	return 0;
}

int sslSetDes3CbcKey(SslCipherCore *core, const void *key,
		     const unsigned char *iv, int flags)
{
	SslDes3CbcState *state = core->state;

	core->iv = state->iv.bytes;
	core->initialIv = state->initialIv.bytes;
	core->schedule = state->schedule;
	if (iv != NULL)
		state->initialIv = *(const SslDesBlock *)iv;
	if (!(flags & SSL_CIPHER_KEEP_IV)) {
		state->iv = state->initialIv;
		core->ivUsed = 0;
	}
	if (core->keyLength != 16 && core->keyLength != 24)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	if (key != NULL) {
		core->encrypt = 1;
		sslSetDes3Key(state->schedule, key, core->keyLength);
	}
	if (core->encrypt != (flags & SSL_CIPHER_ENCRYPT)) {
		core->encrypt = flags & SSL_CIPHER_ENCRYPT;
		sslReverseDes3Schedule(state->schedule);
	}
	return 0;
}

const SslCipherDescriptor *sslGetDes3CipherDescriptor(void)
{
	return &sslDes3CbcCipherDescriptor;
}

const SslCipherDescriptor *sslGetDesEde3CbcCipherDescriptor(void)
{
	return &sslDes3CbcCipherDescriptor;
}

/* Camellia is not built: its getter gives AES-256 */
const SslCipherDescriptor *sslGetAes256CipherDescriptor(void)
{
	return &sslAes256CbcCipherDescriptor;
}

const SslCipherDescriptor *sslGetAesCbcCipherDescriptor(void)
{
	return &sslAes256CbcCipherDescriptor;
}

const SslCipherDescriptor *sslGetCamelliaCbcCipherDescriptor(void)
{
	return &sslAes256CbcCipherDescriptor;
}

/* Sixteen rounds, four at a time */
int sslDesRounds(DES_LONG *data, const SslDesSchedule *schedule)
{
	const DES_LONG (*sp)[64] = schedule->spTrans;
	const DES_LONG *s = schedule->subkeys[0];
	DES_LONG l, r, t, u;
	int ret = 0;
	int i;

	r = data[0];
	l = data[1];
	for (i = 4; i != 0; i--) {
		D_ENCRYPT(l, r, 0);
		D_ENCRYPT(r, l, 2);
		D_ENCRYPT(l, r, 4);
		D_ENCRYPT(r, l, 6);
		s += 8;
	}
	data[0] = l;
	data[1] = r;
	return ret;
}

int sslDes3Encrypt(DES_LONG *data, const SslDesSchedule *schedule)
{
	DES_LONG l, r;

	l = data[0];
	r = data[1];
	IP(l, r);
	data[0] = ROTATE(l, 29);
	data[1] = ROTATE(r, 29);
	sslDesRounds(data, &schedule[0]);
	sslDesRounds(data, &schedule[1]);
	sslDesRounds(data, &schedule[2]);
	l = ROTATE(data[0], 3);
	r = ROTATE(data[1], 3);
	FP(r, l);
	data[0] = l;
	data[1] = r;
	return 0;
}
