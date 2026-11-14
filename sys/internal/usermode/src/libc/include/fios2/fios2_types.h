/*
 * fios2_types.h: FIOS2's types, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include_common/fios2/fios2_types.h); the ones the tree uses so far.
 */
#ifndef _SCE_FIOS2_TYPES_H
#define _SCE_FIOS2_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef int64_t SceFiosOffset;	/* an offset within a file */
typedef int64_t SceFiosSize;	/* a file size */
typedef int64_t SceFiosTime;	/* a time in FIOS2 ticks */

typedef int32_t SceFiosHandle;
#define SCE_FIOS_HANDLE_INVALID		0

typedef SceFiosHandle SceFiosOp;	/* operation handle */
#define SCE_FIOS_OP_INVALID		0

typedef SceFiosHandle SceFiosFH;	/* file handle */
#define SCE_FIOS_FH_INVALID		0

/* sceFiosFHSeek's whence */
typedef enum SceFiosWhence {
	SCE_FIOS_SEEK_SET = 0,		/* from the beginning of the file */
	SCE_FIOS_SEEK_CUR = 1,		/* from the current position */
	SCE_FIOS_SEEK_END = 2		/* from the end of the file */
} SceFiosWhence;

/* open flags */
typedef enum SceFiosOpenFlags {
	SCE_FIOS_O_READ = (1U << 0),
	SCE_FIOS_O_RDONLY = SCE_FIOS_O_READ,
	SCE_FIOS_O_WRITE = (1U << 1),
	SCE_FIOS_O_WRONLY = SCE_FIOS_O_WRITE,
	SCE_FIOS_O_RDWR = (SCE_FIOS_O_READ | SCE_FIOS_O_WRITE),
	SCE_FIOS_O_APPEND = (1U << 2),	/* write at the end only */
	SCE_FIOS_O_CREAT = (1U << 3),	/* create the file if it does not exist */
	SCE_FIOS_O_TRUNC = (1U << 4),	/* truncate the file if it exists */
	SCE_FIOS_O_DIRECT = (1U << 12)
} SceFiosOpenFlags;

struct SceFiosOpAttr;
typedef int (*SceFiosOpCallback)(void *pContext, SceFiosOp op, uint8_t event, int err);

/* a buffer in memory */
typedef struct SceFiosBuffer {
	void *pPtr;			/* start of the buffer */
	size_t length;			/* length in bytes */
} SceFiosBuffer;
#define SCE_FIOS_BUFFER_INITIALIZER	{ 0, 0 }

/* operation attributes */
typedef struct SceFiosOpAttr {
	SceFiosTime deadline;
	SceFiosOpCallback pCallback;
	void *pCallbackContext;
	int32_t priority : 8;
	uint32_t opflags : 24;
	uint32_t userTag;
	void *userPtr;
	void *pReserved;
} SceFiosOpAttr;
#define SCE_FIOS_OPATTR_INITIALIZER	{ 0, 0, 0, 0, 0, 0, 0, 0 }

/* file open parameters */
typedef struct SceFiosOpenParams {
	uint32_t openFlags : 16;	/* SceFiosOpenFlags */
	uint32_t opFlags : 16;		/* for internal use */
	uint32_t reserved;
	SceFiosBuffer buffer;		/* optional I/O buffer */
} SceFiosOpenParams;
#define SCE_FIOS_OPENPARAMS_INITIALIZER	{ 0, 0, 0, SCE_FIOS_BUFFER_INITIALIZER }

#endif /* _SCE_FIOS2_TYPES_H */
