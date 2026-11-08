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

int sslCipherMethodUpdate(SslCryptoAlgorithm *algorithm, const void *input,
			  SceUInt32 inputLength, void *output,
			  SceUInt32 *outputLength)
{
	int (*update)(SslCryptoAlgorithm *, const void *, SceUInt32, void *,
		      SceUInt32 *);
	int status;

	if (input == NULL || output == NULL || outputLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (inputLength != 0) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			update = algorithm->implementationMethods->Update;
			if (update != NULL)
				callStatus = update(algorithm, input,
						    inputLength, output,
						    outputLength);
			status = callStatus;
		}
	}
	return status;
}

int sslCipherMethodFinal(SslCryptoAlgorithm *algorithm, void *output,
			 SceUInt32 *outputLength)
{
	int (*finalOp)(SslCryptoAlgorithm *, void *, SceUInt32 *);
	int status;

	if (output == NULL || outputLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = SSL_CRYPTO_ERROR_NO_METHODS;
	if (algorithm->implementationMethods != NULL) {
		int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

		finalOp = algorithm->implementationMethods->Final;
		if (finalOp != NULL)
			callStatus = finalOp(algorithm, output, outputLength);
		status = callStatus;
	}
	return status;
}

int sslCipherMethodOperate(SslCryptoAlgorithm *algorithm, const void *input,
			   SceUInt32 inputLength, void *output,
			   SceUInt32 *outputLength)
{
	int (*operate)(SslCryptoAlgorithm *, const void *, SceUInt32, void *,
		       SceUInt32 *);
	int status;

	if (input == NULL || output == NULL || outputLength == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (inputLength != 0) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			operate = algorithm->implementationMethods->Operate;
			if (operate != NULL)
				callStatus = operate(algorithm, input,
						     inputLength, output,
						     outputLength);
			status = callStatus;
		}
	}
	return status;
}

int sslCipherMethodEncryptInit(SslCryptoAlgorithm *algorithm, void *key,
			       const void *iv)
{
	int (*init)(SslCryptoAlgorithm *, void *, const void *, int);
	int status = SSL_LIBRARY_ERROR_SELECTOR;

	if (algorithm->family == 2) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			init = algorithm->implementationMethods->Init;
			if (init != NULL)
				callStatus = init(algorithm, key, iv, 1);
			status = callStatus;
		}
	}
	return status;
}

int sslCipherMethodDecryptInit(SslCryptoAlgorithm *algorithm, void *key,
			       const void *iv)
{
	int (*init)(SslCryptoAlgorithm *, void *, const void *, int);
	int status = SSL_LIBRARY_ERROR_SELECTOR;

	if (algorithm->family == 2) {
		status = SSL_CRYPTO_ERROR_NO_METHODS;
		if (algorithm->implementationMethods != NULL) {
			int callStatus = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

			init = algorithm->implementationMethods->Init;
			if (init != NULL)
				callStatus = init(algorithm, key, iv, 0);
			status = callStatus;
		}
	}
	return status;
}

int sslAsymMethodInit(SslCryptoAlgorithm *algorithm, void *key)
{
	int (*init)(SslCryptoAlgorithm *, void *);
	int status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;

	if (key != NULL) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
		if (algorithm->family == 1) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				int callStatus =
					SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;

				init = algorithm->implementationMethods->Init;
				if (init != NULL)
					callStatus = init(algorithm, key);
				status = callStatus;
			}
		}
	}
	return status;
}

int sslAsymMethodEncrypt(SslCryptoAlgorithm *algorithm, const void *input,
			 SceUInt32 inputLength, void *output,
			 SceUInt32 *outputLength)
{
	int (*op)(SslCryptoAlgorithm *, void *, SceUInt32 *, SceUInt32,
		  const void *, SceUInt32, int);
	SceUInt32 mode;
	int status;

	if (outputLength == NULL || input == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (inputLength != 0) {
		if (algorithm->family == 1 &&
		    ((mode = algorithm->mode) == 40 || mode == 72)) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
				if (mode == 40) {
					op = algorithm->implementationMethods->Operate;
					if (op != NULL)
						status = op(algorithm, output,
							    outputLength,
							    *outputLength, input,
							    inputLength, 0);
				} else {
					op = algorithm->implementationMethods->Final;
					if (op != NULL)
						status = op(algorithm, output,
							    outputLength,
							    *outputLength, input,
							    inputLength, 0);
				}
			}
		} else {
			status = SSL_LIBRARY_ERROR_SELECTOR;
		}
	}
	return status;
}

int sslAsymMethodDecrypt(SslCryptoAlgorithm *algorithm, const void *input,
			 SceUInt32 inputLength, void *output,
			 SceUInt32 *outputLength)
{
	int (*op)(SslCryptoAlgorithm *, void *, SceUInt32 *, SceUInt32,
		  const void *, SceUInt32, int);
	SceUInt32 mode;
	int status;

	if (outputLength == NULL || input == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	status = 10022;
	if (inputLength != 0) {
		if (algorithm->family == 1 &&
		    ((mode = algorithm->mode) == 48 || mode == 80)) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
			if (algorithm->implementationMethods != NULL) {
				status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
				if (mode == 48) {
					op = algorithm->implementationMethods->Update;
					if (op != NULL)
						status = op(algorithm, output,
							    outputLength,
							    *outputLength, input,
							    inputLength, 0);
				} else {
					op = algorithm->implementationMethods->Verify;
					if (op != NULL)
						status = op(algorithm, output,
							    outputLength,
							    *outputLength, input,
							    inputLength, 0);
				}
			}
		} else {
			status = SSL_LIBRARY_ERROR_SELECTOR;
		}
	}
	return status;
}
