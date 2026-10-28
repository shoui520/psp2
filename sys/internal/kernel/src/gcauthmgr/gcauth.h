/* Game card authentication over SD CMD56; the type names are ours. */
#pragma once

#include <stddef.h>
#include <sdif/sdif.h>

typedef struct GcAuthCartSecretData {
	unsigned char bytes[32];
} __attribute__((may_alias)) GcAuthCartSecretData;

typedef struct GcAuthSessionKeyData {
	unsigned char bytes[48];
} __attribute__((may_alias)) GcAuthSessionKeyData;

typedef struct GcAuthChallengeData {
	unsigned char bytes[16];
} __attribute__((may_alias)) GcAuthChallengeData;

/* the card's two status bytes */
typedef struct GcAuthStatusBytes {
	unsigned char low;
	unsigned char high;
} GcAuthStatusBytes;

/* what the final KIRK step derives: the secret npdrm reads and the cart hash */
typedef struct GcAuthCartKeys {
	GcAuthCartSecretData secret;
	unsigned char hash[20];
} GcAuthCartKeys;

typedef struct GcAuthCmd56Key {
	unsigned char bytes[32];
} GcAuthCmd56Key;

/* the payload both sides exchange: a code, then its size counted from code */
typedef struct GcAuthCmd56Payload {
	unsigned char code;
	unsigned char reserved;
	unsigned char size;
	unsigned char data[509];
} GcAuthCmd56Payload;

typedef struct GcAuthCmd56Request {
	GcAuthCmd56Key key;
	unsigned char command[4];
	unsigned char size[4];
	unsigned char responseSize[4];
	unsigned char payload[468];
} GcAuthCmd56Request;

typedef struct GcAuthCmd56Response {
	unsigned char command;
	unsigned char reserved0[3];
	unsigned char error;
	unsigned char reserved1[3];
	unsigned char payload[2];
	unsigned char status;
	unsigned char data[501];
} GcAuthCmd56Response;

_Static_assert(sizeof(GcAuthCmd56Request) == 512, "CMD56 request size");
_Static_assert(sizeof(GcAuthCmd56Response) == 512, "CMD56 response size");
_Static_assert(offsetof(GcAuthCmd56Response, status) == 10, "CMD56 response status");

extern SceUID gcAuthGcAuthSemaphore;
extern SdifMmcContext *gcAuthMmcContext;
extern GcAuthSessionKeyData gcAuthSessionKey;
extern GcAuthCmd56Request gcAuthCmd56Request;
extern unsigned int gcAuthKeyId;
extern GcAuthCmd56Payload gcAuthCmd56Payload;
extern GcAuthCartSecretData gcAuthCartSecret;
extern GcAuthCartKeys gcAuthCartKeys;
extern GcAuthCmd56Response gcAuthCmd56Response;
extern GcAuthCmd56Response gcAuthCmd56SecondResponse;
extern GcAuthCartSecretData gcAuthCartDerivedKey;

int GcAuthExecuteKirkCommand(void *output, const void *input,
		unsigned int command, unsigned int size, unsigned int keyId);

void GcAuthBuildCmd56Request(const GcAuthCmd56Payload *payload, unsigned int size,
		unsigned int responseSize, unsigned int command, GcAuthCmd56Request *request);
int GcAuthGetCmd56Status(SdifMmcContext *context, GcAuthStatusBytes *status);
int GcAuthBuildSignedRequest(unsigned char type, unsigned int keyId);
int GcAuthGenerateRandom16(unsigned char *output);
typedef struct GcAuthCartAuthenticationRequest {
	unsigned int keyId;
} GcAuthCartAuthenticationRequest;

int GcAuthCartAuthentication(const GcAuthCartAuthenticationRequest *request);
typedef struct GcAuthGetCartSecretRequest {
	GcAuthCartSecretData *secret;
} GcAuthGetCartSecretRequest;

int GcAuthGetCartSecret(const GcAuthGetCartSecretRequest *request);
int GcAuthGetCartSecretForUser(GcAuthCartSecretData *secret);

/* a 16-byte DrmBB key, word aligned */
typedef union GcAuthDrmKey {
	unsigned char bytes[16];
	unsigned int words[4];
} GcAuthDrmKey;

/* a PSP license as DrmBB reads it */
typedef struct GcAuthDrmLicense {
	unsigned char header[8];
	unsigned int type;
	unsigned char reserved[2];
	unsigned char version;
	unsigned char flags;
	unsigned char body[48];
	unsigned char keyMask[16];
	unsigned char data[48];
	unsigned char mac[16];
} GcAuthDrmLicense;

_Static_assert(offsetof(GcAuthDrmLicense, mac) == 128, "license MAC offset");

typedef struct GcAuthDrmLicenseKeyRequest {
	const GcAuthDrmLicense *license;
	const GcAuthDrmKey *key;
	const unsigned char *secret;
	GcAuthDrmKey *output;
} GcAuthDrmLicenseKeyRequest;

int GcAuthDrmGetLicenseKey(const GcAuthDrmLicenseKeyRequest *request);
/* a PGD header: the license MACs, then the data header (encrypted) */
typedef struct GcAuthPgdHeader {
	unsigned int magic;
	unsigned int keyIndex;
	unsigned int drmType;
	unsigned int reserved;
	GcAuthDrmKey seed;
	unsigned char seedTail[16];
	GcAuthDrmKey dataSeed;
	unsigned char dataReserved[4];
	unsigned int dataSize;
	unsigned int blockSize;
	unsigned char dataTail[36];
	unsigned char licenseMac[16];
	unsigned char headerMac[16];
} GcAuthPgdHeader;

_Static_assert(sizeof(GcAuthPgdHeader) == 0x90, "PGD header size");
_Static_assert(offsetof(GcAuthPgdHeader, dataSize) == 0x44, "PGD data size");
_Static_assert(offsetof(GcAuthPgdHeader, licenseMac) == 0x70, "PGD license MAC");

typedef struct GcAuthPgdDecryptRequest {
	const GcAuthDrmKey *key;
	const unsigned char *pgd;
	unsigned int pgdSize;
	unsigned char *output;
	unsigned int outputSize;
} GcAuthPgdDecryptRequest;

int GcAuthPgdDecryptData(const GcAuthPgdDecryptRequest *request);
int GcAuthFinishCartAuthentication(SdifMmcContext *context, unsigned int keyId);
