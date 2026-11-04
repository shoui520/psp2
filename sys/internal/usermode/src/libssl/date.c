/* Filename and descriptive private names are ours; inferred date component. */
#include "date.h"
#include "memory.h"

static const SslDateValue sslDateEpoch;

int SceSslInternal_D3C049B9(SslLibrary *library, int parameter, void **result)
{
	SslDateModuleFactory factory;
	SslDateModuleFactory getMethods;
	const SslDateMethods *methods;
	void *data = NULL;
	int status;

	status = sslGetLibraryMethod(library, SSL_DATE_MODULE_TYPE, 0, 0, parameter,
				     SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	getMethods = factory;
	status = sslGetLibraryData(library, SSL_DATE_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	methods = getMethods(data);
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return methods->CreateContext(library, parameter, result);
}

int SceSslInternal_D4ADF069(void *context)
{
	SslDateContext *object = context;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->methods->DestroyContext(context);
}

int SceSslInternal_5E50CB55(void *context, void **result)
{
	SslDateContext *object = context;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->methods->CreateDate(context, result);
}

int SceSslInternal_6CDB4EE8(void *date)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->DestroyDate(date);
}

int sslRetainDateObject(void *date)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->RetainDate(date);
}

int SceSslInternal_04BAED82(const void *left, const void *right, int *result)
{
	const SslDateObject *object = left;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->CompareDate(left, right, result);
}

int sslAddDateObject(void *result, const void *source, int seconds)
{
	SslDateObject *object = result;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->AddDate(result, source, seconds);
}

int SceSslInternal_D1A82B1A(void *date, int format, void *data,
			  SceUInt32 *length, SceUInt32 capacity)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->EncodeDate(date, format, data, length, capacity);
}

int SceSslInternal_88391E03(void *date, int format, const void *data, SceUInt32 length)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->DecodeDate(date, format, data, length);
}

int sslCopyDateObject(void *date, void **result)
{
	SslDateObject *object = date;
	SslDateContext *context;
	SceUInt8 data[8];
	SceUInt32 length = 0;
	int status;

	if (object == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	context = object->context;
	status = SceSslInternal_5E50CB55(context, result);
	if (status != 0)
		goto failed;
	status = object->context->methods->EncodeDate(date, SSL_TIME_FORMAT_SECONDS,
						    data, &length, sizeof(data));
	if (status != 0)
		goto failed;
	status = SceSslInternal_88391E03(*result, SSL_TIME_FORMAT_SECONDS, data, length);
	if (status == 0)
		return 0;
failed:
	if (*result != NULL) {
		SceSslInternal_6CDB4EE8(*result);
		*result = NULL;
	}
	return status;
}

int sslNewDateObject(void *context, void **result)
{
	SslDateObject *object;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	object = sslAllocate(sizeof(*object));
	if (object == NULL)
		return 10005;
	object->context = context;
	object->heapObject = 1;
	object->value.secondsHigh = 0;
	object->value.secondsLow = 0;
	object->value.fraction = 0;
	*result = object;
	return 0;
}

int sslFreeDateObject(void *date)
{
	SslDateObject *object = date;

	if (object != NULL && object->heapObject == 1)
		sslFree(object);
	return 0;
}

int sslAddDateValue(void *result, const void *source, int seconds)
{
	SslDateObject *object = result;
	const SslDateObject *input = source;
	const SslDateValue *value = input != NULL ? &input->value : &sslDateEpoch;
	SceUInt32 previous, sum;
	int carry = 0;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	previous = value->secondsLow;
	sum = previous + (SceUInt32)seconds;
	object->value.secondsLow = sum;
	if (seconds > 0 && sum < previous)
		carry = 1;
	else if (seconds < 0 && sum > previous)
		carry = -1;
	object->value.secondsHigh = (SceInt32)((SceUInt32)value->secondsHigh + carry);
	return 0;
}

int sslNewDateContext(SslLibrary *library, int parameter, void **result)
{
	SslDateContext *object;
	SslDateModuleFactory factory;
	SslDateModuleFactory getMethods;
	void *data = NULL;
	int status;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	object = sslAllocate(sizeof(*object));
	if (object == NULL)
		return 10005;
	status = sslGetLibraryMethod(library, SSL_DATE_MODULE_TYPE, 0, 0, parameter,
				     SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	getMethods = factory;
	status = sslGetLibraryData(library, SSL_DATE_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	object->methods = getMethods(data);
	*result = object;
	object->heapObject = 1;
	return 0;
}

int sslFreeDateContext(void *context)
{
	SslDateContext *object = context;

	if (object != NULL && object->heapObject == 1)
		sslFree(object);
	return 0;
}

int sslCompareDateValues(const void *left, const void *right, int *result)
{
	const SslDateObject *first = left, *second = right;
	const SslDateValue zero = {0};
	const SslDateValue *a = first != NULL ? &first->value : &zero;
	const SslDateValue *b = second != NULL ? &second->value : &zero;
	int comparison;

	if (a->secondsHigh != b->secondsHigh)
		comparison = a->secondsHigh > b->secondsHigh ? 1 : -1;
	else if (a->secondsLow != b->secondsLow)
		comparison = a->secondsLow > b->secondsLow ? 1 : -1;
	else if (a->fraction != b->fraction)
		comparison = a->fraction > b->fraction ? 1 : -1;
	else
		comparison = 0;
	*result = comparison;
	return 0;
}
