/* Filename, private types and fields are ours, not recovered Sony names. */
#ifndef LIBSSL_SSL_EXT_H
#define LIBSSL_SSL_EXT_H

#include <stddef.h>
#include <scetypes.h>

/* Two handler records are copied/configured as callback plus two opaque
 * parameters. Their complete call interfaces are reconstructed separately.
 */
typedef struct SslExtensionHandler {
	int (*callback)();
	SceUIntPtr parameters[2];
} SslExtensionHandler;

typedef struct SslExtension {
	SceUInt32 length;
	void *data;
	SceUInt32 type;
	SceUInt32 boolean_option;
	SceUInt32 state;
	int alert;
	SslExtensionHandler handlers[2];
} SslExtension;

void sslExtensionFree(SslExtension *extension);

_Static_assert(sizeof(SslExtension) == 48, "SslExtension");
_Static_assert(offsetof(SslExtension, handlers) == 24, "handlers");

#endif
