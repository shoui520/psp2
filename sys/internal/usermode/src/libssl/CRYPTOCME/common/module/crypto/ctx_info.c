/* Filename and descriptive private names are ours. The component folder
 * follows the recovered common/module/crypto/ctx_init.c path.
 */
#include "ctx.h"

static const SslCryptoMacAlgorithmMapping sslDefaultMacAlgorithmMappings[] = {
	{ 65, 64, 6 },
	{ 4106, 64, 4105 },
	{ 133, 165, 6 },
	{ 130, 162, 6 },
	{ 131, 163, 6 },
	{ 132, 164, 6 },
	{ 4111, 64, 4110 },
	{ 4124, 162, 4110 },
	{ 4125, 163, 4110 },
	{ 4126, 164, 4110 },
	{ 4129, 162, 4128 },
	{ 4173, 64, 4172 },
	{ 4175, 163, 4174 },
	{ 4177, 164, 4176 },
	{ 4179, 165, 4178 },
	{ 7, 3, 6 },
	{ 8, 4, 6 },
	{ 113, 64, 116 },
	{ 179, 64, 4118 },
	{ 233, 165, 4118 },
	{ 234, 162, 4118 },
	{ 235, 163, 4118 },
	{ 236, 164, 4118 },
	{ -1, -1, -1 }
};

int sslSetCryptoContextInfo(SslCryptoContext *context, int selector,
			    SceUIntPtr value)
{
	if (context == NULL || value == 0)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case SSL_CRYPTO_CONTEXT_RANDOM_ALGORITHM:
		if (context->randomAlgorithm != NULL)
			sslDestroyAlgorithm(context->randomAlgorithm);
		context->randomAlgorithm = (SslCryptoAlgorithm *)value;
		break;
	case SSL_CRYPTO_CONTEXT_ENTROPY_SOURCE:
		context->entropySource = (void *)value;
		break;
	default:
		return SSL_CRYPTO_ERROR_NO_OPERATION;
	}
	return 0;
}

int sslQueryAlgorithmSupport(SslCryptoContextHeader *context, int family,
			     int algorithm, SceUInt32 mode, int *result)
{
	const SslCryptoAlgorithmDescriptor *table;

	*result = 0;
	if (context == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	for (table = context->algorithms; table->algorithm != -1; table++) {
		if (table->family != family || table->algorithm != algorithm)
			continue;
		if ((mode & 1) || mode == 0) {
			if (table->mode != mode)
				continue;
		} else if ((table->mode & mode) != mode) {
			continue;
		}
		*result = SSL_CRYPTO_ALGORITHM_SUPPORTED;
		break;
	}
	return 0;
}

void *sslGetDefaultMacAlgorithmMappings(void *data)
{
	(void)data;
	return (void *)sslDefaultMacAlgorithmMappings;
}

int sslMapMacAlgorithms(SslLibrary *library, int *algorithm,
			int *digestAlgorithm, int *macAlgorithm)
{
	const SslCryptoMacAlgorithmMapping *table;
	const SslCryptoMacAlgorithmMapping *(*getMapping)(void *data);
	SceUIntPtr factory = 0;
	void *data = NULL;
	int wanted;
	int status;

	status = sslGetLibraryMethod(library, SSL_CRYPTO_MODULE_TYPE, 0,
				     SSL_CRYPTO_MAC_MAPPING_FLAGS, 0,
				     SSL_LIBRARY_GET_IMPLEMENTATION_FACTORY, &factory);
	if (status != 0)
		return SSL_LIBRARY_ERROR_NOT_FOUND;
	getMapping = (const SslCryptoMacAlgorithmMapping *(*)(void *))factory;
	status = sslGetLibraryData(library, SSL_CRYPTO_MODULE_TYPE, &data);
	if (status != 0)
		return SSL_LIBRARY_ERROR_NOT_FOUND;
	table = getMapping(data);
	if (table == NULL)
		return SSL_LIBRARY_ERROR_NOT_FOUND;
	wanted = *algorithm;
	for (; table->algorithm != -1; table++) {
		if (wanted != -1 && table->algorithm == wanted) {
			*digestAlgorithm = table->digestAlgorithm;
			*macAlgorithm = table->macAlgorithm;
			break;
		}
		if (wanted == -1 && table->digestAlgorithm == *digestAlgorithm &&
		    table->macAlgorithm == *macAlgorithm) {
			*algorithm = table->algorithm;
			break;
		}
	}
	return table->algorithm == -1 ? SSL_CRYPTO_ERROR_NO_OPERATION : 0;
}

int sslGetMacAlgorithms(SslCryptoContextHeader *context, int algorithm,
			int *digestAlgorithm, int *macAlgorithm)
{
	SslLibrary *library;
	int identifier = algorithm;

	*macAlgorithm = -1;
	library = context->library;
	*digestAlgorithm = -1;
	return sslMapMacAlgorithms(library, &identifier, digestAlgorithm,
				   macAlgorithm);
}
