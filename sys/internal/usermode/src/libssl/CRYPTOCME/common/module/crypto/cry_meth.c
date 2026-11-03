/* Filename and descriptive private names are ours. The component folder
 * follows the recovered CRYPTOCME common/module/crypto sources.
 */
#include "cry_api.h"

int sslFindAlgorithmDescriptor(const SslCryptoAlgorithmDescriptor *table,
			       int family, int algorithm, SceUInt32 mode,
			       const SslCryptoAlgorithmDescriptor **result)
{
	if (table == NULL)
		return SSL_LIBRARY_ERROR_NOT_FOUND;
	for (; table->algorithm != -1; table++) {
		if (table->family != family || table->algorithm != algorithm)
			continue;
		if ((mode & 1) || mode == 0) {
			if (table->mode != mode)
				continue;
		} else if ((table->mode & mode) != mode) {
			continue;
		}
		*result = table;
		return 0;
	}
	return SSL_LIBRARY_ERROR_NOT_FOUND;
}

int sslGetAlgorithmDescriptor(SslCryptoAlgorithm *algorithm,
			      const SslCryptoAlgorithmDescriptor *table,
			      const SslCryptoAlgorithmDescriptor **result)
{
	return sslFindAlgorithmDescriptor(table, algorithm->family,
					  algorithm->algorithm, algorithm->mode,
					  result);
}
