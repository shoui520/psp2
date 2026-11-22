/*
 * The operations on an Octopus key: decrypting with it, an HMAC checked with it, and the
 * calls of the key store below them (0x81061858-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_OCTOPUS_KEY_ERROR_NONE	0x23040002	/* the key was not loaded */
#define MLN_OCTOPUS_KEY_ERROR_STORE	0x23040003	/* the key store failed */

#define MLN_TRACK_ERROR_KIND		0x23010001
#define MLN_TRACK_ERROR_CIPHER		0x23010002
#define MLN_TRACK_ERROR_SIZE		0x23010003
#define MLN_TRACK_ERROR_KEY		0x23010004

#define MLN_TRACK_CLEAR			1
#define MLN_TRACK_CTR			2
#define MLN_TRACK_CBC			3

/* a selectively encrypted sample's first byte when the rest is encrypted */
#define MLN_TRACK_SAMPLE_ENCRYPTED	0x80

/* the cipher algorithms of the key store */
#define MLN_CIPHER_AES_CTR		0x01030301
#define MLN_CIPHER_AES_CBC		0x01020201
#define MLN_CIPHER_AES_CBC_PADDED	0x02020201

/* how a record's samples are encrypted (16 bytes, made by MlnImpl_81061BBC) */
typedef struct MlnOctopusTrack {
	int kind;			/* MLN_TRACK_* */
	union {
		unsigned char data[11];
		struct {
			unsigned char selective;	/* a first byte says if a sample is */
			unsigned char reserved;		/* must be 0 */
			unsigned char padding;		/* 1 padded, 2 not allowed */
		} cbc;
		struct {
			unsigned char nonce[8];
			unsigned char ivSize;		/* the counter's bytes at the end */
			unsigned char selective;
			unsigned char skip;		/* bytes skipped at the start */
		} ctr;
	} info;
} MlnOctopusTrack;

/* decrypt with a key */
__attribute__((noinline)) int MlnImpl_81061858(const MlnOctopusKey *key, unsigned int flags,
					      const void *parameters, const MlnBuffer *in,
					      void *out, unsigned int *outSize)
{
	void *handle;

	handle = key->key;
	if (handle == NULL)
		return MLN_OCTOPUS_KEY_ERROR_NONE;
	return MlnImpl_81072020(&handle, flags, parameters, in, out, outSize);
}

/* whether a message's HMAC (kind 1 SHA-1, 2 SHA-256) is the signature: 0 when it is */
int MlnImpl_81061898(int kind, const MlnOctopusKey *key, const MlnBuffer *message,
		     const MlnBuffer *signature)
{
	int valid = 0;
	void *context = NULL;
	int sha256;
	int ret;

	if (key->key == NULL) {
		ret = MLN_OCTOPUS_KEY_ERROR_NONE;
		context = key->key;
	} else if (kind != 1 && kind != 2) {
		ret = 1;
	} else {
		sha256 = kind == 2;
		ret = MlnImpl_81080BFC(&context);
		if (ret == 0)
			ret = MlnImpl_81080F80(key->key, sha256, message, signature, &valid);
		if (ret == 0)
			ret = valid != 1;
		else if (ret >> 16 != 0)
			ret = MLN_OCTOPUS_KEY_ERROR_STORE;
	}
	MlnImpl_81080C1C(context);
	return ret;
}

/*
 * Whether the HMAC (kind 1 SHA-1, 2 SHA-256) of data without its last 16 bytes is the
 * digest: 0 when it is.
 */
int MlnImpl_81061964(int kind, const MlnOctopusKey *key, unsigned int size, const void *data,
		     unsigned int digestSize, const void *digest)
{
	void *context = NULL;
	int valid = 0;
	MlnBuffer *unused = NULL;
	MlnBuffer *message = NULL;
	MlnBuffer *expected = NULL;
	unsigned int length;
	int sha256;
	int ret;

	if (key == NULL || data == NULL || digest == NULL) {
		ret = 3;
		goto out;
	}
	if (key->key == NULL) {
		ret = MLN_OCTOPUS_KEY_ERROR_NONE;
		goto out;
	}
	message = MlnBufferCreate(MLN_HEAP_GENERAL, size, data);
	if (message == NULL) {
		ret = 2;
		goto out;
	}
	expected = MlnBufferCreate(MLN_HEAP_GENERAL, digestSize, digest);
	if (expected == NULL) {
		ret = 2;
		goto out;
	}
	switch (kind) {
	case 1:
		sha256 = 0;
		break;
	case 2:
		sha256 = 1;
		break;
	default:
		ret = 1;
		goto out;
	}
	length = message->size;
	if (length <= 16) {
		ret = 3;
		goto out;
	}
	message->size = length - 16;
	ret = MlnImpl_81080BFC(&context);
	if (ret == 0) {
		ret = MlnImpl_81080FF4(key->key, sha256, message, expected, &valid);
		if (ret == 0) {
			message->size = length;
			ret = valid != 1;
			goto out;
		}
	}
	if (ret >> 16 != 0)
		ret = MLN_OCTOPUS_KEY_ERROR_STORE;
out:
	MlnImpl_81080C1C(context);
	MlnBufferDestroy(MLN_HEAP_GENERAL, unused);
	MlnBufferDestroy(MLN_HEAP_GENERAL, message);
	MlnBufferDestroy(MLN_HEAP_GENERAL, expected);
	return ret;
}

/* check data with a key: its HMAC SHA-256 */
unsigned int MlnImpl_81061B2C(const MlnOctopusKey *key, const MlnBuffer *data, MlnBuffer **out)
{
	void *context = NULL;
	void *handle;
	unsigned int ret;

	if (key == NULL || out == NULL) {
		ret = 3;
		goto out;
	}
	handle = key->key;
	ret = MlnImpl_81080BFC(&context);
	if (ret == 0)
		ret = MlnImpl_8108106C(handle, data, out);
	if (ret >> 16 != 0)
		ret = MLN_OCTOPUS_KEY_ERROR_STORE;
out:
	MlnImpl_81080C1C(context);
	return ret;
}

/* make a track, of no kind yet */
unsigned int MlnImpl_81061BBC(void **out)
{
	MlnOctopusTrack *track;

	track = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*track));
	if (track == NULL)
		return 2;
	MlnMemset(track, 0, sizeof(*track));
	track->kind = 0;
	*out = track;
	return 0;
}

/* free a track */
unsigned int MlnImpl_81061C00(void *track)
{
	if (track != NULL)
		MlnFree(MLN_HEAP_GENERAL, track);
	return 0;
}

/* set a track's kind and, for an encrypted one, its 11 bytes of information */
unsigned int MlnImpl_81061C1C(void *object, int kind, const unsigned char *data)
{
	MlnOctopusTrack *track = object;

	switch (kind) {
	case MLN_TRACK_CTR:
	case MLN_TRACK_CBC:
		MlnMemcpy(track->info.data, data, sizeof(track->info.data));
		/* fall through */
	case MLN_TRACK_CLEAR:
		track->kind = kind;
		return 0;
	}
	return MLN_TRACK_ERROR_KIND;
}

/* copy a sample's clear bytes out */
static inline unsigned int MlnTrackCopy(const unsigned char *data, unsigned int size,
					unsigned char *out, unsigned int *outSize)
{
	if (size > *outSize)
		return MLN_TRACK_ERROR_SIZE;
	if (out != data && out != NULL)
		MlnMemcpy(out, data, size);
	*outSize = size;
	return 0;
}

/*
 * Decrypt a sample of a record by its track with the content key; a selectively encrypted
 * one is copied when its first byte says it is clear.
 */
unsigned int MlnImpl_81061C64(void *object, const unsigned char *data, unsigned int size,
			      const MlnOctopusKey *key, unsigned char *out,
			      unsigned int *outSize)
{
	const MlnOctopusTrack *track = object;
	MlnBuffer in = { 0, NULL };
	unsigned int algorithm;
	unsigned int ret;

	switch (track->kind) {
	case MLN_TRACK_CTR:
		if (track->info.ctr.selective) {
			if (*data++ != MLN_TRACK_SAMPLE_ENCRYPTED)
				return MlnTrackCopy(data, size - 1, out, outSize);
			size--;
		}
		if (track->info.ctr.ivSize != 0 && track->info.ctr.ivSize > size)
			return 3;
		if (track->info.ctr.skip != 0) {
			if (track->info.ctr.skip > size - track->info.ctr.ivSize)
				return 3;
			data += track->info.ctr.skip;
			size -= track->info.ctr.skip;
		}
		if (size == track->info.ctr.ivSize)
			return 0;
		if (*outSize < size - track->info.ctr.ivSize) {
			ret = MLN_TRACK_ERROR_SIZE;
			break;
		}
		in.size = size;
		in.data = (unsigned char *)data;
		ret = MlnImpl_81061858(key, MLN_CIPHER_AES_CTR, track->info.data, &in, out,
				       outSize);
		goto decrypted;
	case MLN_TRACK_CBC:
		if (track->info.cbc.selective) {
			if (*data++ != MLN_TRACK_SAMPLE_ENCRYPTED)
				return MlnTrackCopy(data, size - 1, out, outSize);
			size--;
		}
		if (track->info.cbc.padding == 2) {
			ret = MLN_TRACK_ERROR_KIND;
			break;
		}
		algorithm = track->info.cbc.padding == 1 ? MLN_CIPHER_AES_CBC_PADDED :
							   MLN_CIPHER_AES_CBC;
		if (track->info.cbc.reserved != 0) {
			ret = MLN_TRACK_ERROR_KIND;
			break;
		}
		if (size < 16)
			return 3;
		if (*outSize < size - 16) {
			ret = MLN_TRACK_ERROR_SIZE;
			break;
		}
		in.size = size;
		in.data = (unsigned char *)data;
		ret = MlnImpl_81061858(key, algorithm, NULL, &in, out, outSize);
		goto decrypted;
	case MLN_TRACK_CLEAR:
		if (*outSize < size) {
			ret = MLN_TRACK_ERROR_SIZE;
			break;
		}
		return MlnTrackCopy(data, size, out, outSize);
	default:
		ret = MLN_TRACK_ERROR_KIND;
		break;
	}
	goto error;
decrypted:
	if (ret == 0)
		return 0;
	if (ret == MLN_OCTOPUS_KEY_ERROR_NONE)
		return MLN_TRACK_ERROR_KEY;
error:
	switch (ret & 0xFFFF0000) {
	case 0x22030000:
	case 0x23040000:
	case 0x22020000:
		return MLN_TRACK_ERROR_CIPHER;
	}
	return ret;
}
