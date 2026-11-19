/*
 * SceIniFileProcessor: what its users see. An application holds an IniFileProcessor (two words),
 * sets it up with a configuration, and opens, reads and writes an INI file through it.
 */
#ifndef INI_FILE_PROCESSOR_H
#define INI_FILE_PROCESSOR_H

#include <scetypes.h>

#define SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY	-2138832895	/* 0x80840001 */
#define SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN		-2138832894	/* 0x80840002 */
#define SCE_INI_FILE_PROCESSOR_ERROR_EMPTY		-2138832891	/* 0x80840005 */
#define SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT	-2138832890	/* 0x80840006 */
#define SCE_INI_FILE_PROCESSOR_ERROR_NOT_WRITABLE	-2138832889	/* 0x80840007 */
#define SCE_INI_FILE_PROCESSOR_ERROR_NO_FILE_NAME	-2138832887	/* 0x80840009 */
#define SCE_INI_FILE_PROCESSOR_ERROR_BAD_KEY		-2138832882	/* 0x8084000e */
#define SCE_INI_FILE_PROCESSOR_ERROR_NO_OFFSET		-2138832881	/* 0x8084000f */
#define SCE_INI_FILE_PROCESSOR_ERROR_NO_SUCH_KEY	-2138832880	/* 0x80840010 */
#define SCE_INI_FILE_PROCESSOR_ERROR_BUFFER_TOO_SMALL	-2138832879	/* 0x80840011 */
#define SCE_INI_FILE_PROCESSOR_ERROR_TOO_MANY		-2138832876	/* 0x80840014 */

/* the end of the entries (D5C7B3EE) or of a file's text */
#define INI_RESULT_STREAM_END	8650753		/* 0x840001 */

/* the application's allocator: one function to take memory and one to give it back */
struct IniAllocatorOps {
	void *(*alloc)(SceSize size, void *context);
	void (*free)(void *ptr, void *context);
};

/* what the application gives the processor: the size of the pool, or its own allocator */
class IniConfig {
public:
	IniConfig();

	SceSize poolSize;
	SceUInt32 reserved;
	IniAllocatorOps *ops;
	void *context;
};

struct IniHolder;

/*
 * The application's processor: a table of functions (its destructors) and what the library keeps
 * for it. Before initialize() every method returns NOT_OPEN (close, freeText, lastError and
 * count: 0).
 */
class IniFileProcessor {
public:
	IniFileProcessor();
	virtual ~IniFileProcessor();

	/* its implementation, on the allocator the configuration asks for */
	int initialize(const IniConfig *config);
	/* the implementation ended and its memory given back; NULL, or this when there was none */
	void *terminate();

	int open(const char *path, const char *mode, int flags);
	/* mode and flags are not read: the file is made writable and empty */
	int create(const char *path, const char *mode, int flags);
	int rewind();
	int flush();
	int close();
	int get(const char *key, char *buf, SceSize size, SceSize offset);
	/* the next entry's key and value (INI_RESULT_STREAM_END after the last) */
	int next(char *key, char *value, SceSize size);
	int set(const char *key, const char *value);
	int add(const char *key, const char *value);
	/* offset (which of the key's entries) is not read: the first is removed */
	int remove(const char *key, SceSize offset);
	/* the entries as the text of a file (Sony's message calls it Serialize) */
	int serialize(void **buf, int *length);
	/* the entries read from the text of a file (Deserialize) */
	int deserialize(const char *buf, SceSize size);
	/* the text serialize() made, given back */
	int freeText();
	int lastError();
	SceSize count();

	IniHolder *m_holder;
};

#endif
