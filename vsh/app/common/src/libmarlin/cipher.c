/*
 * The library's block cipher: AES in ECB, CBC and a counter mode, with
 * the padding schemes a cipher is created with (0x81052f70-). The file's
 * name is ours.
 */

#include "mln_internal.h"

/* the parts of a cipher's algorithm word (MlnImpl_81054880) */
#define MLN_CIPHER_AES			1
#define MLN_CIPHER_KEY_ENCRYPT		0x100
#define MLN_CIPHER_KEY_DECRYPT		0x200
#define MLN_CIPHER_KEY_COUNTER		0x300
#define MLN_CIPHER_MODE_ECB		0x10000
#define MLN_CIPHER_MODE_CBC		0x20000
#define MLN_CIPHER_MODE_CTR		0x30000
#define MLN_CIPHER_PAD_NONE		0x1000000
#define MLN_CIPHER_PAD_PKCS		0x2000000
#define MLN_CIPHER_PAD_ZERO		0x3000000
#define MLN_CIPHER_PAD_SPACE		0x4000000

typedef struct MlnCipher {
	int algorithm;
	int keyType;
	int mode;
	int padding;
	unsigned int blockSize;
	unsigned char iv[16];
	unsigned int ivSize;
	unsigned char nonce[8];
	void *key;
} MlnCipher;

typedef int (*MlnCipherBlockFunc)(void *key, const unsigned char *in, unsigned char *out);
typedef unsigned int (*MlnCipherModeFunc)(MlnCipher *cipher, void *key, const unsigned char *in,
					  unsigned int size, unsigned char *out,
					  unsigned int *outSize, const unsigned char *last,
					  unsigned int lastSize);
typedef unsigned int (*MlnCipherDecryptFunc)(MlnCipher *cipher, void *key, const unsigned char *in,
					     unsigned int size, unsigned char *out,
					     unsigned int *outSize);

int MlnImpl_8105528C(void *key, const unsigned char *in, unsigned char *out);
int MlnImpl_810552CC(void *key, const unsigned char *in, unsigned char *out);
unsigned int MlnImpl_81052F70(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize,
			      const unsigned char *last, unsigned int lastSize);
unsigned int MlnImpl_81053120(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize,
			      const unsigned char *last, unsigned int lastSize);
unsigned int MlnImpl_810539FC(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize);
unsigned int MlnImpl_81053C14(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize,
			      const unsigned char *last, unsigned int lastSize);
unsigned int MlnImpl_81053E60(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize);
unsigned int MlnImpl_810544EC(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize);

/* each algorithm's block functions and each mode's, by algorithm and by mode >> 16 */
static const struct {
	MlnCipherBlockFunc encryptBlock[2];
	MlnCipherBlockFunc decryptBlock[2];
	MlnCipherModeFunc encrypt[4];
	MlnCipherDecryptFunc decrypt[4];
} s_cipherFuncs = {
	{ NULL, MlnImpl_8105528C },
	{ NULL, MlnImpl_810552CC },
	{ NULL, MlnImpl_81052F70, MlnImpl_81053C14, MlnImpl_81053120 },
	{ NULL, MlnImpl_810539FC, MlnImpl_810544EC, MlnImpl_81053E60 },
};

unsigned int MlnImpl_81054CD4(const unsigned char *iv, unsigned int ivSize,
			      const unsigned char *nonce, unsigned char *counter);
__attribute__((noinline)) unsigned int MlnImpl_810551D8(void **key, int type, unsigned int keySize,
						 const void *keyData);
__attribute__((noinline)) unsigned int MlnImpl_81055274(void *key);

/* how many bytes of padding end the last block; nonzero when the padding is wrong */
static inline __attribute__((always_inline)) int MlnCipherUnpad(const MlnCipher *cipher,
							       const unsigned char *block,
							       unsigned int *padLength)
{
	unsigned int n = cipher->blockSize;
	unsigned int length;

	if (cipher->padding == MLN_CIPHER_PAD_ZERO) {
		if (n == 0)
			return -1;
		length = 0;
		if (block[n - 1] == 0) {
			for (length = 1; length != n; length++)
				if (block[n - 1 - length] != 0)
					break;
			if (length == n)
				return -1;
		}
	} else if (cipher->padding == MLN_CIPHER_PAD_SPACE) {
		if (n == 0)
			return -1;
		length = 0;
		if (block[n - 1] == ' ') {
			for (length = 1; length != n; length++)
				if (block[n - 1 - length] != ' ')
					break;
			if (length == n)
				return -1;
		}
	} else if (cipher->padding == MLN_CIPHER_PAD_PKCS) {
		length = block[n - 1];
		if (length == 0 || length > n)
			return -1;
	} else {
		length = 0;
	}
	*padLength = length;
	return 0;
}

/* ECB: each whole block of the input, then the padded last block */
unsigned int MlnImpl_81052F70(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize,
			      const unsigned char *last, unsigned int lastSize)
{
	unsigned int blockSize = cipher->blockSize;
	unsigned int total = lastSize + size;
	unsigned int blocks;
	unsigned int i;

	if (*outSize < total)
		return 3;
	blocks = size / blockSize;
	for (i = 0; i < blocks; i++) {
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, in, out) != 0)
			return 0x22030003;
		in += blockSize;
		out += blockSize;
	}
	if (lastSize != 0 &&
	    s_cipherFuncs.encryptBlock[cipher->algorithm](key, last, out) != 0)
		return 0x22030003;
	*outSize = total;
	return 0;
}

/* adds one to the big-endian 64-bit counter at the end of a block */
unsigned int MlnImpl_8105306C(unsigned char *block, unsigned int size)
{
	unsigned int low;
	unsigned int high;

	low = block[size - 4] << 24 | block[size - 3] << 16 | block[size - 1] |
	      block[size - 2] << 8;
	if (low + 1 < low) {
		high = (block[size - 8] << 24 | block[size - 7] << 16 | block[size - 5] |
			block[size - 6] << 8) + 1;
		block[size - 8] = high >> 24;
		block[size - 7] = high >> 16;
		block[size - 6] = high >> 8;
		block[size - 5] = high;
	}
	low++;
	block[size - 4] = low >> 24;
	block[size - 3] = low >> 16;
	block[size - 2] = low >> 8;
	block[size - 1] = low;
	return 0;
}

/*
 * The counter mode: the counter is the IV shifted right by four bits, its first eight bytes
 * mixed with the nonce, and the IV's last four bits say where in the first block of key
 * stream the data starts. The input's whole blocks come from in, the rest from last.
 */
unsigned int MlnImpl_81053120(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize,
			      const unsigned char *last, unsigned int lastSize)
{
	unsigned char counter[16];
	unsigned char stream[16];
	unsigned int ivSize = cipher->ivSize;
	unsigned int blockSize = cipher->blockSize;
	unsigned char tail = cipher->iv[ivSize - 1];
	unsigned int offset;
	unsigned int blocks;
	unsigned int rest;
	unsigned int done;
	unsigned int count;
	unsigned int i;
	unsigned int n;
	unsigned int ret;

	if (size > *outSize)
		return 3;
	MlnImpl_81054CD4(cipher->iv, cipher->ivSize, cipher->nonce, counter);
	offset = tail & 0xf;
	blocks = size / blockSize;
	done = 0;

	if (blocks != 0) {
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
			return 0x22030003;
		if (size < blockSize - offset) {
			for (i = offset; i < size + offset; i++)
				out[i - offset] = in[i - offset] ^ stream[i];
		} else {
			for (i = offset; i < blockSize; i++)
				out[i - offset] = in[i - offset] ^ stream[i];
		}
		done = i - offset;
		for (count = 1; count < blocks; count++) {
			ret = MlnImpl_8105306C(counter, blockSize);
			if (ret != 0)
				return ret;
			if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
				return 0x22030003;
			for (i = 0; i < blockSize; i++)
				out[done + i] = in[done + i] ^ stream[i];
			done += blockSize;
		}
	}
	if (blocks != 0 && offset != 0) {
		ret = MlnImpl_8105306C(counter, blockSize);
		if (ret != 0)
			return ret;
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
			return 0x22030003;
		for (i = 0; i < offset; i++)
			out[done + i] = in[done + i] ^ stream[i];
		done += offset;
	}

	rest = size - blocks * blockSize;
	if (rest != 0) {
		if (blocks == 0 &&
		    s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
			return 0x22030003;
		n = blockSize - offset;
		if (rest < n) {
			for (i = 0; i < rest; i++)
				out[done + i] = last[i] ^ stream[offset + i];
		} else {
			for (i = 0; i < n; i++)
				out[done + i] = last[i] ^ stream[offset + i];
			done += n;
			ret = MlnImpl_8105306C(counter, blockSize);
			if (ret != 0)
				return ret;
			if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
				return 0x22030003;
			for (i = n; i < rest; i++)
				out[done + i - n] = last[i] ^ stream[i - n];
		}
	}
	*outSize = size;
	return 0;
}

/* ECB: each block of the input; the padding is taken off the last */
unsigned int MlnImpl_810539FC(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize)
{
	unsigned char block[16];
	unsigned int blockSize = cipher->blockSize;
	unsigned int blocks;
	unsigned int whole;
	unsigned int total;
	unsigned int padLength;
	unsigned int n;
	unsigned int i;

	if (size % blockSize != 0)
		return 3;
	blocks = size / blockSize - 1;
	whole = blockSize * blocks;
	if (*outSize < whole)
		return 3;
	for (i = 0; i < blocks; i++) {
		if (s_cipherFuncs.decryptBlock[cipher->algorithm](key, in, out) != 0)
			return 0x22030004;
		in += blockSize;
		out += blockSize;
	}
	if (s_cipherFuncs.decryptBlock[cipher->algorithm](key, in, block) != 0)
		return 0x22030004;

	if (MlnCipherUnpad(cipher, block, &padLength) != 0)
		return 0x22030004;
	n = blockSize - padLength;
	if (n == 0) {
		total = whole;
	} else {
		total = n + whole;
		if (*outSize < total)
			return 3;
		MlnMemcpy(out, block, n);
	}
	*outSize = total;
	return 0;
}

/* CBC: each whole block of the input mixed with the one before (the IV first), then the padded last block */
unsigned int MlnImpl_81053C14(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize,
			      const unsigned char *last, unsigned int lastSize)
{
	unsigned char chain[16];
	unsigned int blockSize = cipher->blockSize;
	unsigned int total = lastSize + size;
	unsigned int blocks;
	unsigned int i;
	unsigned int k;

	if (*outSize < total)
		return 3;
	blocks = size / blockSize;
	MlnMemcpy(chain, cipher->iv, blockSize);
	for (i = 0; i < blocks; i++) {
		for (k = 0; k < blockSize; k++)
			chain[k] ^= in[k];
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, chain, out) != 0)
			return 0x22030003;
		MlnMemcpy(chain, out, blockSize);
		in += blockSize;
		out += blockSize;
	}
	if (lastSize != 0) {
		for (k = 0; k < blockSize; k++)
			chain[k] ^= last[k];
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, chain, out) != 0)
			return 0x22030003;
	}
	*outSize = total;
	return 0;
}

/* the counter mode's decryption: as encryption, every byte of the input coming from in */
unsigned int MlnImpl_81053E60(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize)
{
	unsigned char counter[16];
	unsigned char stream[16];
	unsigned int ivSize = cipher->ivSize;
	unsigned int blockSize = cipher->blockSize;
	unsigned char tail = cipher->iv[ivSize - 1];
	unsigned int offset;
	unsigned int blocks;
	unsigned int rest;
	unsigned int done;
	unsigned int count;
	unsigned int i;
	unsigned int ret;

	if (*outSize < size)
		return 3;
	offset = tail & 0xf;
	if (blockSize > size) {
		blocks = 0;
		if (size < blockSize - offset)
			rest = 0;
		else
			rest = offset - blockSize + size;
	} else {
		blocks = (offset - blockSize + size) / blockSize + 1;
		rest = size + offset - blockSize * blocks;
	}
	MlnImpl_81054CD4(cipher->iv, cipher->ivSize, cipher->nonce, counter);

	if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
		return 0x22030003;
	if (size < blockSize - offset) {
		for (i = offset; i < size + offset; i++)
			out[i - offset] = in[i - offset] ^ stream[i];
	} else {
		for (i = offset; i < blockSize; i++)
			out[i - offset] = in[i - offset] ^ stream[i];
	}
	done = i - offset;
	for (count = 1; count < blocks; count++) {
		ret = MlnImpl_8105306C(counter, blockSize);
		if (ret != 0)
			return ret;
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
			return 0x22030003;
		for (i = 0; i < blockSize; i++)
			out[done + i] = in[done + i] ^ stream[i];
		done += blockSize;
	}
	if (rest != 0) {
		ret = MlnImpl_8105306C(counter, blockSize);
		if (ret != 0)
			return ret;
		if (s_cipherFuncs.encryptBlock[cipher->algorithm](key, counter, stream) != 0)
			return 0x22030003;
		for (i = 0; i < rest; i++)
			out[done + i] = in[done + i] ^ stream[i];
	}
	*outSize = size;
	return 0;
}

/* CBC: each block of the input mixed with the block before it (the IV first); the padding is taken off the last */
unsigned int MlnImpl_810544EC(MlnCipher *cipher, void *key, const unsigned char *in,
			      unsigned int size, unsigned char *out, unsigned int *outSize)
{
	unsigned char block[16];
	unsigned char chain[16];
	unsigned int blockSize = cipher->blockSize;
	unsigned int blocks;
	unsigned int whole;
	unsigned int total;
	unsigned int padLength;
	unsigned int n;
	unsigned int i;
	unsigned int k;

	if (size % blockSize != 0)
		return 3;
	blocks = size / blockSize - 1;
	whole = blockSize * blocks;
	if (*outSize < whole)
		return 3;
	MlnMemcpy(chain, cipher->iv, blockSize);
	for (i = 0; i < blocks; i++) {
		if (s_cipherFuncs.decryptBlock[cipher->algorithm](key, in, out) != 0)
			return 0x22030004;
		for (k = 0; k < blockSize; k++)
			out[k] ^= chain[k];
		MlnMemcpy(chain, in, blockSize);
		in += blockSize;
		out += blockSize;
	}
	if (s_cipherFuncs.decryptBlock[cipher->algorithm](key, in, block) != 0)
		return 0x22030004;
	for (k = 0; k < blockSize; k++)
		block[k] ^= chain[k];
	if (MlnCipherUnpad(cipher, block, &padLength) != 0)
		return 0x22030004;

	n = blockSize - padLength;
	if (n == 0) {
		total = whole;
	} else {
		total = n + whole;
		if (*outSize < total)
			return 3;
		MlnMemcpy(out, block, n);
	}
	*outSize = total;
	return 0;
}


static inline __attribute__((always_inline)) int MlnCipherModeValid(int mode)
{
	switch (mode) {
	case MLN_CIPHER_MODE_ECB:
	case MLN_CIPHER_MODE_CBC:
	case MLN_CIPHER_MODE_CTR:
		return 1;
	}
	return 0;
}

static inline __attribute__((always_inline)) int MlnCipherPaddingValid(int padding)
{
	switch (padding) {
	case MLN_CIPHER_PAD_NONE:
	case MLN_CIPHER_PAD_PKCS:
	case MLN_CIPHER_PAD_ZERO:
	case MLN_CIPHER_PAD_SPACE:
		return 1;
	}
	return 0;
}

/*
 * Creates a cipher from its algorithm word (algorithm, key type, mode and padding, a byte
 * each from the lowest), its IV, its nonce and its key.
 */
__attribute__((noinline))
unsigned int MlnImpl_81054880(void **out, int algorithm, const void *iv, unsigned int ivSize,
			      const void *nonce, const void *keyData, unsigned int keySize)
{
	MlnCipher *cipher;
	void *key;
	int part;
	unsigned int ret;
	unsigned int r;

	*out = NULL;
	cipher = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*cipher));
	if (cipher == NULL) {
		ret = 2;
		goto fail;
	}
	MlnMemset(cipher, 0, sizeof(*cipher));
	if ((algorithm & 0xff) != MLN_CIPHER_AES)
		goto invalid;
	cipher->algorithm = algorithm & 0xff;
	cipher->blockSize = 16;
	part = algorithm & 0xff00;
	switch (part) {
	case MLN_CIPHER_KEY_ENCRYPT:
	case MLN_CIPHER_KEY_DECRYPT:
	case MLN_CIPHER_KEY_COUNTER:
		cipher->keyType = part;
		break;
	default:
		goto invalid;
	}
	part = algorithm & 0xff0000;
	switch (part) {
	case MLN_CIPHER_MODE_ECB:
	case MLN_CIPHER_MODE_CBC:
		cipher->mode = part;
		break;
	case MLN_CIPHER_MODE_CTR:
		cipher->mode = part;
		if (cipher->keyType != MLN_CIPHER_KEY_COUNTER)
			goto invalid;
		break;
	default:
		goto invalid;
	}
	part = algorithm & 0xff000000;
	if (!MlnCipherPaddingValid(part))
		goto invalid;
	cipher->padding = part;

	if (iv == NULL) {
		MlnMemset(cipher->iv, 0, sizeof(cipher->iv));
		cipher->ivSize = 0;
	} else {
		if (ivSize > sizeof(cipher->iv)) {
			ret = 3;
			goto fail;
		}
		MlnMemcpy(cipher->iv, iv, ivSize);
		cipher->ivSize = ivSize;
	}
	if (nonce == NULL)
		MlnMemset(cipher->nonce, 0, sizeof(cipher->nonce));
	else
		MlnMemcpy(cipher->nonce, nonce, sizeof(cipher->nonce));

	key = NULL;
	if (cipher->algorithm != MLN_CIPHER_AES)
		goto invalid;
	r = MlnImpl_810551D8(&key,
			     (cipher->keyType & ~MLN_CIPHER_KEY_DECRYPT) == MLN_CIPHER_KEY_ENCRYPT ? 1 : 2,
			     keySize, keyData);
	if (r != 0) {
		ret = (r >> 16) != 0 ? 0x22030005 : r;
		if (key != NULL && cipher->algorithm == MLN_CIPHER_AES)
			MlnImpl_81055274(key);
		goto fail;
	}
	cipher->key = key;
	*out = cipher;
	return 0;

invalid:
	ret = 0x22030001;
fail:
	if (*out == NULL)
		MlnFree(MLN_HEAP_GENERAL, cipher);
	return ret;
}

unsigned int MlnImpl_81054A90(void *handle)
{
	MlnCipher *cipher = handle;

	if (cipher != NULL) {
		if (cipher->algorithm == MLN_CIPHER_AES)
			MlnImpl_81055274(cipher->key);
		MlnFree(MLN_HEAP_GENERAL, cipher);
	}
	return 0;
}

/* encrypts: the input's last partial block is padded as the cipher says */
__attribute__((noinline))
unsigned int MlnImpl_81054AC8(void *handle, const void *data, unsigned int size, void *out,
			      unsigned int *outSize)
{
	MlnCipher *cipher = handle;
	const unsigned char *in = data;
	unsigned char last[16];
	void *key = cipher->key;
	unsigned int blockSize;
	unsigned int whole;
	unsigned int rest;
	unsigned int fill;
	signed char value;

	if (cipher->algorithm != MLN_CIPHER_AES || !MlnCipherModeValid(cipher->mode) ||
	    !MlnCipherPaddingValid(cipher->padding))
		return 0x22030001;
	blockSize = cipher->blockSize;
	whole = size / blockSize * blockSize;
	rest = size - whole;
	if (rest != 0)
		MlnMemcpy(last, in + whole, rest);
	if (cipher->padding == MLN_CIPHER_PAD_ZERO) {
		fill = rest != 0 ? blockSize - rest : 0;
		value = 0;
	} else if (cipher->padding == MLN_CIPHER_PAD_SPACE) {
		fill = rest != 0 ? blockSize - rest : 0;
		value = ' ';
	} else if (cipher->padding == MLN_CIPHER_PAD_PKCS) {
		fill = blockSize - rest;
		value = fill;
	} else {
		fill = 0;
	}
	if (fill != 0)
		MlnMemset(last + rest, value, fill);
	return s_cipherFuncs.encrypt[cipher->mode >> 16](cipher, key, in, size, out, outSize, last,
							 fill);
}

/* decrypts, taking off the padding */
unsigned int MlnImpl_81054C2C(void *handle, const void *in, unsigned int size, void *out,
			      unsigned int *outSize)
{
	MlnCipher *cipher = handle;
	void *key = cipher->key;

	if (cipher->algorithm != MLN_CIPHER_AES || !MlnCipherModeValid(cipher->mode) ||
	    !MlnCipherPaddingValid(cipher->padding))
		return 0x22030001;
	return s_cipherFuncs.decrypt[cipher->mode >> 16](cipher, key, in, size, out, outSize);
}

/* the counter mode's first counter: the IV shifted right by four bits, mixed with the nonce */
unsigned int MlnImpl_81054CD4(const unsigned char *iv, unsigned int ivSize,
			      const unsigned char *nonce, unsigned char *counter)
{
	unsigned char *start;
	unsigned int i;

	MlnMemset(counter, 0, 16);
	start = counter + 16 - ivSize;
	start[0] = iv[0] >> 4;
	for (i = 1; i < ivSize; i++)
		start[i] = iv[i - 1] << 4 | iv[i] >> 4;
	for (i = 0; i < 8; i++)
		counter[i] ^= nonce[i];
	return 0;
}

/* encrypts with AES-128-CBC and PKCS padding in one call */
unsigned int MlnImpl_81054EDC(const void *iv, const void *key, const void *in, unsigned int size,
			      void *out, unsigned int *outSize)
{
	void *cipher = NULL;
	unsigned int ret;

	ret = MlnImpl_81054880(&cipher, MLN_CIPHER_AES | MLN_CIPHER_KEY_ENCRYPT | MLN_CIPHER_MODE_CBC |
					MLN_CIPHER_PAD_PKCS, iv, 16, NULL, key, 16);
	if (ret == 0)
		ret = MlnImpl_81054AC8(cipher, in, size, out, outSize);
	MlnImpl_81054A90(cipher);
	return ret;
}

/* decrypts with AES-128-CBC and PKCS padding in one call */
unsigned int MlnImpl_81054F84(const void *iv, const void *key, const void *in, unsigned int size,
			      void *out, unsigned int *outSize)
{
	void *cipher = NULL;
	unsigned int ret;

	ret = MlnImpl_81054880(&cipher, MLN_CIPHER_AES | MLN_CIPHER_KEY_DECRYPT |
					MLN_CIPHER_MODE_CBC | MLN_CIPHER_PAD_PKCS, iv, 16, NULL,
			       key, 16);
	if (ret == 0)
		ret = MlnImpl_81054C2C(cipher, in, size, out, outSize);
	MlnImpl_81054A90(cipher);
	return ret;
}

/* decrypts one AES-128 block */
unsigned int MlnImpl_810550B0(const void *key, const void *in, void *out)
{
	void *cipher = NULL;
	unsigned int size = 16;
	unsigned int ret;

	ret = MlnImpl_81054880(&cipher, MLN_CIPHER_AES | MLN_CIPHER_KEY_DECRYPT |
					MLN_CIPHER_MODE_ECB | MLN_CIPHER_PAD_NONE, NULL, 16, NULL,
			       key, 16);
	if (ret == 0)
		ret = MlnImpl_81054C2C(cipher, in, 16, out, &size);
	MlnImpl_81054A90(cipher);
	return ret;
}

/* an AES key schedule for a 128-, 192- or 256-bit key */
unsigned int MlnImpl_810551D8(void **key, int type, unsigned int keySize, const void *keyData)
{
	void *context;
	unsigned int ret;

	*key = NULL;
	context = MlnAllocate(MLN_HEAP_GENERAL, 0x3d0);
	if (context == NULL) {
		ret = 2;
		goto fail;
	}
	if (keySize != 16 && keySize != 24 && keySize != 32) {
		ret = 0x22020002;
		goto fail;
	}
	ret = MlnImpl_81086D54(context, keySize * 8, keySize * 8, keyData);
	*key = context;
	return ret;

fail:
	if (*key == NULL)
		MlnFree(MLN_HEAP_GENERAL, context);
	return ret;
}

unsigned int MlnImpl_81055274(void *key)
{
	MlnFree(MLN_HEAP_GENERAL, key);
	return 0;
}

int MlnImpl_8105528C(void *key, const unsigned char *in, unsigned char *out)
{
	if (key == NULL)
		return 0x22020004;
	if (in == NULL || out == NULL)
		return 3;
	MlnImpl_81087358(key, in, out);
	return 0;
}

int MlnImpl_810552CC(void *key, const unsigned char *in, unsigned char *out)
{
	if (key == NULL)
		return 0x22020004;
	if (in == NULL || out == NULL)
		return 3;
	MlnImpl_81087612(key, in, out);
	return 0;
}
