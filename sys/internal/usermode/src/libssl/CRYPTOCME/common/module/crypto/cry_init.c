/* Descriptive helper name is ours; source filename is recovered. */
#include "cry_api.h"
#include "../../property.h"
#include "../../../../memory.h"
#include "../../../../runtime.h"

static const char algorithmSource[] =
	"S:\\work\\CRYPTOCME\\common\\module/crypto/cry_init.c";

int sslFreeAlgorithm(SslCryptoAlgorithm *algorithm)
{
	int status = 0;

	if (algorithm != NULL &&
	    !(algorithm->flags & SSL_CRYPTO_ALGORITHM_DESTROYING) &&
	    sslAddReference(&algorithm->references, -1, SSL_CRYPTO_LOCK,
			    algorithmSource, 282) == 0) {
		const SslCryptoImplementationMethods *methods =
			algorithm->implementationMethods;
		SslObjectHeader *context;

		if (methods == NULL) {
			status = SSL_CRYPTO_ERROR_NO_METHODS;
		} else if (methods->Destroy == NULL) {
			status = SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
		} else {
			algorithm->flags |= SSL_CRYPTO_ALGORITHM_DESTROYING;
			status = methods->Destroy(algorithm);
		}
		if (algorithm->properties != NULL &&
		    (algorithm->flags & SSL_CRYPTO_ALGORITHM_OWNS_PROPERTIES)) {
			sslFreePropertyList(algorithm->properties);
			algorithm->properties = NULL;
		}
		context = algorithm->context;
		if (context != NULL && context->methods != NULL &&
		    context->methods->Destroy != NULL)
			context->methods->Destroy(context);
		sslFree(algorithm);
	}
	return status;
}
