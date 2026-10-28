/* Complete module-private Pcact protocol objects. */
#pragma once

#include <stddef.h>
#include "ecc.h"

typedef struct GcAuthPcactActivationBlock {
	unsigned char format;
	unsigned char version;
	unsigned char mode;
	unsigned char reserved[13];
	union {
		unsigned char encryptedPayload[64];
		struct {
			unsigned char ciphertext[48];
			unsigned char mac[16];
		} authenticated;
	};
} GcAuthPcactActivationBlock;

typedef struct GcAuthPcactRandomRecord {
	unsigned char randomKey[16];
	unsigned char derivedKey[16];
	unsigned char mode;
	unsigned char reserved[31];
} GcAuthPcactRandomRecord;

typedef struct GcAuthPcactKeyRequest {
	const GcAuthPcactActivationBlock *activation;
	const GcAuthPcactRandomRecord *random;
	unsigned char *outputKey;
} GcAuthPcactKeyRequest;

typedef union GcAuthPcactCertificate {
	unsigned char serialized[232];
	struct {
		GcAuthAdhocSignedCertificate224 signedCertificate;
		GcAuthAdhocPublicKey224 publicKey;
		unsigned char encryptedPrivateKey[32];
		unsigned char encryptedCmac[16];
	} fields;
} GcAuthPcactCertificate;

_Static_assert(sizeof(GcAuthPcactCertificate) == 232, "certificate224 size");
_Static_assert(offsetof(GcAuthPcactCertificate, fields.publicKey) == 128,
	"certificate224 public key offset");
_Static_assert(offsetof(GcAuthPcactCertificate, fields.encryptedPrivateKey) == 184,
	"certificate224 encrypted private key offset");
_Static_assert(offsetof(GcAuthPcactCertificate, fields.encryptedCmac) == 216,
	"certificate224 encrypted CMAC offset");

typedef struct GcAuthPcactCertificateRequest {
	unsigned char *output;
} GcAuthPcactCertificateRequest;

typedef struct GcAuthPcactDecodedKey {
	unsigned char nonceHalves[2][8];
	unsigned char comparisonKey[16];
} GcAuthPcactDecodedKey;

typedef struct GcAuthPsmactChallengeHeader {
	unsigned char format;
	unsigned char packetType;
	unsigned char mode;
	unsigned char magic[4];
	unsigned char packetBlocks;
	unsigned char reserved[7];
	unsigned char trailerTag;
} GcAuthPsmactChallengeHeader;

typedef union GcAuthPsmactChallengePayload {
	struct {
		GcAuthPcactCertificate certificate;
		unsigned char zeroFill[264];
	} certificateInput;
	struct {
		unsigned char certificatePrefix[128];
		unsigned char rtcSeconds[8];
		unsigned char challenge[16];
		unsigned char derivationSeed[16];
		unsigned char certificateSuffix[64];
		unsigned char zeroFill[264];
	} challenge;
} GcAuthPsmactChallengePayload;

typedef struct GcAuthPsmactChallenge {
	GcAuthPsmactChallengeHeader header;
	unsigned char encryptedKey[256];
	GcAuthPsmactChallengePayload payload;
	unsigned char signature[256];
} GcAuthPsmactChallenge;

typedef struct GcAuthPsmactRecordHeader {
	unsigned char format;
	unsigned char packetType;
	unsigned char mode;
	unsigned char magic[4];
	unsigned char flags;
	unsigned char recordSize;
	unsigned char reserved[7];
} GcAuthPsmactRecordHeader;

typedef struct GcAuthPsmactRandomRecord {
	GcAuthPsmactRecordHeader header;
	unsigned char rtcSeconds[8];
	unsigned char challenge[16];
	unsigned char derivationSeed[16];
	unsigned char zeroFill[8];
} GcAuthPsmactRandomRecord;

typedef union GcAuthActivationRandom {
	GcAuthPcactRandomRecord pcact;
	GcAuthPsmactRandomRecord psmact;
} GcAuthActivationRandom;

typedef struct GcAuthPsmactChallengeRequest {
	unsigned char mode;
	unsigned char padding[3];
	const unsigned char *rtcSeconds;
	GcAuthPsmactChallenge *output;
	GcAuthPsmactRandomRecord *random;
} GcAuthPsmactChallengeRequest;

typedef struct GcAuthPsmactKeyEnvelope {
	unsigned char rtcSeconds[8];
	unsigned char aesKey[16];
} GcAuthPsmactKeyEnvelope;

typedef struct GcAuthPsmactActivation {
	GcAuthPsmactRecordHeader header;
	unsigned char challenge[16];
	unsigned char derivationSeed[16];
	unsigned char authenticatedPayload[432];
	unsigned char mac[32];
} GcAuthPsmactActivation;

typedef struct GcAuthPsmactDerivedKeys {
	unsigned char stageOneKey[32];
	unsigned char stageTwoKey[32];
} GcAuthPsmactDerivedKeys;

typedef struct GcAuthPsmactVerifyRequest {
	const GcAuthPsmactActivation *activation;
	const GcAuthPsmactRandomRecord *random;
	GcAuthPsmactDerivedKeys *output;
} GcAuthPsmactVerifyRequest;

typedef struct GcAuthPsmactDerivationInput {
	unsigned char recordSeed[16];
	unsigned char activationSeed[16];
	unsigned char stage;
	unsigned char label[4];
} GcAuthPsmactDerivationInput;

typedef struct GcAuthPcactHashInput {
	union {
		struct {
			unsigned int size;
			unsigned char salt[16];
		} framing;
		unsigned char sha1[20];
	} header;
	unsigned char serializedChallenge[112];
} GcAuthPcactHashInput;

typedef struct GcAuthPcactChallengeBody {
	unsigned char derivedKey[16];
	unsigned char salt[16];
	unsigned char consoleDerivedKey[16];
	unsigned char rtcSeconds[8];
	unsigned char entropy[32];
	unsigned char zeroFill[8];
} GcAuthPcactChallengeBody;

typedef struct GcAuthPcactChallengePacket {
	unsigned char format;
	unsigned char version;
	unsigned char mode;
	unsigned char reserved[13];
	unsigned char ciphertext[96];
} GcAuthPcactChallengePacket;

typedef union GcAuthPcactChallengeWorkspace {
	GcAuthPcactChallengeBody plaintext;
	GcAuthPcactChallengePacket encrypted;
} GcAuthPcactChallengeWorkspace;

typedef struct GcAuthPcactChallenge {
	GcAuthPcactChallengePacket packet;
	unsigned char mac[16];
} GcAuthPcactChallenge;

typedef struct GcAuthPcactChallengeRequest {
	const unsigned char *entropy;
	const unsigned char *rtcSeconds;
	GcAuthPcactChallenge *output;
	GcAuthPcactRandomRecord *random;
} GcAuthPcactChallengeRequest;

typedef struct GcAuthPcactChallengeInput {
	unsigned char format;
	unsigned char version;
	unsigned char reserved[14];
	union {
		unsigned char entropy[32];
		unsigned int entropyWords[8];
	};
} GcAuthPcactChallengeInput;

typedef struct GcAuthPcactEncryptedChallenge {
	unsigned char serialized[128];
} GcAuthPcactEncryptedChallenge;

typedef struct GcAuthPcactBeginActivationRequest {
	unsigned int mode;
	const unsigned int *entropy;
	GcAuthPcactEncryptedChallenge *output;
} GcAuthPcactBeginActivationRequest;

typedef struct GcAuthPcactEncryptedNpDrmActivation {
	unsigned char serialized[4160];
} GcAuthPcactEncryptedNpDrmActivation;

typedef struct GcAuthPcactActivationBundle {
	GcAuthPcactActivationBlock key;
	GcAuthPcactEncryptedNpDrmActivation npDrm;
} GcAuthPcactActivationBundle;

typedef struct GcAuthPcactApplyActivationRequest {
	GcAuthPcactActivationBundle *activation;
	unsigned int size;
} GcAuthPcactApplyActivationRequest;

typedef struct GcAuthPsmactEncryptedActivation {
	struct {
		unsigned char format;
		unsigned char packetType;
		unsigned char mode;
		unsigned char recordSize;
		unsigned char reserved[12];
	} header;
	unsigned char ciphertext[1024];
	unsigned char mac[32];
} GcAuthPsmactEncryptedActivation;

typedef union GcAuthActivationWorkspace {
	GcAuthPcactActivationBundle pcact;
	GcAuthPsmactChallenge challenge;
	GcAuthPsmactActivation psmact;
	GcAuthPsmactEncryptedActivation encryptedPsmact;
} GcAuthActivationWorkspace;

typedef struct GcAuthPsmactBeginActivationRequest {
	unsigned char mode;
	unsigned char padding[3];
	GcAuthPsmactChallenge *output;
	GcAuthPsmactRandomRecord *random;
} GcAuthPsmactBeginActivationRequest;

typedef struct GcAuthPsmactActivationFile {
	unsigned char serialized[1024];
} GcAuthPsmactActivationFile;

typedef struct GcAuthPsmactApplyActivationRequest {
	const GcAuthPsmactActivation *activation;
	const GcAuthPsmactRandomRecord *random;
	GcAuthPsmactEncryptedActivation *encryptedActivation;
} GcAuthPsmactApplyActivationRequest;

_Static_assert(sizeof(GcAuthPsmactActivationFile) == 1024,
	       "Serialized PSM activation file ABI");
_Static_assert(sizeof(GcAuthPsmactApplyActivationRequest) == 12,
	       "PSM apply activation request ABI");

_Static_assert(sizeof(GcAuthPsmactEncryptedActivation) == 1072,
	       "Encrypted PSM activation ABI");
_Static_assert(offsetof(GcAuthPsmactEncryptedActivation, ciphertext) == 16,
	       "Encrypted PSM payload offset");
_Static_assert(offsetof(GcAuthPsmactEncryptedActivation, mac) == 1040,
	       "Encrypted PSM MAC offset");
_Static_assert(sizeof(GcAuthActivationWorkspace) == 4240,
	       "Activation workspace ABI");
_Static_assert(sizeof(GcAuthPsmactBeginActivationRequest) == 12,
	       "PSM activation request ABI");

_Static_assert(sizeof(GcAuthPcactEncryptedNpDrmActivation) == 4160,
	       "Encrypted NPDRM activation ABI");
_Static_assert(sizeof(GcAuthPcactActivationBundle) == 4240,
	       "Pcact activation bundle ABI");
_Static_assert(offsetof(GcAuthPcactActivationBundle, npDrm) == 80,
	       "Pcact encrypted NPDRM activation offset");
_Static_assert(sizeof(GcAuthPcactApplyActivationRequest) == 8,
	       "Pcact apply activation request ABI");

_Static_assert(sizeof(GcAuthActivationRandom) == 64,
	       "Activation random storage ABI");
_Static_assert(sizeof(GcAuthPcactBeginActivationRequest) == 12,
	       "Pcact activation request ABI");
_Static_assert(offsetof(GcAuthPcactChallengeInput, entropyWords) == 16,
	       "Pcact entropy word-copy offset");

typedef struct GcAuthPcactEncryptChallengeRequest {
	unsigned char mode;
	unsigned char padding[3];
	const GcAuthPcactChallengeInput *input;
	const unsigned char *rtcSeconds;
	GcAuthPcactEncryptedChallenge *output;
	GcAuthPcactRandomRecord *random;
} GcAuthPcactEncryptChallengeRequest;

typedef struct GcAuthPcactKirkChallenge {
	unsigned char format;
	unsigned char version;
	unsigned char mode;
	unsigned char reserved[13];
	GcAuthPcactChallengeBody plaintext;
	unsigned char trailer[16];
} GcAuthPcactKirkChallenge;

typedef struct SceSblGcAuthMgrSclkData1 {
	unsigned char format;
	unsigned char reservedPrefix[3];
	unsigned char version;
	unsigned char reserved[11];
	unsigned char ciphertext[32];
} SceSblGcAuthMgrSclkData1;

typedef struct GcAuthPcactRtcChallengeRequest {
	SceSblGcAuthMgrSclkData1 *output;
} GcAuthPcactRtcChallengeRequest;

typedef struct GcAuthPcactRtcPlaintext {
	unsigned char random[16];
	unsigned char salt[16];
} GcAuthPcactRtcPlaintext;

typedef struct SceSblGcAuthMgrSclkData2 {
	SceSblGcAuthMgrSclkData1 challenge;
	unsigned char signature[256];
} SceSblGcAuthMgrSclkData2;

typedef struct GcAuthPcactRtcActivationRequest {
	const SceSblGcAuthMgrSclkData2 *activation;
} GcAuthPcactRtcActivationRequest;

typedef struct GcAuthPcactRtcDecoded {
	unsigned char nonceHalves[2][8];
	unsigned char seconds[8];
	unsigned char reserved[8];
	unsigned char comparisonKey[16];
} GcAuthPcactRtcDecoded;

typedef union GcAuthPcactScratch {
	GcAuthPcactCertificate certificate;
	GcAuthPcactActivationBlock activation;
	GcAuthPcactDecodedKey decodedKey;
	GcAuthPsmactKeyEnvelope keyEnvelope;
	GcAuthPsmactDerivationInput derivation;
	GcAuthPcactHashInput hashInput;
	GcAuthPcactKirkChallenge kirkChallenge;
	GcAuthPcactEncryptedChallenge encryptedChallenge;
	GcAuthPcactRtcPlaintext rtcPlaintext;
	GcAuthPcactRtcDecoded rtcDecoded;
	unsigned char random[20];
	unsigned char sha256[32];
} GcAuthPcactScratch;

_Static_assert(sizeof(GcAuthPcactActivationBlock) == 80, "Pcact activation ABI");
_Static_assert(offsetof(GcAuthPcactActivationBlock, encryptedPayload) == 16,
	       "Pcact encrypted payload offset");
_Static_assert(sizeof(GcAuthPcactRandomRecord) == 64, "Pcact random record ABI");
_Static_assert(offsetof(GcAuthPcactRandomRecord, derivedKey) == 16,
	       "Pcact derived key offset");
_Static_assert(offsetof(GcAuthPcactRandomRecord, mode) == 32,
	       "Pcact random record mode offset");
_Static_assert(sizeof(GcAuthPcactKeyRequest) == 12, "Pcact key request ABI");
_Static_assert(offsetof(GcAuthPcactKeyRequest, random) == 4,
	       "Pcact random record pointer offset");
_Static_assert(offsetof(GcAuthPcactKeyRequest, outputKey) == 8,
	       "Pcact output key pointer offset");
_Static_assert(sizeof(GcAuthPcactCertificate) == 232, "Pcact certificate ABI");
_Static_assert(sizeof(GcAuthPcactCertificateRequest) == 4,
	       "Pcact certificate request ABI");
_Static_assert(sizeof(GcAuthPcactDecodedKey) == 32, "Pcact decoded key ABI");
_Static_assert(offsetof(GcAuthPcactDecodedKey, comparisonKey) == 16,
	       "Pcact comparison key offset");
_Static_assert(sizeof(GcAuthPcactScratch) == 232, "Pcact scratch ABI");
_Static_assert(sizeof(GcAuthPsmactChallengeHeader) == 16,
	       "Psmact challenge header ABI");
_Static_assert(sizeof(GcAuthPsmactChallengePayload) == 496,
	       "Psmact challenge payload ABI");
_Static_assert(offsetof(GcAuthPsmactChallengePayload, challenge.rtcSeconds) == 128,
	       "Psmact challenge timestamp offset");
_Static_assert(offsetof(GcAuthPsmactChallengePayload, challenge.challenge) == 136,
	       "Psmact challenge random offset");
_Static_assert(offsetof(GcAuthPsmactChallengePayload, challenge.derivationSeed) == 152,
	       "Psmact challenge seed offset");
_Static_assert(sizeof(GcAuthPsmactChallenge) == 1024, "Psmact challenge ABI");
_Static_assert(offsetof(GcAuthPsmactChallenge, payload) == 272,
	       "Psmact challenge payload offset");
_Static_assert(offsetof(GcAuthPsmactChallenge, signature) == 768,
	       "Psmact challenge signature offset");
_Static_assert(sizeof(GcAuthPsmactRandomRecord) == 64, "Psmact random record ABI");
_Static_assert(sizeof(GcAuthPsmactRecordHeader) == 16, "Psmact record header ABI");
_Static_assert(offsetof(GcAuthPsmactRandomRecord, rtcSeconds) == 16,
	       "Psmact random timestamp offset");
_Static_assert(offsetof(GcAuthPsmactRandomRecord, challenge) == 24,
	       "Psmact random challenge offset");
_Static_assert(offsetof(GcAuthPsmactRandomRecord, derivationSeed) == 40,
	       "Psmact random seed offset");
_Static_assert(sizeof(GcAuthPsmactChallengeRequest) == 16,
	       "Psmact challenge request ABI");
_Static_assert(sizeof(GcAuthPsmactKeyEnvelope) == 24, "Psmact key envelope ABI");
_Static_assert(sizeof(GcAuthPsmactActivation) == 512, "Psmact activation ABI");
_Static_assert(offsetof(GcAuthPsmactActivation, mac) == 480,
	       "Psmact activation MAC offset");
_Static_assert(sizeof(GcAuthPsmactDerivedKeys) == 64, "Psmact derived keys ABI");
_Static_assert(sizeof(GcAuthPsmactVerifyRequest) == 12, "Psmact verify request ABI");
_Static_assert(sizeof(GcAuthPsmactDerivationInput) == 37,
	       "Psmact derivation input ABI");
_Static_assert(sizeof(GcAuthPcactHashInput) == 132, "Pcact hash input ABI");
_Static_assert(sizeof(GcAuthPcactChallengeBody) == 96, "Pcact challenge body ABI");
_Static_assert(offsetof(GcAuthPcactChallengeBody, rtcSeconds) == 48,
	       "Pcact challenge timestamp offset");
_Static_assert(offsetof(GcAuthPcactChallengeBody, entropy) == 56,
	       "Pcact challenge entropy offset");
_Static_assert(sizeof(GcAuthPcactChallengePacket) == 112,
	       "Pcact challenge packet ABI");
_Static_assert(sizeof(GcAuthPcactChallengeWorkspace) == 112,
	       "Pcact challenge workspace ABI");
_Static_assert(sizeof(GcAuthPcactChallenge) == 128, "Pcact challenge ABI");
_Static_assert(sizeof(GcAuthPcactChallengeRequest) == 16,
	       "Pcact challenge request ABI");
_Static_assert(sizeof(GcAuthPcactChallengeInput) == 48,
	       "Pcact challenge input ABI");
_Static_assert(sizeof(GcAuthPcactEncryptedChallenge) == 128,
	       "Pcact encrypted challenge ABI");
_Static_assert(sizeof(GcAuthPcactEncryptChallengeRequest) == 20,
	       "Pcact encrypt challenge request ABI");
_Static_assert(sizeof(GcAuthPcactKirkChallenge) == 128,
	       "Pcact KIRK challenge ABI");
_Static_assert(offsetof(GcAuthPcactKirkChallenge, plaintext) == 16,
	       "Pcact KIRK plaintext offset");
_Static_assert(sizeof(SceSblGcAuthMgrSclkData1) == 48, "Pcact RTC challenge ABI");
_Static_assert(sizeof(GcAuthPcactRtcChallengeRequest) == 4,
	       "Pcact RTC request ABI");
_Static_assert(sizeof(GcAuthPcactRtcPlaintext) == 32, "Pcact RTC plaintext ABI");
_Static_assert(sizeof(SceSblGcAuthMgrSclkData2) == 304, "Pcact RTC activation ABI");
_Static_assert(sizeof(GcAuthPcactRtcActivationRequest) == 4,
	       "Pcact RTC activation request ABI");
_Static_assert(sizeof(GcAuthPcactRtcDecoded) == 48, "Pcact RTC decoded ABI");
_Static_assert(offsetof(GcAuthPcactRtcDecoded, comparisonKey) == 32,
	       "Pcact RTC nonce comparison offset");
_Static_assert(offsetof(GcAuthPcactHashInput, serializedChallenge) == 20,
	       "Pcact hash challenge offset");

extern int gcAuthPcactSemaphore;
extern GcAuthPcactScratch gcAuthPcactScratch;
extern GcAuthPcactScratch *gcAuthPcactScratchAddress;
extern const unsigned char gcAuthPcactActivationMask[16];
extern GcAuthActivationWorkspace gcAuthActivationWorkspace;
extern GcAuthActivationRandom gcAuthActivationRandom;
int GcAuthPcactDecryptChallenge(size_t size, int keyId);

/* drm_pcact.c */
int GcAuthPcactGetAesDecryptKey(const GcAuthPcactKeyRequest *request);
int GcAuthPcactGetCertificate(const GcAuthPcactCertificateRequest *request);
int GcAuthPsmactCreateChallenge(const GcAuthPsmactChallengeRequest *request);
int GcAuthPsmactVerifyActivation(const GcAuthPsmactVerifyRequest *request);

/* pcact_activation.c */
int GcAuthPcactGetAuthenticatedAesKey(const GcAuthPcactKeyRequest *request);
int GcAuthPcactCreateChallenge(const GcAuthPcactChallengeRequest *request);
int GcAuthPcactEncryptChallenge(const GcAuthPcactEncryptChallengeRequest *request);
int GcAuthPcactCreateRtcChallenge(const GcAuthPcactRtcChallengeRequest *request);
int GcAuthPcactApplyRtcActivation(const GcAuthPcactRtcActivationRequest *request);
int GcAuthInitializePcact(void);

/* pcact_api.c */
int GcAuthPcactEncryptChallengeWithStack(unsigned char mode,
		const GcAuthPcactChallengeInput *input,
		const unsigned char *rtcSeconds, GcAuthPcactEncryptedChallenge *output,
		GcAuthPcactRandomRecord *random);
int GcAuthPcactGetAesDecryptKeyWithStack(const GcAuthPcactActivationBlock *activation,
		const GcAuthPcactRandomRecord *random, unsigned char *outputKey);
int GcAuthPsmactCreateChallengeWithStack(unsigned char mode,
		const unsigned char *rtcSeconds, GcAuthPsmactChallenge *output,
		GcAuthPsmactRandomRecord *random);
int GcAuthPsmactVerifyActivationWithStack(const GcAuthPsmactActivation *activation,
		const GcAuthPsmactRandomRecord *random, GcAuthPsmactDerivedKeys *output);
int GcAuthPcactCreateRtcChallengeWithStack(SceSblGcAuthMgrSclkData1 *output);
int GcAuthPcactApplyRtcActivationWithStack(const SceSblGcAuthMgrSclkData2 *activation);

/* pcact_control.c */
int GcAuthPcactBeginActivation(const GcAuthPcactBeginActivationRequest *request);
int GcAuthPcactApplyActivation(const GcAuthPcactApplyActivationRequest *request);
int GcAuthPsmactBeginActivation(const GcAuthPsmactBeginActivationRequest *request);
int GcAuthPsmactDecryptActivation(const GcAuthPsmactDerivedKeys *keys);

/* pcact_finish.c */
int GcAuthResetState(void);
int GcAuthPcactBeginActivationWithStack(unsigned int mode,
		const unsigned int *entropy, GcAuthPcactEncryptedChallenge *output);
int GcAuthPcactApplyActivationWithStack(const GcAuthPcactActivationBundle *activation,
		unsigned int size);
