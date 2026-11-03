/* Filename and descriptive private names are ours, not Sony spellings. */
#ifndef LIBSSL_CRYPTOCME_PROPERTY_H
#define LIBSSL_CRYPTOCME_PROPERTY_H

#include <stddef.h>
#include <scetypes.h>

#define SSL_PROPERTY_HEAP_OBJECT 1
#define SSL_PROPERTY_OWNS_DATA 2
#define SSL_PROPERTY_WIPE_DATA 4
#define SSL_PROPERTY_COPY_DATA 2
#define SSL_PROPERTY_BORROW_ENTRY 8
#define SSL_PROPERTY_REPLACE 16
#define SSL_PROPERTY_WIPE_COPY 32
#define SSL_PROPERTY_TAKE_DATA 64
#define SSL_PROPERTY_FIND_WRAP 4
#define SSL_PROPERTY_FAILURE 1
#define SSL_PROPERTY_STORAGE_ERROR 2

typedef struct SslProperty {
	int type;
	int id;
	SceUInt32 value;
	void *data;
	SceUInt32 length;
	SceUInt32 flags;
} SslProperty;

typedef struct SslPropertyList {
	/* Only whole-object initialization accesses this word. */
	SceUInt32 reserved;
	int count;
	int capacity;
	SslProperty **entries;
	void *storage;
	SceUInt32 flags;
} SslPropertyList;

_Static_assert(sizeof(SslProperty) == 24, "SslProperty");
_Static_assert(sizeof(SslPropertyList) == 24, "SslPropertyList");
_Static_assert(offsetof(SslProperty, data) == 12, "data");
_Static_assert(offsetof(SslPropertyList, entries) == 12, "entries");

SslProperty *sslNewProperty(void);
int sslSetProperty(SslProperty *property, int type, int id, const void *data,
		   SceUInt32 length, unsigned int flags);
int sslCopyProperty(SslProperty *destination, const SslProperty *source,
		    unsigned int flags);
void sslFreeProperty(SslProperty *property);
SslProperty *sslCloneProperty(SslProperty *destination,
			      const SslProperty *source, unsigned int flags);
int sslFindProperty(SslPropertyList *list, int type, int id, int *cursor,
		    SslProperty **result, unsigned int flags);
void sslFreePropertyList(SslPropertyList *list);
int sslDeleteProperty(SslPropertyList *list, int type, int id);
int sslAddProperty(SslPropertyList *list, SslProperty *property,
		   unsigned int flags);
SslPropertyList *sslNewPropertyList(void);
SslPropertyList *sslCopyPropertyList(SslPropertyList *destination,
				    SslPropertyList *source, unsigned int flags);
SslPropertyList *sslInitPropertyList(SslPropertyList *list);
int sslAddPropertyValue(SslPropertyList *list, int type, int id, SceUInt32 value,
			const void *data, SceUInt32 length, unsigned int flags);
int sslNextProperty(SslPropertyList *list, int *cursor, SslProperty **result);
int sslPackProperties(SslPropertyList *list, unsigned int flags);

#endif
