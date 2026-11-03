/* Native extension ownership. Filename and helper name are ours. */
#include "ssl_ext.h"
#include "../memory.h"

void sslExtensionFree(SslExtension *extension)
{
	if (extension == NULL)
		return;
	if (extension->length != 0) {
		if (extension->data != NULL) {
			sslFree(extension->data);
			extension->data = NULL;
		}
		extension->length = 0;
	}
	sslFree(extension);
}
