#ifndef GCAUTHMGR_MS_SAVEBB_H
#define GCAUTHMGR_MS_SAVEBB_H

#include <scetypes.h>
#include <stddef.h>

/* The MAC state of a Memory Stick save: CBC-MAC over the data, the last block held back. */
typedef struct GcAuthMsSaveBbMacContext {
	int type;
	unsigned char key[16];
	unsigned char pad[16];
	SceSize padSize;
} GcAuthMsSaveBbMacContext;

/* The cipher state of a Memory Stick save: a counter mode keyed by an encrypted seed. */
typedef struct GcAuthMsSaveBbCipherContext {
	int type;
	unsigned int counter;
	unsigned char key[16];
} GcAuthMsSaveBbCipherContext;

_Static_assert(sizeof(GcAuthMsSaveBbMacContext) == 40, "MsSaveBB MAC context ABI");
_Static_assert(sizeof(GcAuthMsSaveBbCipherContext) == 24, "MsSaveBB cipher context ABI");

typedef struct GcAuthMsSaveBbMacUpdateRequest {
	GcAuthMsSaveBbMacContext *context;
	const unsigned char *data;
	SceSize size;
} GcAuthMsSaveBbMacUpdateRequest;

typedef struct GcAuthMsSaveBbMacFinalRequest {
	GcAuthMsSaveBbMacContext *context;
	unsigned char *mac;
	const unsigned char *key;
} GcAuthMsSaveBbMacFinalRequest;

typedef struct GcAuthMsSaveBbCipherInitRequest {
	GcAuthMsSaveBbCipherContext *context;
	int type;
	int mode;
	unsigned char *key;
	const unsigned char *seed;
} GcAuthMsSaveBbCipherInitRequest;

typedef struct GcAuthMsSaveBbCipherUpdateRequest {
	GcAuthMsSaveBbCipherContext *context;
	unsigned char *data;
	SceSize size;
} GcAuthMsSaveBbCipherUpdateRequest;

/* The parameter block of the CipherInit syscall. */
typedef struct GcAuthMsSaveBbCipherInitParam {
	unsigned char *key;
	const unsigned char *seed;
	SceSize keySize;
	SceSize seedSize;
	SceSize reserved[2];
} GcAuthMsSaveBbCipherInitParam;

/* drm_cipher.c */
extern const unsigned char gcAuthMsSaveBbMacMask[16];
extern const unsigned char gcAuthMsSaveBbSeedInputMask[16];
extern const unsigned char gcAuthMsSaveBbSeedOutputMask[16];

/* ms_savebb.c */
int GcAuthInitializeMsSaveBb(void);
int GcAuthMsSaveBbMacInitForUser(GcAuthMsSaveBbMacContext *context, int type);
int GcAuthMsSaveBbMacUpdateForUser(GcAuthMsSaveBbMacContext *context,
		const unsigned char *data, SceSize size);
int GcAuthMsSaveBbMacFinalForUser(GcAuthMsSaveBbMacContext *context,
		unsigned char *mac, const unsigned char *key);
int GcAuthMsSaveBbCipherInitForUser(GcAuthMsSaveBbCipherContext *context,
		int type, int mode, unsigned char *key, const unsigned char *seed);
int GcAuthMsSaveBbCipherUpdateForUser(GcAuthMsSaveBbCipherContext *context,
		unsigned char *data, SceSize size);
int GcAuthMsSaveBbCipherFinalForUser(GcAuthMsSaveBbCipherContext *context);

#endif
