/* Filename and descriptive private names are ours. The component folder
 * follows the recovered common/module/crypto/ctx_init.c path.
 */
#include "cry_api.h"

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
