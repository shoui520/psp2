/*
 * The device's personalization (Sony's "Mlnpsnl", SceDrmBridge's
 * sceDrmBridgeMlnpsnlAuth1/2): the personality blob is decrypted, checked
 * and its records stored, and the device's signed identity is made
 * (0x81052330-). The file's name is ours.
 */

#include "mln_internal.h"

/* the record tags of a personality, as big-endian words */
#define MLN_PSNL_TAG_NROL	0x4E524F4C	/* "NROL" */
#define MLN_PSNL_TAG_NPPR	0x4E505052	/* "NPPR" */
#define MLN_PSNL_TAG_NPPU	0x4E505055	/* "NPPU" */
#define MLN_PSNL_TAG_NPID	0x4E504944	/* "NPID" */
#define MLN_PSNL_TAG_OPPR	0x4F505052	/* "OPPR" */
#define MLN_PSNL_TAG_OPPU	0x4F505055	/* "OPPU" */
#define MLN_PSNL_TAG_OPID	0x4F504944	/* "OPID" */

/* what SceDrmBridge keeps between the personalization calls */
static unsigned char s_psnlState[64];

/* maps an error of the personality's steps to the API's */
__attribute__((noinline)) unsigned int MlnImpl_81052904(unsigned int error);

/*
 * Personalize the device from a personality blob: SceDrmBridge gives its key, the blob
 * decrypts to records ending in their SHA-1, and each record is stored; returns the core's
 * state.
 */
unsigned int MlnImpl_81052330(unsigned int size, const void *data, MlnBuffer **state)
{
	const unsigned char *in = data;
	unsigned char key[16];
	unsigned char digest[20];
	void *handle = NULL;
	unsigned char *buffer;
	unsigned char *p;
	unsigned int length;
	unsigned int tag;
	unsigned int offset;
	unsigned int ret;

	buffer = MlnAllocate(MLN_HEAP_GENERAL, size);
	if (buffer == NULL) {
		ret = 2;
		goto out;
	}
	if (sceDrmBridgeMlnpsnlAuth2(data, s_psnlState, key) != 0)
		goto invalid;
	ret = MlnImpl_81054F84(in + 0x50, key, in + 0x60, size - 0x60, buffer, &size);
	if (ret != 0)
		goto out;
	if (size < 0x44)
		goto invalid;
	size -= sizeof(digest);
	ret = MlnImpl_810570A8(buffer, size, digest);
	if (ret != 0)
		goto out;
	if (MlnMemcmp(buffer + size, digest, sizeof(digest)) != 0)
		goto invalid;
	ret = MlnImpl_81080BFC(&handle);
	if (ret != 0)
		goto fail;

	switch (MlnPlatformMode()) {
	case 1:
		if (MlnImpl_8105D280(buffer, &size) != 0)
			goto invalid;
		break;
	case 2:
		if (MlnImpl_8105D3F4(buffer, &size) != 0)
			goto invalid;
		break;
	default:
		ret = 7;
		goto out;
	}

	p = buffer;
	for (offset = 0; offset < size; p += length) {
		MlnMemcpy(&length, p, sizeof(length));
		length = MlnNtohl(length);
		offset += length;
		if (offset > size)
			goto invalid;
		MlnMemcpy(&tag, p + 4, sizeof(tag));
		tag = MlnNtohl(tag);
		switch (tag) {
		case MLN_PSNL_TAG_NROL:
			ret = MlnImpl_81043A84(3, length - 8, p + 8);
			break;
		case MLN_PSNL_TAG_OPPR:
			ret = MlnImpl_8108136C(0, length - 8, p + 8);
			break;
		case MLN_PSNL_TAG_OPPU:
			ret = MlnImpl_81043A84(1, length - 8, p + 8);
			break;
		case MLN_PSNL_TAG_NPPR:
			ret = MlnImpl_8108136C(1, length - 8, p + 8);
			break;
		case MLN_PSNL_TAG_NPPU:
			ret = MlnImpl_81043A84(2, length - 8, p + 8);
			break;
		case MLN_PSNL_TAG_OPID:
		case MLN_PSNL_TAG_NPID:
			ret = 0;
			break;
		default:
			goto invalid;
		}
		if (ret != 0)
			goto fail;
	}
	ret = MlnImpl_81043A60(1, state);
	if (ret == 0)
		goto out;
fail:
	ret = MlnImpl_81052904(ret);
	goto out;
invalid:
	ret = 0x24;
out:
	MlnImpl_81080C1C(handle);
	MlnFree(MLN_HEAP_GENERAL, buffer);
	return ret;
}

int MlnImpl_81052600(void)
{
	return 0;
}

int MlnImpl_81052608(void)
{
	return 0;
}

/*
 * The device's identity signed by SceDrmBridge for a kind of message, over the secure tick
 * in milliseconds since the Unix epoch; the output must hold 256 bytes.
 */
unsigned int MlnImpl_81052610(unsigned int kind, unsigned int capacity, MlnBlob *out)
{
	unsigned long long tick;
	unsigned char input[32];
	unsigned char output[128];
	MlnBuffer *identity;
	MlnBuffer *spare = NULL;
	unsigned int size;
	unsigned int ret;

	if (sceDrmBridgeGetCurrentSecureTick(&tick) == 0)
		tick = (tick - 62135596800000000ULL) / 1000;
	else
		MlnMemset(&tick, 0, sizeof(tick));
	if (capacity < 0x100) {
		ret = 0x2F0001;
		goto out;
	}
	MlnMemset(input, 0, sizeof(input));
	MlnMemset(output, 0, sizeof(output));
	input[0] = kind;
	sceDrmBridgeMlnpsnlAuth1(input, &tick, output, s_psnlState);

	identity = MlnBufferCreate(MLN_HEAP_GENERAL, sizeof(output), output);
	if (identity == NULL) {
		MlnBufferDestroy(MLN_HEAP_GENERAL, identity);
		ret = 2;
		goto out;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare);
	ret = 0;
	size = identity->size;
	out->buffer = identity;
	out->size = size;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, spare);
	return ret;
}

/*
 * Personalize the device from a personality as given, or, unless it is plain, first
 * decrypted with the library's key (AES-128-CBC, its first 16 bytes the IV); returns the
 * core's state.
 */
unsigned int MlnImpl_8105272C(unsigned int size, const unsigned char *data, int plain,
			      MlnBlob *out)
{
	MlnBuffer *personality = NULL;
	MlnBuffer *text;
	MlnBuffer *decoded;
	MlnBuffer *result;
	void *cipher;
	unsigned int length;
	unsigned int ret;
	unsigned int r;

	if (size == 0 || data == NULL || out == NULL) {
		ret = 4;
		goto out;
	}
	if (plain) {
		ret = MlnImpl_81052330(size, data, &result);
		if (ret != 0)
			goto out;
	} else {
		MlnBuffer encoded = { 24, (unsigned char *)"4/ek4G6CPVAufeY3aj3ykw==" };

		decoded = NULL;
		length = 0;
		cipher = NULL;
		text = NULL;
		r = MlnImpl_81026A9C(MLN_HEAP_GENERAL, &encoded, &decoded);
		if (r != 0) {
			ret = MlnImpl_81052904(r);
			goto decrypted;
		}
		length = size - 16;
		text = MlnBufferCreate(MLN_HEAP_GENERAL, length, NULL);
		if (text == NULL) {
			ret = 2;
			goto decrypted;
		}
		r = MlnImpl_81054880(&cipher, 0x02020201, data, 16, NULL, decoded->data, 16);
		if (r != 0) {
			ret = MlnImpl_81052904(r);
			goto decrypted;
		}
		r = MlnImpl_81054C2C(cipher, data + 16, length, text->data, &length);
		if (r != 0) {
			ret = MlnImpl_81052904(r);
			goto decrypted;
		}
		text->size = length;
		personality = text;
		text = NULL;
		ret = 0;
decrypted:
		MlnBufferDestroy(MLN_HEAP_GENERAL, decoded);
		MlnBufferDestroy(MLN_HEAP_GENERAL, text);
		if (cipher != NULL)
			MlnImpl_81054A90(cipher);
		if (ret != 0)
			goto out;
		ret = MlnImpl_81052330(personality->size, personality->data, &result);
		if (ret != 0)
			goto out;
	}
	out->buffer = result;
	out->size = result->size;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, personality);
	return ret;
}

unsigned int MlnImpl_81052904(unsigned int error)
{
	switch (error) {
	case 0:
		return 0;
	case 2:
		return 2;
	case 4:
		return 3;
	}
	return (error & 0xFFFF0000) == 0x11060000 ? 5 : 7;
}
