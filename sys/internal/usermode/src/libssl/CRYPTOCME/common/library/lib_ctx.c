#include <stddef.h>
#include "lib_ctx.h"
#include "../../../memory.h"
#include "../../../runtime.h"

static const char librarySource[] =
	"S:\\work\\CRYPTOCME\\common\\library/lib_ctx.c";
static SslLibrary *defaultLibrary;

int sslCleanupLibrary(SslLibrary *library)
{
	SslLibraryModule *next = library->modules_table;
	int borrowed = library->flags & SSL_LIBRARY_BORROWED_MODULE_DATA;
	int i;

	while (next != NULL) {
		SslLibraryModule *module = next;
		SslLibraryModuleState *state;

		next = module->type != 0 ? module + 1 : NULL;
		state = &library->modules[module->type];
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

int sslLibraryAddReference(SslLibrary *library)
{
	if (library != NULL &&
	    sslAddReference(&library->references, 1, SSL_LIBRARY_LOCK,
			    librarySource, 325) > 0)
		return 0;
	return SSL_LIBRARY_ERROR_REFERENCE;
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
	SslLibraryModule *next;
	int status;

	(void)unused;
	if (table == NULL || result == NULL) {
		status = SSL_CRYPTO_ERROR_NULL_ARGUMENT;
		goto error;
	}
	library = sslAllocate(sizeof(*library));
	if (library == NULL) {
		status = SSL_LIBRARY_ERROR_ALLOCATION;
		goto error;
	}
	sslFillMemory(library, 0, sizeof(*library));
	library->modules_table = table;
	library->references = 1;
	next = table;
	while (next != NULL) {
		SslLibraryModule *module = next;
		SslLibraryModuleState *state;

		next = module->type != 0 ? module + 1 : NULL;
		if (module->type < 0 || module->type >= SSL_LIBRARY_MODULE_COUNT) {
			status = SSL_LIBRARY_ERROR_MODULE_TYPE;
			goto error;
		}
		state = &library->modules[module->type];
		if (module->Initialize != NULL) {
			status = module->Initialize(table, module, &state->data);
			if (status != 0)
				goto error;
		}
		if (module->type == 1 && module->flags == 0 &&
		    library->methods == NULL && module->GetMethods != NULL) {
			library->methods = module->GetMethods(state->data);
			if (library->methods == NULL) {
				status = SSL_LIBRARY_ERROR_NOT_FOUND;
				goto error;
			}
		}
	}
	*result = library;
	if (defaultLibrary == NULL)
		defaultLibrary = library;
	return 0;

error:
	if (library != NULL) {
		SceSslInternal_4725F9E7(library);
	} else {
		SslLibrary temporary;

		temporary.methods = NULL;
		sslFillMemory(&temporary.modules_table, 0,
			      sizeof(temporary) - sizeof(temporary.methods));
		temporary.modules_table = table;
		sslCleanupLibrary(&temporary);
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
