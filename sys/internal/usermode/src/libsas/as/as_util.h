/* The AS runtime's utilities (as_util.c). */
#ifndef SCE_SAS_AS_UTIL_H
#define SCE_SAS_AS_UTIL_H

#include <stdarg.h>
#include <scetypes.h>

/* the AS runtime's own errors (negative, small) */
#define SCE_SAS_AS_ERROR_INVALID_ARGUMENT	(-4)
#define SCE_SAS_AS_ERROR_CONFIG			(-5)

/* the longest name sceSasAsStrcasecmp compares */
#define SCE_SAS_AS_NAME_LENGTH 128

/*
 * A linear allocator over one buffer. With no buffer it only measures: used grows by every
 * allocation, aligned, and alignment keeps the largest alignment asked for.
 */
typedef struct SceSasAsAllocator {
	char *buffer;
	char *cursor;
	SceSize used;
	SceSize alignment;
} SceSasAsAllocator;

#define SCE_SAS_AS_ALLOCATOR_ALIGNMENT		16
#define SCE_SAS_AS_ALLOCATOR_DEFAULT_ALIGNMENT	8

/* one key=value of a configuration string; referenceCount counts the reads of the key */
typedef struct SceSasAsConfigEntry {
	struct SceSasAsConfigEntry *next;
	char *key;
	char *value;
	int referenceCount;
} SceSasAsConfigEntry;

const char *sceSasAsConfigGetTokenLength(const char *string, SceSize *length);
const char *sceSasAsConfigGetToken(const char *string, char *token, SceSize size);
int sceSasAsConfigParse(SceSasAsAllocator *allocator, const char *config, SceSasAsConfigEntry ***list);
void *sceSasAsMemcpy(void *destination, const void *source, SceSize size);
void *sceSasAsMemZero(void *buffer, SceSize size);
char *sceSasAsStrlcpy(char *destination, SceSize capacity, const char *source);
char *sceSasAsStrlcat(char *destination, SceSize capacity, const char *source);
int sceSasAsStrcmp(const char *left, const char *right);
int sceSasAsStrlen(const char *string);
int sceSasAsStrcasecmp(const char *left, const char *right);
char *sceSasAsStrchr(const char *string, int value);
char *sceSasAsStrlwr(char *string, SceSize count);
int sceSasAsAtoi(const char *string);
float sceSasAsAtof(const char *string);
const char *sceSasAsGetToken(const char *string, char *token, SceSize size,
			     const char *delimiters, const char *spaces);
int sceSasAsVsnprintf(char *buffer, SceSize size, const char *format, va_list arguments);
int sceSasAsSnprintf(char *buffer, SceSize size, const char *format, ...);
void sceSasAsAllocatorInit(SceSasAsAllocator *allocator, void *buffer);
void *sceSasAsAllocatorAllocAligned(SceSasAsAllocator *allocator, SceSize size,
				    SceSize alignment, int clear);
void *sceSasAsAllocatorAlloc(SceSasAsAllocator *allocator, SceSize size, int clear);
char *sceSasAsAllocatorStrdup(SceSasAsAllocator *allocator, const char *string);
void *sceSasAsAllocatorGetBuffer(const SceSasAsAllocator *allocator);
SceSize sceSasAsAllocatorGetAlignedOffset(const SceSasAsAllocator *allocator, SceUIntPtr base,
					  SceSize alignment);
SceSize sceSasAsAllocatorGetOffset(const SceSasAsAllocator *allocator, SceUIntPtr base);
SceSize sceSasAsAllocatorGetSize(const SceSasAsAllocator *allocator);
int sceSasAsConfigGetSize(const char *config, SceSize *size);
int sceSasAsConfigCreate(const char *config, void *buffer, SceSize size, SceSasAsConfigEntry ***list);
int sceSasAsConfigCheckUnused(SceSasAsConfigEntry **list);
int sceSasAsConfigGetUInt(SceSasAsConfigEntry **list, const char *key, SceUInt32 defaultValue,
			  SceUInt32 *value);
int sceSasAsConfigGetInt(SceSasAsConfigEntry **list, const char *key, SceInt32 defaultValue,
			 SceInt32 *value);
int sceSasAsConfigGetFloat(SceSasAsConfigEntry **list, const char *key, float defaultValue,
			   float *value);
int sceSasAsConfigGetBool(SceSasAsConfigEntry **list, const char *key, SceInt8 defaultValue,
			  SceInt8 *value);
int sceSasAsConfigGetString(SceSasAsConfigEntry **list, const char *key, const char *defaultValue,
			    char *value, SceSize size);

#endif
