#include "pkey_pk.h"
#include "../asn1_props.h"
#include "../../../memory.h"
#include "../../../runtime.h"
#include "../../../string_inline.h"

static const unsigned char sslRsaEncryptionOidString[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x00
};

static const char publicKeySource[] =
	"S:\\work\\CRYPTOCME\\common\\key/pkey_pk.c";

static __inline SslPropertyList *sslNewPropertyListInline(void)
{
	SslPropertyList *list = sslAllocate(sizeof(*list));

	if (list == NULL)
		return NULL;
	sslFillMemory(list, 0, sizeof(*list));
	list->flags = SSL_PROPERTY_HEAP_OBJECT;
	return list;
}

static __inline int sslGetPublicKeyInfoInline(void *key, int selector,
					      void *result)
{
	SslPublicKeyHeader *object = key;

	if (result == NULL || object == NULL || object->context->module == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (object->context->module->GetKeyInfo == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return object->context->module->GetKeyInfo(key, selector, result);
}

#define sslGetPublicKeyInfo sslGetPublicKeyInfoInline

int sslGetKeyUnsupportedError(void)
{
	return 10010;
}

int sslNoopKeyMethod(void)
{
	return 0;
}

const SslKeyGeneratorModule *sslGetDefaultKeyGeneratorModule(void)
{
	return &sslDefaultKeyGeneratorModule;
}

int sslSetPublicKeyProperty(void *key, int selector, const SceUInt32 *value)
{
	SslPublicKey *object = key;
	SceUInt32 pair[2];
	SslProperty property;
	int status = 0;
	int type = 0;

	if (object == NULL || value == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		switch (selector) {
		case 12289:
		case 12290:
		case 12291:
		case 12304:
		case 12319:
		case 12337:
		case 12338:
		case 12339:
		case 12340:
		case 12341:
			type = 32;
			selector -= 12288;
			break;
		case 12342:
			type = 32;
			selector -= 12288;
			break;
		default:
			switch (object->type) {
			case 6:
				type = 16;
				if (selector == 2008) {
					pair[0] = value[0];
					pair[1] = 0;
					value = pair;
					selector = 1;
				}
				break;
			case 28:
				type = 18;
				if (selector == 2007)
					selector = 5;
				break;
			case 116:
				type = 20;
				switch (selector) {
				case 32:
				case 33:
				case 34:
				case 35:
				case 36:
				case 37:
				case 39:
				case 2007:
					break;
				case 38:
					pair[0] = value[0];
					pair[1] = 0;
					value = pair;
					break;
				default:
					if (selector < 18176 ||
					    selector > 18447)
						status = 10018;
					break;
				}
				break;
			case 1001:
				type = 20;
				switch (selector) {
				case 32:
				case 33:
				case 34:
				case 37:
				case 39:
					break;
				case 38:
					pair[0] = value[0];
					pair[1] = 0;
					value = pair;
					break;
				default:
					status = 10018;
					break;
				}
				break;
			default:
				status = 10018;
				break;
			}
			if (selector == 18177) {
				if (sslDeleteProperty(object->properties, type,
						      (int)value[0]) != 0)
					status = 10001;
				return status;
			}
			break;
		}
		if (status == 0) {
			property.type = type;
			property.id = selector;
			property.flags = 0x32;
			property.data = (void *)value[1];
			property.length = value[0];
			property.value = 0;
			if (sslAddProperty(object->properties, &property,
					   0x32) != 0)
				status = 10001;
		}
	}
	return status;
}

int sslSetPublicKeyField(void *key, int selector, void *value)
{
	SslPublicKey *object = key;
	const SceUInt32 *words = value;
	SslProperty property;
	int status = 0;

	if (object == NULL || words == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (selector == 12343) {
		SceUInt32 encodedValue;

		switch (words[0]) {
		case 0:
			encodedValue = 27;
			break;
		case 1:
			encodedValue = 30;
			break;
		case 2:
			encodedValue = 4;
			break;
		default:
			return 10001;
		}
		property.data = (void *)words[2];
		property.length = words[1];
		property.type = 32;
		property.id = 55;
		property.flags = 0x32;
		property.value = encodedValue;
		if (sslAddProperty(object->properties, &property, 0x32) != 0)
			status = 10001;
	} else {
		status = sslSetPublicKeyProperty(object, selector, words);
	}
	return status;
}

int sslGetPublicKeyPropertySize(SslPublicKey *key, int selector)
{
	if ((key->type == 116 || key->type == 1001) && selector == 38)
		return 1;
	return 2;
}

int sslGetPublicKeyProperty(SslPublicKey *key, int selector, SceUInt32 *result)
{
	SslProperty *property = NULL;
	int cursor = 0;
	int status = 0;
	int id = selector;
	int type;

	if (key == NULL || result == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		switch (selector) {
		case 12289:
		case 12290:
		case 12291:
		case 12304:
		case 12319:
		case 12337:
		case 12338:
		case 12339:
		case 12340:
		case 12341:
		case 12342:
		case 12343:
			type = 32;
			id -= 12288;
			break;
		default:
			if (selector == 12342) {
				type = 32;
				break;
			}
			switch (key->type) {
			case 6:
				type = 16;
				break;
			case 28:
				type = 18;
				break;
			case 116:
			case 1001:
				type = 20;
				break;
			default:
				status = 10018;
				goto done;
			}
			break;
		}
		if (sslFindProperty(key->properties, type, id, &cursor,
				    &property, 0) != 0) {
			status = 10008;
		} else {
			void *data = property->data;

			result[0] = property->length;
			result[1] = (SceUInt32)data;
		}
	}
done:
	return status;
}

int sslGetPublicKeyEncodingProperty(SslPublicKey *key, SceUInt32 *result)
{
	SslProperty *property = NULL;
	int cursor = 0;
	int status = 0;

	if (key == NULL || result == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else if (sslFindProperty(key->properties, 32, 55, &cursor, &property,
				   0) != 0) {
		status = 10008;
	} else {
		SceUInt32 value = property->value;
		void *data = property->data;

		result[1] = property->length;
		result[2] = (SceUInt32)data;
		if (value == 4)
			result[0] = 2;
		else if (value == 27)
			result[0] = 0;
		else if (value == 30)
			result[0] = 1;
		else
			status = 10022;
	}
	return status;
}

int sslGetPublicKeyComponentBitLength(SslPublicKey *key, int type, int id,
				      int *result)
{
	SslProperty *property = NULL;
	int cursor = 0;
	int status;

	status = sslFindProperty(key->properties, type, id, &cursor, &property, 0);
	if (status != 0) {
		status = 10008;
		*result = 0;
	} else if (property->length != 0 && property->data != NULL) {
		for (cursor = 0; cursor < (int)property->length; cursor++) {
			const unsigned char *bytes = property->data;

			if (bytes[cursor] != 0) {
				int bits = 8 * ((int)property->length - cursor);
				int bit;

				*result = bits;
				for (bit = 7; bit >= 0; bit--) {
					if ((bytes[cursor] & (1 << bit)) != 0)
						break;
					*result = --bits;
				}
				break;
			}
		}
	} else {
		*result = 0;
	}
	return status;
}

int sslGetPublicKeyAlgorithm(void *key)
{
	int algorithm;

	if (sslGetPublicKeyInfo(key, SSL_KEY_INFO_ALGORITHM, &algorithm) != 0)
		return -1;
	return algorithm;
}

SceUInt32 sslGetRsaKeyParameterLength(SslPublicKey *key)
{
	SslProperty *property = NULL;
	int cursor = 0;
	SceUInt32 length = 0;
	int status;

	if (sslGetPublicKeyAlgorithm(key) == 6) {
		status = sslFindProperty(key->properties, 16, 1, &cursor,
					 &property, 0);
		if (status != 0)
			status = 10008;
		else
			length = property->length;
	} else {
		status = 10018;
	}
	if (status != 0)
		return 0;
	return length;
}

int sslGetRsaKeyModulusBitLength(SslPublicKey *key, int *result)
{
	int status;

	if (sslGetPublicKeyAlgorithm(key) == 6) {
		status = sslGetPublicKeyComponentBitLength(key, 16, 16, result);
		if (status == 0)
			return status;
	} else {
		status = 10018;
	}
	*result = 0;
	return status;
}

int sslGetPublicKeyType(SslPublicKey *key)
{
	return key->type;
}

int sslGetPublicKeyField(void *key, int selector, void *result)
{
	SslPublicKey *object = key;
	SceUInt32 encoding[3];
	SceUInt32 property[2];
	int bits;
	int status = 0;

	switch (selector) {
	case 2001:
		*(SslKeyContext **)result = object->context;
		break;
	case 2002:
		*(SslPropertyList **)result = object->properties;
		break;
	case 2003:
		*(SceUInt32 *)result = object->flags;
		break;
	case 2004:
		*(void **)result = object->externalData;
		break;
	case 2005:
		sslLock(SSL_LOCK_ERROR_QUEUE, SSL_KEY_CONTEXT_LOCK,
			publicKeySource, 179);
		*(int *)result = object->references;
		sslLock(SSL_UNLOCK_ERROR_QUEUE, SSL_KEY_CONTEXT_LOCK,
			publicKeySource, 181);
		break;
	case 2006:
		*(int *)result = object->type;
		break;
	case 2007:
		status = sslGetRsaKeyModulusBitLength(object, &bits);
		if (status == 0)
			*(int *)result = bits;
		break;
	case 2008:
		*(SceUInt32 *)result = sslGetRsaKeyParameterLength(object);
		break;
	case 2009:
		status = sslGetPublicKeyComponentBitLength(object, 6, 17, &bits);
		if (status == 0)
			*(int *)result = bits;
		break;
	case 12343:
		status = sslGetPublicKeyEncodingProperty(object, encoding);
		if (status == 0)
			sslCopyMemory(result, encoding, sizeof(encoding));
		break;
	default:
		status = sslGetPublicKeyProperty(object, selector, property);
		if (status == 0) {
			if (sslGetPublicKeyPropertySize(object, selector) == 1) {
				*(SceUInt32 *)result = property[0];
			} else {
				*(SslKeyComponent *)result =
					*(const SslKeyComponent *)property;
			}
		}
		break;
	}
	return status;
}

int sslEnumeratePublicKeyProperties(void *key, SslKeyComponentCallback callback,
				    void *arg)
{
	SslPublicKey *object = key;
	SslPropertyList *properties = object->properties;
	SslKeyComponent component;
	int status = 0;
	int i;

	if (properties != NULL) {
		int count = properties->count;

		for (i = 0; i < count; i++) {
			SslProperty *entry = object->properties->entries[i];

			component.length = entry->length;
			component.data = entry->data;
			status = callback(arg, entry->id, &component);
			if (status != 0)
				break;
		}
	}
	return status;
}

int sslRetainKeyContext(void *context)
{
	SslKeyContext *keyContext = context;

	if (keyContext == NULL || keyContext->module == NULL)
		return 0;
	if (keyContext->module->RetainContext == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->RetainContext(context);
}

int sslAllocatePublicKeyObject(SslPublicKey **result)
{
	SslPublicKey *key;
	int status = 0;

	if (result == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		key = sslAllocate(sizeof(*key));
		if (key == NULL) {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
		} else {
			SslPropertyList *properties;

			sslFillMemory(key, 0, sizeof(*key));
			properties = sslNewPropertyListInline();
			if (properties == NULL) {
				sslFree(key);
				status = SSL_LIBRARY_ERROR_ALLOCATION;
			} else {
				key->context = NULL;
				key->properties = properties;
				key->references = 1;
				*result = key;
			}
		}
	}
	return status;
}

int sslCreatePublicKeyObject(void *context, int type, void **result)
{
	SslPublicKey **output = (SslPublicKey **)result;
	int status;

	status = sslAllocatePublicKeyObject(output);
	if (status == 0) {
		sslRetainKeyContext(context);
		(*output)->context = context;
		(*output)->type = type;
		(*output)->references = 1;
	}
	return status;
}

int sslCleanupPublicKeyObject(SslPublicKey *key)
{
	if (key != NULL) {
		if (key->properties != NULL) {
			sslFreePropertyList(key->properties);
			key->properties = NULL;
		}
		if (!(key->flags & SSL_PUBLIC_KEY_BORROWED_DATA) &&
		    key->externalData != NULL)
			sslFree(key->externalData);
		key->externalData = NULL;
		key->context = NULL;
		sslFree(key);
	}
	return 0;
}

int sslDestroyKeyContext(void *context)
{
	SslKeyContext *keyContext = context;

	if (keyContext == NULL || keyContext->module == NULL)
		return 0;
	if (keyContext->module->DestroyContext == NULL)
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	return keyContext->module->DestroyContext(context);
}

int sslFreePublicKey(void *key)
{
	SslPublicKey *object = key;

	if (sslAddReference(&object->references, -1, SSL_KEY_CONTEXT_LOCK,
			    publicKeySource, 910) == 0) {
		sslDestroyKeyContext(object->context);
		sslCleanupPublicKeyObject(object);
	}
	return 0;
}

int sslCopyPublicKeyFields(void *source, void *destination, int mode)
{
	SslPublicKey *src = source;
	SslPublicKey *dst = destination;
	SslPropertyList *properties;
	int references;
	int status = 0;

	if (dst == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		references = dst->references;
		if (dst->properties != NULL)
			sslFreePropertyList(dst->properties);
		if (!(dst->flags & SSL_PUBLIC_KEY_BORROWED_DATA) &&
		    dst->externalData != NULL)
			sslFree(dst->externalData);
		sslDestroyKeyContext(dst->context);
		dst->context = NULL;
		dst->properties = NULL;
		dst->references = 0;
		dst->type = 0;
		dst->flags = 0;
		dst->externalData = NULL;
		sslRetainKeyContext(src->context);
		dst->context = src->context;
		dst->references = references;
		properties = sslCopyPropertyList(NULL, src->properties,
						 SSL_PROPERTY_WIPE_COPY);
		dst->properties = properties;
		if (properties == NULL) {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
		} else if (mode == 0 &&
			   sslPackProperties(properties,
					     SSL_PROPERTY_WIPE_COPY) != 0) {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
		} else {
			dst->flags |= SSL_PUBLIC_KEY_BORROWED_DATA;
			dst->externalData = src->externalData;
		}
	}
	return status;
}

int sslCopyPublicKeyObject(void *source, void *destination, int mode)
{
	SslPublicKey *src = source;
	SslPublicKey *dst = destination;
	int status;

	status = sslCopyPublicKeyFields(src, dst, mode);
	dst->type = src->type;
	return status;
}

int sslDecodeRsaKeyFromPrivateProperties(SslPublicKey **result,
					 const SceUInt8 **data,
					 SceUInt32 length)
{
	SslPublicKey *key = NULL;
	SceUInt32 consumed = 0;
	int status = 10001;

	if (result == NULL || data == NULL || length == 0 || *data == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
		goto error;
	}
	if (*result == NULL) {
		status = sslAllocatePublicKeyObject(&key);
		if (status != 0)
			goto done;
	} else {
		key = *result;
	}
	if (key != NULL) {
		SslPropertyList *properties = key->properties;

		status = sslDecodeRsaPrivateKeyProperties(properties, *data,
							  length, &consumed);
		if (status == 0) {
			if (sslPackProperties(properties,
					      SSL_PROPERTY_WIPE_COPY) != 0) {
				status = 10001;
				goto error;
			}
			status = 0;
			*data += consumed;
		}
	}
done:
	if (status == 0) {
		*result = key;
		return status;
	}
error:
	if (result != NULL && *result == NULL)
		sslCleanupPublicKeyObject(key);
	return status;
}

int sslDecodeRsaPublicKeyData(SslPublicKey **result, const SceUInt8 **data,
			      SceUInt32 length)
{
	SslPublicKey *key = NULL;
	SslPropertyList *properties;
	SslProperty *property = NULL;
	int cursor = 0;
	SceUInt32 consumed = 0;
	SceUInt32 innerConsumed = 0;
	int status;

	if (result == NULL || data == NULL || length == 0 || *data == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = sslDecodeRsaKeyFromPrivateProperties(result, data, length);
	if (status == 0)
		return status;
	if (*result == NULL) {
		status = sslAllocatePublicKeyObject(&key);
		if (status != 0)
			goto error;
	} else {
		key = *result;
	}
	properties = key->properties;
	if (sslDecodeSubjectPublicKeyInfoProperties(properties, *data,
						    length, &consumed) == 0) {
		if (sslFindProperty(properties, 48, 1, &cursor, &property,
				    0) != 0) {
			status = 10001;
			goto error;
		}
		if (property->length !=
		    sslStringLengthInline((const char *)sslRsaEncryptionOidString) ||
		    sslCompareMemory(property->data, sslRsaEncryptionOidString,
				     property->length) != 0) {
			status = 10001;
			goto error;
		}
		status = sslFindProperty(properties, 48, 3, &cursor,
					 &property, 0);
		if (status == 0) {
			status = sslDecodeRsaPublicKeyProperties(
				properties,
				(const SceUInt8 *)property->data + 1,
				property->length - 1, &innerConsumed);
			if (status == 0) {
				if (sslPackProperties(properties, 0) != 0) {
					status = 10001;
					goto error;
				}
				status = 0;
				*data += consumed;
			}
		}
	} else {
		status = sslDecodeRsaPublicKeyProperties(properties, *data,
							 length, &innerConsumed);
		if (status == 0) {
			if (sslFindProperty(properties, 16, 16, &cursor,
					    &property,
					    SSL_PROPERTY_FIND_WRAP) != 0 ||
			    sslFindProperty(properties, 16, 17, &cursor,
					    &property,
					    SSL_PROPERTY_FIND_WRAP) != 0 ||
			    sslPackProperties(properties, 0) != 0) {
				status = 10001;
				goto error;
			}
			status = 0;
			*data += consumed;
		}
	}
	if (status == 0) {
		if (result != NULL)
			*result = key;
		return status;
	}
error:
	sslCleanupPublicKeyObject(key);
	return status;
}

int sslDecodePublicKeyObject(void *context, int format, int algorithm,
			     SceUInt32 length, const void *data,
			     SceUInt32 *consumed, void **result)
{
	SslPublicKey **output = (SslPublicKey **)result;
	const SceUInt8 *cursor;
	int status = SSL_CRYPTO_ERROR_NO_OPERATION;

	(void)format;
	if (algorithm == 6) {
		cursor = data;
		status = sslDecodeRsaPublicKeyData(output, &cursor, length);
		if (status == 0) {
			SslPublicKey *key = *output;

			if (key->context != context) {
				sslDestroyKeyContext(key->context);
				sslRetainKeyContext(context);
				(*output)->context = context;
				key = *output;
			}
			key->type = 6;
			if (consumed != NULL)
				*consumed = (SceUInt32)(cursor -
							(const SceUInt8 *)data);
		}
	}
	return status;
}

int sslDecodeRsaPrivateKeyData(SslPublicKey **result, const SceUInt8 **data,
			       SceUInt32 length)
{
	SslPublicKey *key = NULL;
	SceUInt32 consumed = 0;
	int status = 10001;

	if (result == NULL || data == NULL || length == 0 || *data == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (*result == NULL) {
		status = sslAllocatePublicKeyObject(&key);
		if (status != 0)
			goto done;
	} else {
		key = *result;
	}
	if (key != NULL) {
		SslPropertyList *properties = key->properties;

		status = sslDecodeRsaPrivateKeyProperties(properties, *data,
							  length, &consumed);
		if (status == 0) {
			if (sslPackProperties(properties,
					      SSL_PROPERTY_WIPE_COPY) != 0) {
				status = 10001;
				goto error;
			}
			status = 0;
			*data += consumed;
		}
	}
done:
	if (status == 0) {
		*result = key;
		return status;
	}
error:
	sslCleanupPublicKeyObject(key);
	return status;
}

int sslDecodePrivateKeyObject(void *context, int format, int algorithm,
			      SceUInt32 length, const void *data,
			      SceUInt32 *consumed, void **result)
{
	SslPublicKey **output = (SslPublicKey **)result;
	const SceUInt8 *cursor;
	int status = SSL_CRYPTO_ERROR_NO_OPERATION;

	(void)format;
	if (algorithm == 6) {
		cursor = data;
		status = sslDecodeRsaPrivateKeyData(output, &cursor, length);
		if (status == 0) {
			SslPublicKey *key = *output;

			if (key->context != context) {
				sslDestroyKeyContext(key->context);
				sslRetainKeyContext(context);
				(*output)->context = context;
				key = *output;
			}
			key->type = 6;
			if (consumed != NULL)
				*consumed = (SceUInt32)(cursor -
							(const SceUInt8 *)data);
		}
	}
	return status;
}

int sslComparePublicKeyPropertyLists(void *left, void *right)
{
	SslPublicKey *leftKey = left;
	SslPublicKey *rightKey = right;
	SslPropertyList *leftProps;
	SslPropertyList *rightProps;
	SceUInt32 leftSize = 0;
	SceUInt32 rightSize = 0;
	int result;
	int i;

	if (leftKey == NULL && rightKey == NULL)
		return 0;
	if (leftKey == NULL && rightKey != NULL)
		return -1;
	if (leftKey != NULL && rightKey == NULL)
		return 1;
	leftProps = leftKey->properties;
	rightProps = rightKey->properties;
	for (i = 0; i < leftProps->count; i++) {
		if (leftProps->entries[i]->data != NULL)
			leftSize += leftProps->entries[i]->length;
	}
	for (i = 0; i < rightProps->count; i++) {
		if (rightProps->entries[i]->data != NULL)
			rightSize += rightProps->entries[i]->length;
	}
	result = (int)(leftSize - rightSize);
	if (leftSize == rightSize) {
		SslPropertyList *leftCopy;
		SslPropertyList *rightCopy;

		leftCopy = sslCopyPropertyList(NULL, leftProps,
					       SSL_PROPERTY_WIPE_COPY);
		rightCopy = sslCopyPropertyList(NULL, rightProps,
						SSL_PROPERTY_WIPE_COPY);
		sslPackProperties(leftCopy, SSL_PROPERTY_WIPE_COPY);
		sslPackProperties(rightCopy, SSL_PROPERTY_WIPE_COPY);
		result = sslCompareMemory(leftCopy->storage, rightCopy->storage,
					  leftSize);
		if (leftCopy != NULL)
			sslFreePropertyList(leftCopy);
		if (rightCopy != NULL)
			sslFreePropertyList(rightCopy);
	}
	return result;
}

int sslCompareKeyIntegerComponents(const SslKeyComponent *left,
				   const SslKeyComponent *right)
{
	SceUInt32 leftLength = left->length;
	SceUInt32 rightLength = right->length;

	if (leftLength > rightLength) {
		while (leftLength > rightLength) {
			if (((const unsigned char *)left->data)[left->length - leftLength] != 0)
				return 1;
			leftLength--;
		}
	} else if (leftLength < rightLength) {
		while (leftLength < rightLength) {
			if (((const unsigned char *)right->data)[right->length - rightLength] != 0)
				return -1;
			rightLength--;
		}
	}
	return sslCompareBuffers(
		(const unsigned char *)left->data + left->length - leftLength,
		(const unsigned char *)right->data + right->length - rightLength,
		leftLength, rightLength);
}

int sslComparePublicKeyObjects(void *left, void *right)
{
	SslPublicKey *leftKey = left;
	SslPublicKey *rightKey = right;
	SslKeyComponent leftComponent;
	SslKeyComponent rightComponent;
	int result = 1;

	if (leftKey == NULL && rightKey == NULL)
		return 0;
	if (leftKey == NULL && rightKey != NULL)
		return -1;
	if (leftKey != NULL && rightKey == NULL)
		return 1;
	if (leftKey->type == rightKey->type && leftKey->type == 6 &&
	    sslGetPublicKeyInfo(leftKey, 16, &leftComponent) == 0 &&
	    sslGetPublicKeyInfo(rightKey, 16, &rightComponent) == 0) {
		result = sslCompareKeyIntegerComponents(&leftComponent,
							&rightComponent);
		if (result == 0) {
			result = 1;
			if (sslGetPublicKeyInfo(leftKey, 17, &leftComponent) == 0 &&
			    sslGetPublicKeyInfo(rightKey, 17, &rightComponent) == 0)
				result = sslCompareKeyIntegerComponents(
					&leftComponent, &rightComponent);
		}
	}
	return result;
}
