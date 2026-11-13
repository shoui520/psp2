/* Filename and descriptive names are ours: CRYPTOCME's low-level object chain
 * and its PKCS #1 DigestInfo layer.
 */
#include "cry_api.h"
#include "ll_obj.h"
#include "../dmem/dmem.h"
#include "../../../../memory.h"

int sslPkcs1SignOperate(SslLowLevelObject *object,
			SslLowLevelCipherParams *params, int command);
int sslPkcs1VerifyOperate(SslLowLevelObject *object,
			  SslLowLevelVerifyParams *params, int command);
int sslLowLevelNullControl(SslLowLevelObject *object, int command, int arg,
			   void *value);
int sslPkcs1SetInfo(SslLowLevelObject *object, int type, int id,
		    const void *value);
static SslLowLevelMethod sslPkcs1SignMethod = {
	(int (*)(SslLowLevelObject *, void *, int))sslPkcs1SignOperate,
	sslLowLevelNullControl,
	sslPkcs1SetInfo,
	sslForwardLowLevelGetInfo,
	NULL
};

static SslLowLevelMethod sslPkcs1VerifyMethod = {
	(int (*)(SslLowLevelObject *, void *, int))sslPkcs1VerifyOperate,
	sslLowLevelNullControl,
	sslPkcs1SetInfo,
	sslForwardLowLevelGetInfo,
	NULL
};

const SslLowLevelMethod *sslGetPkcs1SignMethod(void)
{
	return &sslPkcs1SignMethod;
}

const SslLowLevelMethod *sslGetPkcs1VerifyMethod(void)
{
	return &sslPkcs1VerifyMethod;
}

/* Type 3, id 1 names the digest of the DigestInfo; everything also goes on
 * down the chain.
 */
int sslPkcs1SetInfo(SslLowLevelObject *object, int type, int id,
		    const void *value)
{
	SslLowLevelObject *next = object->next;

	if (type == 3 && id == 1)
		object->state = (void *)value;
	if (next != NULL)
		return next->method->SetInfo(next, type, id, value);
	return 0;
}

static __inline int sslDerLengthSize(int length)
{
	if (length <= 127)
		return 1;
	if (length <= 255)
		return 2;
	if (length <= 0xffff)
		return 3;
	if (length <= 0xffffff)
		return 4;
	return 5;
}

int sslEncodeDigestInfo(SceUInt32 *outputLength, unsigned char *output,
			SceUInt32 outputMax, const SslDerBuffer *oid,
			const unsigned char *digest, int digestLength,
			int form)
{
	int algorithmLength = oid->length;
	int algorithmLengthSize;
	int digestLengthSize;
	int totalLength;
	int totalLengthSize;
	int encodedLength;
	unsigned char *algorithm;
	unsigned char *p;
	unsigned char *q;
	int value;
	int i;

	if (form == SSL_DIGEST_INFO_NULL_PARAMETERS)
		algorithmLength = oid->length + 2;
	algorithmLengthSize = sslDerLengthSize(algorithmLength);
	digestLengthSize = sslDerLengthSize(digestLength);
	totalLength = algorithmLengthSize + algorithmLength + digestLength +
		      digestLengthSize + 2;
	totalLengthSize = sslDerLengthSize(totalLength);
	encodedLength = totalLengthSize + totalLength + 1;
	if ((SceUInt32)encodedLength > outputMax)
		return SSL_CRYPTO_ERROR_NO_OPERATION;

	/* The digest goes in first, at the end of the encoding */
	if (digestLength != 0) {
		p = &output[totalLengthSize + totalLength - digestLength + 1];
		for (i = digestLength; i != 0; i--)
			*p++ = *digest++;
	}
	*outputLength = encodedLength;

	/* SEQUENCE { */
	output[0] = 0x30;
	value = totalLength;
	if (totalLengthSize == 1) {
		output[1] = value & 0x7f;
	} else {
		p = &output[totalLengthSize];
		i = totalLengthSize - 1;
		do {
			i--;
			*p-- = value;
			value >>= 8;
		} while (i != 0);
		*p = (totalLengthSize - 1) | 0x80;
	}

	/* SEQUENCE { OID, NULL } */
	algorithm = &output[totalLengthSize + 1];
	algorithm[0] = 0x30;
	value = algorithmLength;
	if (algorithmLengthSize == 1) {
		algorithm[1] = value & 0x7f;
	} else {
		p = &algorithm[algorithmLengthSize];
		i = algorithmLengthSize - 1;
		do {
			i--;
			*p-- = value;
			value >>= 8;
		} while (i != 0);
		*p = (algorithmLengthSize - 1) | 0x80;
	}
	algorithm++;
	sslCopyMemory(&algorithm[algorithmLengthSize], oid->data, oid->length);
	p = &algorithm[algorithmLengthSize + oid->length];
	q = p + 1;
	if (form == SSL_DIGEST_INFO_NULL_PARAMETERS) {
		p[0] = 5;
		p[1] = 0;
		q = p + 3;
		p += 2;
	}

	/* OCTET STRING digest } */
	*p = 4;
	value = digestLength;
	if (digestLengthSize == 1) {
		*q = value & 0x7f;
	} else {
		p = &q[digestLengthSize - 1];
		i = digestLengthSize - 1;
		do {
			i--;
			*p-- = value;
			value >>= 8;
		} while (i != 0);
		*p = (digestLengthSize - 1) | 0x80;
	}
	return 0;
}

int sslGetLowLevelDigestInfo(const SslDigestDescriptor *descriptor,
			     SslDigestContext *context, int selector,
			     SceUInt32 *value, const void **pointer)
{
	SceUInt32 number = 0;
	const void *address = NULL;
	int status;

	if (descriptor == NULL) {
		if (context != NULL)
			descriptor = context->descriptor;
		if (descriptor == NULL)
			return SSL_CRYPTO_ERROR_NO_METHODS;
	}
	if (descriptor->GetInfo != NULL) {
		status = descriptor->GetInfo(descriptor, context, selector,
					     value, pointer);
		if (status != SSL_CRYPTO_ERROR_UNHANDLED)
			return status;
	}
	switch (selector) {
	case SSL_LL_DIGEST_INFO_CONTEXT_SIZE:
		number = ((descriptor->implementation->stateSize + 7) & ~7u) +
			 24;
		if (descriptor->innerImplementation != NULL)
			number += (descriptor->innerImplementation->stateSize +
				   7) & ~7u;
		break;
	case SSL_LL_DIGEST_INFO_TYPE:
		number = descriptor->type;
		break;
	case SSL_LL_DIGEST_INFO_OID:
		address = &descriptor->oid;
		break;
	case SSL_LL_DIGEST_INFO_BLOCK_LENGTH:
		number = descriptor->blockLength;
		break;
	case SSL_LL_DIGEST_INFO_DIGEST_LENGTH:
		number = descriptor->digestLength;
		break;
	case SSL_LL_DIGEST_INFO_NAME:
		address = descriptor->name;
		break;
	default:
		return SSL_CRYPTO_ERROR_UNHANDLED;
	}
	if (value != NULL)
		*value = number;
	if (pointer != NULL)
		*pointer = address;
	return 0;
}

int sslPkcs1VerifyOperate(SslLowLevelObject *object,
			  SslLowLevelVerifyParams *params, int command)
{
	const SslDigestDescriptor *digest;
	const SslDerBuffer *oid;
	SslLowLevelCipherParams decrypt;
	SceUInt32 decryptedLength;
	SceUInt32 encodedLength;
	unsigned char *decrypted;
	unsigned char *encoded;
	int verified = 0;
	int status;
	int i;

	if ((command & SSL_LL_OPERATE_VERIFICATION) == 0 ||
	    (command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_CHECK)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	digest = object->state;
	if (digest == NULL)
		return SSL_CRYPTO_ERROR_NO_DIGEST;
	encoded = NULL;
	decrypted = NULL;
	status = sslGetLowLevelDigestInfo(digest, NULL,
					  SSL_LL_DIGEST_INFO_OID, NULL,
					  (const void **)&oid);
	if (status != 0)
		return status;
	status = sslAllocateDirectMemory((void **)&decrypted,
					 params->signatureLength, object->dmem,
					 0);
	if (status == 0) {
		decrypt.output = decrypted;
		decrypt.outputMax = params->signatureLength;
		decrypt.outputLength = &decryptedLength;
		decrypt.input = params->signature;
		decrypt.inputLength = params->signatureLength;
		decrypt.options = params->options;
		decrypt.reserved24 = 0;
		status = object->next->method->Operate(
			object->next, &decrypt,
			SSL_LL_OPERATE_CIPHER | SSL_LL_OPERATE_DECRYPT);
		if (status == 0) {
			status = sslAllocateDirectMemory(
				(void **)&encoded, params->signatureLength,
				object->dmem, 0);
			if (status == 0) {
				for (i = 2; i > 0; i--) {
					int form = SSL_DIGEST_INFO_NO_PARAMETERS;

					if (i == 2)
						form = SSL_DIGEST_INFO_NULL_PARAMETERS;
					status = sslEncodeDigestInfo(
						&encodedLength, encoded,
						params->signatureLength, oid,
						params->digest,
						params->digestLength, form);
					if (status != 0)
						break;
					if (decryptedLength == encodedLength &&
					    sslCompareMemory(decrypted, encoded,
							     decryptedLength) == 0) {
						verified = 1;
						break;
					}
				}
			}
		}
	}
	if (decrypted != NULL)
		sslFreeDirectMemory(decrypted, object->dmem);
	if (encoded != NULL)
		sslFreeDirectMemory(encoded, object->dmem);
	if (params->result != NULL)
		*params->result = verified;
	return status;
}

int sslPkcs1SignOperate(SslLowLevelObject *object,
			SslLowLevelCipherParams *params, int command)
{
	const SslDigestDescriptor *digest;
	const SslDerBuffer *oid;
	SceUInt32 digestLength;
	SceUInt32 encodedLength;
	const void *input;
	SceUInt32 inputLength;
	int status;

	if ((command & SSL_LL_OPERATE_SIGNATURE) == 0 ||
	    (command & SSL_LL_OPERATE_CLASS_MASK) != SSL_LL_OPERATE_CIPHER)
		return SSL_CRYPTO_ERROR_BAD_COMMAND;
	digest = object->state;
	if (digest == NULL)
		return SSL_CRYPTO_ERROR_NO_DIGEST;
	status = sslGetLowLevelDigestInfo(digest, NULL,
					  SSL_LL_DIGEST_INFO_OID, NULL,
					  (const void **)&oid);
	if (status != 0)
		return status;
	status = sslGetLowLevelDigestInfo(digest, NULL,
					  SSL_LL_DIGEST_INFO_DIGEST_LENGTH,
					  &digestLength, NULL);
	if (status != 0)
		return status;
	if (digestLength != params->inputLength)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	status = sslEncodeDigestInfo(&encodedLength, params->output,
				     params->outputMax, oid, params->input,
				     digestLength,
				     SSL_DIGEST_INFO_NULL_PARAMETERS);
	if (status != 0)
		return status;
	/* The private key operation then runs on the DigestInfo in place */
	input = params->input;
	inputLength = params->inputLength;
	params->inputLength = encodedLength;
	params->input = params->output;
	status = object->next->method->Operate(object->next, params, command);
	params->input = input;
	params->inputLength = inputLength;
	return status;
}

int sslLowLevelNullControl(SslLowLevelObject *object, int command, int arg,
			   void *value)
{
	return 0;
}

int sslForwardLowLevelSetInfo(SslLowLevelObject *object, int type, int id,
			      const void *value)
{
	SslLowLevelObject *next = object->next;

	if (next != NULL)
		return next->method->SetInfo(next, type, id, value);
	return 0;
}

int sslForwardLowLevelGetInfo(SslLowLevelObject *object, int type, int id,
			      void *result)
{
	SslLowLevelObject *next = object->next;

	if (next != NULL)
		return next->method->GetInfo(next, type, id, result);
	return 0;
}

int sslLowLevelCipherOperate(SslLowLevelObject *object, void *output,
			     SceUInt32 *outputLength, SceUInt32 outputMax,
			     const void *input, SceUInt32 inputLength,
			     SceUInt32 options, int encrypt)
{
	SslLowLevelCipherParams params;
	SceUInt32 length;
	int (*operate)(SslLowLevelObject *, void *, int);
	int status;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (object->method == NULL ||
	    (operate = object->method->Operate) == NULL)
		return SSL_CRYPTO_ERROR_UNHANDLED;
	params.output = output;
	params.outputMax = outputMax;
	params.outputLength = &length;
	params.input = input;
	params.inputLength = inputLength;
	params.options = options;
	params.reserved24 = 0;
	status = operate(object, &params,
			 SSL_LL_OPERATE_CIPHER |
				 (encrypt != 0 ? SSL_LL_OPERATE_ENCRYPT :
						 SSL_LL_OPERATE_DECRYPT));
	if (status == 0)
		*outputLength = length;
	return status;
}

int sslAllocateLowLevelObject(SslLowLevelObject **result,
			      SslDirectMemoryContext *dmem)
{
	SslLowLevelObject *object;
	void *memory;
	int status;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	status = sslAllocateDirectMemory(&memory, sizeof(*object), dmem, 256);
	if (status == 0) {
		object = memory;
		object->dmem = dmem;
		object->flags |= SSL_LL_OBJECT_OWNED;
		*result = object;
	}
	return status;
}

void sslDestroyLowLevelObject(SslLowLevelObject *object)
{
	if (object != NULL) {
		if (object->method != NULL && object->method->Control != NULL)
			object->method->Control(object, SSL_LL_CONTROL_FREE, 0,
						NULL);
		if ((object->flags & SSL_LL_OBJECT_OWNED) != 0)
			sslFreeDirectMemory(object, object->dmem);
	}
}

void sslFreeLowLevelObject(SslLowLevelObject *object)
{
	SslLowLevelObject *next;

	while (object != NULL) {
		const SslLowLevelMethod *method = object->method;

		next = object->next;
		object->next = NULL;
		if (method != NULL && method->Control != NULL)
			method->Control(object, SSL_LL_CONTROL_FREE, 0, NULL);
		if ((object->flags & SSL_LL_OBJECT_OWNED) != 0)
			sslFreeDirectMemory(object, object->dmem);
		object = next;
	}
}

int sslSetLowLevelObjectInfo(void *object, int type, int id, const void *value)
{
	SslLowLevelObject *self = object;

	if (self == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (self->method != NULL && self->method->SetInfo != NULL)
		return self->method->SetInfo(self, type, id, value);
	return SSL_CRYPTO_ERROR_UNHANDLED;
}

int sslGetLowLevelObjectInfo(void *object, int type, int id, void *result)
{
	SslLowLevelObject *self = object;

	if (self == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (self->method != NULL && self->method->GetInfo != NULL)
		return self->method->GetInfo(self, type, id, result);
	if (self->next != NULL)
		return sslGetLowLevelObjectInfo(self->next, type, id, result);
	return SSL_CRYPTO_ERROR_UNHANDLED;
}

/* Replaces the method, freeing the old one's state and initialising the new */
int sslSetLowLevelObjectMethod(SslLowLevelObject *object,
			       const SslLowLevelMethod *method)
{
	int (*control)(SslLowLevelObject *, int, int, void *);

	if (object == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (object->method != NULL)
		object->method->Control(object, SSL_LL_CONTROL_FREE, 0, NULL);
	object->method = method;
	if (method != NULL && (control = method->Control) != NULL)
		return control(object, SSL_LL_CONTROL_INIT, 0, NULL);
	return SSL_CRYPTO_ERROR_UNHANDLED;
}

/* Replaces the rest of the chain */
__attribute__((noinline))
int sslSetLowLevelObjectNext(SslLowLevelObject *object,
			     SslLowLevelObject *next)
{
	sslFreeLowLevelObject(object->next);
	object->next = next;
	return 0;
}

/* Builds a chain with one object per method, first method first */
int sslCreateLowLevelObject(SslLowLevelObject **result,
			    const SslLowLevelMethodGetter *methods,
			    SslDirectMemoryContext *dmem)
{
	SslLowLevelObject *head = NULL;
	SslLowLevelObject *last = NULL;
	SslLowLevelObject *object = NULL;
	SslLowLevelObject *added;
	int status = 0;

	for (; *methods != NULL; methods++) {
		status = sslAllocateLowLevelObject(&object, dmem);
		added = object;
		if (status != 0)
			break;
		status = sslSetLowLevelObjectMethod(object, (*methods)());
		if (status != 0)
			break;
		if (head != NULL) {
			status = sslSetLowLevelObjectNext(last, object);
			if (status != 0)
				break;
			object = NULL;
		} else {
			head = object;
		}
		last = added;
	}
	if (status == 0) {
		*result = head;
		return 0;
	}
	if (head != NULL)
		sslFreeLowLevelObject(head);
	if (last != object && object != NULL)
		sslDestroyLowLevelObject(object);
	return status;
}

/* The input of a big number operation as the first of two context numbers */
int sslInitLowLevelBigNumParams(SslLowLevelBigNumParams *params,
				const unsigned char *data, SceUInt32 length,
				SslBigNumContext *ctx, SceUInt32 options)
{
	SslBigNum *bns = &ctx->bn[ctx->tos];

	params->ctx = ctx;
	params->input = &bns[0];
	ctx->tos += 2;
	params->options = options;
	params->output = &bns[1];
	return sslSetBigNumData(&bns[0], data, length, ctx);
}

int sslLowLevelSign(SslLowLevelObject *object, void *signature,
		    SceUInt32 *signatureLength, SceUInt32 signatureMax,
		    const void *digest, SceUInt32 digestLength,
		    SceUInt32 options)
{
	SslLowLevelCipherParams params;
	SceUInt32 length;
	int (*operate)(SslLowLevelObject *, void *, int);
	int status;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (object->method == NULL ||
	    (operate = object->method->Operate) == NULL)
		return SSL_CRYPTO_ERROR_UNHANDLED;
	params.output = signature;
	params.outputMax = signatureMax;
	params.outputLength = &length;
	params.input = digest;
	params.inputLength = digestLength;
	params.options = options;
	params.reserved24 = 0;
	status = operate(object, &params, SSL_LL_OPERATE_SIGN);
	if (status == 0)
		*signatureLength = length;
	return status;
}

/* A signature that does not decode or match verifies as false */
int sslLowLevelVerify(SslLowLevelObject *object, int *result,
		      const void *signature, SceUInt32 signatureLength,
		      const void *digest, SceUInt32 digestLength,
		      SceUInt32 options)
{
	SslLowLevelVerifyParams params;
	int (*operate)(SslLowLevelObject *, void *, int);
	int status;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_INVALID_STATE;
	if (object->method == NULL ||
	    (operate = object->method->Operate) == NULL)
		return SSL_CRYPTO_ERROR_UNHANDLED;
	params.signature = signature;
	params.signatureLength = signatureLength;
	params.digest = digest;
	params.digestLength = digestLength;
	params.result = result;
	params.options = options;
	status = operate(object, &params, SSL_LL_OPERATE_VERIFY);
	if (status == SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD ||
	    status == SSL_CRYPTO_ERROR_NOT_INITIALIZED) {
		status = 0;
		*result = 0;
	}
	return status;
}
