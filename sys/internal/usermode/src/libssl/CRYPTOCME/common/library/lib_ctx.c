#include <stddef.h>
#include "lib_ctx.h"
#include "../key/key_api.h"
#include "../property.h"
#include "../../../memory.h"
#include "../../../runtime.h"

typedef struct SslDefaultKey {
	const SslKeyGeneratorModule *methods;
	int type;
	int reserved0;
	int reserved1;
	SslLibrary *library;
	SslPropertyList *properties;
} SslDefaultKey;

_Static_assert(sizeof(SslDefaultKey) == 24, "SslDefaultKey");

const SslKeyGeneratorModule sslDefaultKeyGeneratorModule = {
	sslCreateDefaultKey,
	sslDestroyDefaultKey,
	sslGetDefaultKeyInfo,
	sslSetDefaultKeyInfo,
};

static const char librarySource[] =
	"S:\\work\\CRYPTOCME\\common\\library/lib_ctx.c";
static SslLibrary *defaultLibrary;

static __inline int sslSelectLibraryModuleInline(SslLibraryModule *table,
						 SslLibraryModule **result,
						 SslLibraryModule **next)
{
	if (table == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	*result = table;
	if (next != NULL)
		*next = table->type != 0 ? table + 1 : NULL;
	return 0;
}

int sslCleanupLibrary(SslLibrary *library)
{
	SslLibraryModule *module = NULL;
	SslLibraryModule *next = library->modules_table;
	int borrowed = library->flags & SSL_LIBRARY_BORROWED_MODULE_DATA;
	int i;

	while (sslSelectLibraryModuleInline(next, &module, &next) == 0) {
		SslLibraryModuleState *state = &library->modules[module->type];

		if (state->object != NULL) {
			state->object->methods->Destroy(state->object);
			state->object = NULL;
		}
		if (module->Cleanup != NULL)
			module->Cleanup(library->modules_table,
					borrowed ? NULL : module, &state->data);
	}

	for (i = 0; i < SSL_LIBRARY_MODULE_COUNT; i++) {
		if (!borrowed && library->modules[i].data != NULL) {
			sslFree(library->modules[i].data);
			library->modules[i].data = NULL;
		}
	}
	return 0;
}

int SceSslInternal_4725F9E7(SslLibrary *library)
{
	if (library != NULL) {
		sslLock(SSL_LOCK_REFERENCE, SSL_LIBRARY_LOCK, librarySource, 361);
		library->references = (int)((SceUInt32)library->references - 1);
		if (library->references == library->module_references) {
			if (defaultLibrary == library)
				defaultLibrary = NULL;
			sslLock(SSL_UNLOCK_REFERENCE, SSL_LIBRARY_LOCK,
				librarySource, 373);
			sslCleanupLibrary(library);
			sslFree(library);
		} else {
			sslLock(SSL_UNLOCK_REFERENCE, SSL_LIBRARY_LOCK,
				librarySource, 386);
		}
	}
	return 0;
}

int sslDestroyDefaultKey(void *key)
{
	SslDefaultKey *object = key;

	if (object != NULL) {
		if (object->properties != NULL)
			sslFreePropertyList(object->properties);
		if (object->library != NULL)
			SceSslInternal_4725F9E7(object->library);
		sslFree(object);
	}
	return 0;
}

int sslSetDefaultKeyInfo(void *key, int selector, void *value)
{
	SslDefaultKey *object = key;
	const SceUInt32 *words = value;
	SslPropertyList *properties = object->properties;
	SslProperty property;
	int status = 0;
	unsigned int flags = 0x12;

	if (selector == 18177) {
		if (sslDeleteProperty(properties, 1, (int)words[0]) != 0)
			status = 10001;
	} else {
		if (selector == 14)
			flags = 0x32;
		property.type = 1;
		property.id = selector;
		property.flags = flags & ~8u;
		property.data = (void *)words[1];
		property.length = words[0];
		property.value = 0;
		if (sslAddProperty(properties, &property, flags & ~8u) != 0)
			status = 10001;
	}
	return status;
}

int sslLibraryAddReference(SslLibrary *library)
{
	int status = SSL_LIBRARY_ERROR_REFERENCE;

	if (library != NULL &&
	    sslAddReference(&library->references, 1, SSL_LIBRARY_LOCK,
			    librarySource, 325) > 0)
		status = 0;
	return status;
}

static __inline int sslAllocateDefaultKeyInline(SslDefaultKey **result)
{
	SslDefaultKey *key;
	SslPropertyList *properties;
	int status = 0;

	key = sslAllocate(sizeof(*key));
	if (key != NULL &&
	    (sslFillMemory(key, 0, sizeof(*key)),
	     properties = sslAllocate(sizeof(*properties)),
	     properties != NULL)) {
		sslFillMemory(properties, 0, sizeof(*properties));
		properties->flags = SSL_PROPERTY_HEAP_OBJECT;
		key->library = NULL;
		key->properties = properties;
		*result = key;
	} else {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
		if (key != NULL)
			sslFree(key);
	}
	return status;
}

int sslCreateDefaultKey(SslLibrary *library, int type, const void *info,
			void **result)
{
	SslDefaultKey *key = NULL;
	SslKeyGeneratorFactory factory;
	SslKeyGeneratorFactory getMethods;
	void *data = NULL;
	int status;

	if (type != 1) {
		status = SSL_LIBRARY_ERROR_SELECTOR;
	} else {
		status = sslAllocateDefaultKeyInline(&key);
		if (status == 0) {
			if (sslGetLibraryMethod(library, 9, 0, 0, 0,
						SSL_LIBRARY_GET_METHOD_FACTORY,
						&factory) == 0) {
				getMethods = factory;
				if (sslGetLibraryData(library, 9, &data) == 0)
					key->methods = getMethods(data);
			} else {
				key->methods = &sslDefaultKeyGeneratorModule;
			}
			if (key->methods == NULL) {
				status = SSL_LIBRARY_ERROR_REFERENCE;
			} else {
				status = sslLibraryAddReference(library);
				if (status == 0) {
					key->library = library;
					key->type = type;
					if (info == NULL ||
					    (status = sslSetDefaultKeyInfo(
						     key, 14,
						     (void *)info)) == 0) {
						*result = key;
						return status;
					}
				}
			}
		}
		sslDestroyDefaultKey(key);
	}
	return status;
}

int sslGetDefaultKeyInfo(void *key, int selector, void *result)
{
	SslDefaultKey *object = key;
	SceUInt32 *words = result;
	SslPropertyList *properties = object->properties;
	SslProperty *property = NULL;
	int cursor = 0;
	int status = 0;

	if (sslFindProperty(properties, 1, selector, &cursor, &property, 0) != 0) {
		status = SSL_LIBRARY_ERROR_NOT_FOUND;
	} else {
		words[1] = (SceUInt32)property->data;
		words[0] = property->length;
	}
	return status;
}

int sslGetLibraryModuleData(SslLibrary *library, int type, void **result)
{
	*result = library->modules[type].data;
	return 0;
}

SslLibrary **sslResetDefaultLibrary(void)
{
	defaultLibrary = NULL;
	return &defaultLibrary;
}

SslLibrary *sslGetDefaultLibrary(void)
{
	if (defaultLibrary != NULL &&
	    sslAddReference(&defaultLibrary->references, 1, SSL_LIBRARY_LOCK,
			    librarySource, 325) > 0)
		return defaultLibrary;
	return NULL;
}

int SceSslInternal_09657E15(SslLibraryModule *table, int unused,
			  SslLibrary **result)
{
	SslLibrary *library = NULL;
	SslLibraryModule *module = NULL;
	SslLibraryModule *next;
	int status = 0;

	(void)unused;
	if (table == NULL || result == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	} else {
		library = sslAllocate(sizeof(*library));
		if (library == NULL) {
			status = SSL_LIBRARY_ERROR_ALLOCATION;
		} else {
			sslFillMemory(library, 0, sizeof(*library));
			library->modules_table = table;
			library->references = 1;
			next = table;
			while (sslSelectLibraryModuleInline(next, &module,
							    &next) == 0) {
				SslLibraryModuleState *state;

				if (module->type < 0 ||
				    module->type >= SSL_LIBRARY_MODULE_COUNT) {
					status = SSL_LIBRARY_ERROR_MODULE_TYPE;
					goto done;
				}
				state = &library->modules[module->type];
				if (module->Initialize != NULL) {
					status = module->Initialize(
						table, module, &state->data);
					if (status != 0)
						goto check_status;
				}
				if (module->type == 1 && module->flags == 0 &&
				    library->methods == NULL &&
				    module->GetMethods != NULL) {
					library->methods = module->GetMethods(
						state->data);
					if (library->methods == NULL) {
						status = SSL_LIBRARY_ERROR_NOT_FOUND;
						goto done;
					}
				}
			}
			*result = library;
			if (defaultLibrary == NULL)
				defaultLibrary = library;
		}
	}
check_status:
	if (status == 0)
		return status;
done:
	if (library == NULL) {
		SslLibrary temporary;

		temporary.methods = NULL;
		sslFillMemory(&temporary.modules_table, 0,
			      sizeof(temporary) - sizeof(temporary.methods));
		temporary.modules_table = table;
		sslCleanupLibrary(&temporary);
	} else {
		SceSslInternal_4725F9E7(library);
	}
	return status;
}

int sslGetLibraryInfo(SslLibrary *library, int selector, void *result)
{
	if (library == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	switch (selector) {
	case 1:
		*(SslLibraryModule **)result = library->modules_table;
		return 0;
	case 16:
		*(void **)result = library->modules[6].data;
		return 0;
	default:
		return SSL_LIBRARY_ERROR_SELECTOR;
	}
}
