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
	unsigned char *data;
	SceUInt32 type;
	SceUInt32 boolean_option;
	SceUInt32 state;
	int alert;
	SslExtensionHandler handlers[2];
} SslExtension;

typedef struct SslExtensionData {
	SceUInt32 length;
	unsigned char *data;
} SslExtensionData;

int sslDestroyExtension(SslExtension *extension);
int sslExtensionNew(SceUInt32 type, SslExtension **result);
int sslExtensionCopy(const SslExtension *src, SslExtension **result);
int sslExtensionSetProperty(SslExtension *ext, int selector, const void *value);
int sslExtensionGetProperty(const SslExtension *ext, int selector, void *result);
int sslExtensionCompare(const SslExtension *const *a, const SslExtension *const *b);
void sslExtensionFree(SslExtension *extension);
int sslCreateRenegotiationInfoExtension(
	void *library,
	const SslExtensionData *verify_data,
	SslExtension **result);

_Static_assert(sizeof(SslExtension) == 48, "SslExtension");
_Static_assert(offsetof(SslExtension, handlers) == 24, "handlers");

#endif
