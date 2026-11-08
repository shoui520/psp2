/* Private descriptive interface names are ours, not recovered Sony spellings. */
#ifndef LIBSSL_CRYPTOCME_LIB_CTX_H
#define LIBSSL_CRYPTOCME_LIB_CTX_H

#include <stddef.h>
#include "../object.h"

#define SSL_LIBRARY_MODULE_COUNT 49
#define SSL_LIBRARY_BORROWED_MODULE_DATA 1
#define SSL_LIBRARY_LOCK 26
#define SSL_LIBRARY_ERROR_REFERENCE 10001
#define SSL_LIBRARY_ERROR_ALLOCATION 10005
#define SSL_LIBRARY_ERROR_NOT_FOUND 10008
#define SSL_LIBRARY_ERROR_MODULE_TYPE 10019
#define SSL_LIBRARY_ERROR_SELECTOR 10021

typedef struct SslLibraryMethods SslLibraryMethods;
typedef struct SslLibraryModule SslLibraryModule;
typedef struct SslLibrary SslLibrary;

/* Shared method-table prefix. Further library operations follow these slots. */
struct SslLibraryMethods {
	int (*GetMethod)(SslLibrary *library, int type, int selector,
			 SceUInt32 flags, int parameter, int request, void *result);
	int (*CreateObject)(SslLibrary *library, int type, int selector,
			    SceUInt32 flags, int parameter, void *result);
	int (*GetModuleData)(SslLibrary *library, int type, void **result);
};

struct SslLibraryModule {
	int type;
	int selector;
	SceUInt32 flags;
	int parameter;
	int (*Initialize)(SslLibraryModule *table, SslLibraryModule *module,
			  void **data);
	void (*Cleanup)(SslLibraryModule *table, SslLibraryModule *module,
			void **data);
	const void *(*GetMethods)(void *data);
	void *(*GetImplementation)(void *data);
};

typedef struct SslLibraryModuleState {
	SslObjectHeader *object;
	void *data;
} SslLibraryModuleState;

typedef struct SslLibraryValueMap {
	int key;
	int first;
	int second;
} SslLibraryValueMap;

struct SslLibrary {
	const SslLibraryMethods *methods;
	SslLibraryModule *modules_table;
	SslLibraryModuleState modules[SSL_LIBRARY_MODULE_COUNT];
	SceUInt32 flags;
	int references;
	int module_references;
};

_Static_assert(sizeof(SslLibraryModule) == 32, "SslLibraryModule");
_Static_assert(sizeof(SslLibraryModuleState) == 8, "SslLibraryModuleState");
_Static_assert(sizeof(SslLibrary) == 412, "SslLibrary");
_Static_assert(offsetof(SslLibrary, flags) == 400, "flags");
_Static_assert(offsetof(SslLibrary, references) == 404, "references");
_Static_assert(offsetof(SslLibrary, module_references) == 408,
	       "module_references");

int sslCleanupLibrary(SslLibrary *library);
int SceSslInternal_4725F9E7(SslLibrary *library);
int sslDestroyDefaultKey(void *key);
int sslSetDefaultKeyInfo(void *key, int selector, void *value);
int sslLibraryAddReference(SslLibrary *library);
int sslCreateDefaultKey(SslLibrary *library, int type, const void *info,
			void **result);
int sslGetDefaultKeyInfo(void *key, int selector, void *result);
int sslGetLibraryModuleData(SslLibrary *library, int type, void **result);
SslLibrary **sslResetDefaultLibrary(void);
SslLibrary *sslGetDefaultLibrary(void);
int SceSslInternal_09657E15(SslLibraryModule *table, int unused,
			  SslLibrary **result);
int sslGetLibraryInfo(SslLibrary *library, int selector, void *result);
int sslSelectLibraryModule(SslLibraryModule *table, SslLibraryModule **result,
			   SslLibraryModule **next);
int sslFindLibraryModule(SslLibraryModule *table, int type, int selector,
			SceUInt32 flags, int parameter, SslLibraryModule **result,
			SslLibraryModule **next);
int sslCleanupLibraryModuleData(void *data, SslLibraryModule *module);
int sslLookupLibraryValue(const SslLibraryValueMap *table, int key, int column,
			  int fallback);
const char *sslLookupLibraryString(const SslLibraryValueMap *table, int key,
				   int column);
int sslLookupLibraryMethod(SslLibrary *library, int type, int selector,
			   SceUInt32 flags, int parameter, int request,
			   SceUIntPtr *result);
int sslGetLibraryMethod(SslLibrary *library, int type, int selector,
			SceUInt32 flags, int parameter, int request, void *result);
int sslCreateLibraryObject(SslLibrary *library, int type, int selector,
			   SceUInt32 flags, int parameter, void *result);
int sslGetLibraryData(SslLibrary *library, int type, void **result);
SslLibraryModule *sslGetLibraryModuleTable(SslLibrary *library);
int sslConstructLibraryObject(SslLibrary *library, int type, int selector,
			      SceUInt32 flags, int parameter,
			      SslObjectHeader **result);
int sslGetCachedLibraryObject(SslLibrary *library, int type, int selector,
			      SceUInt32 flags, int parameter,
			      SslObjectHeader **result);
int sslInitCachedLibraryObject(SslLibrary *library, int type, int selector,
			       SceUInt32 flags, int parameter,
			       SslObjectHeader **result);

/* Descriptive request and matching names are ours. */
#define SSL_LIBRARY_ANY_TYPE (-2)
#define SSL_LIBRARY_ANY_SELECTOR (-1)
#define SSL_LIBRARY_ANY_PARAMETER (-1)
#define SSL_LIBRARY_ANY_FLAGS ((SceUInt32)-1)
#define SSL_LIBRARY_GET_IMPLEMENTATION_FACTORY 1
#define SSL_LIBRARY_GET_METHOD_FACTORY 2
#define SSL_LIBRARY_GET_IMPLEMENTATION 5
#define SSL_LIBRARY_GET_METHODS 6

_Static_assert(offsetof(SslLibraryMethods, CreateObject) == 4, "CreateObject");
_Static_assert(offsetof(SslLibraryMethods, GetModuleData) == 8, "GetModuleData");

#endif
