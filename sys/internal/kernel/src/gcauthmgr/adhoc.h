#ifndef GCAUTH_ADHOC_H
#define GCAUTH_ADHOC_H

#include <stddef.h>
#include <stdint.h>
#include "pcact.h"

typedef union GcAuthAdhocCertificate160 {
	unsigned char serialized[184];
	struct {
		GcAuthAdhocSignedCertificate160 signedCertificate;
		GcAuthAdhocPublicKey160 publicKey;
		unsigned char encryptedPrivateKey[32];
		unsigned char encryptedCmac[16];
	} fields;
} GcAuthAdhocCertificate160;

typedef union GcAuthAdhocCertificate {
	GcAuthPcactCertificate certificate224;
	GcAuthAdhocCertificate160 certificate160;
} GcAuthAdhocCertificate;

typedef struct GcAuthAdhocSign160Input {
	union {
		unsigned char encryptedPrivateKey[32];
		unsigned char zero[32];
	};
	unsigned char digest[20];
} GcAuthAdhocSign160Input;

typedef struct GcAuthAdhocSign224Input {
	union {
		unsigned char encryptedPrivateKey[32];
		unsigned char zero[32];
	};
	unsigned char digest[28];
} GcAuthAdhocSign224Input;

typedef struct GcAuthAdhocVerify160Input {
	GcAuthAdhocPublicKey160 publicKey;
	unsigned char digest[20];
	GcAuthAdhocSignature160 signature;
} GcAuthAdhocVerify160Input;

typedef struct GcAuthAdhocVerify224Input {
	GcAuthAdhocPublicKey224 publicKey;
	unsigned char digest[28];
	GcAuthAdhocSignature224 signature;
} GcAuthAdhocVerify224Input;

typedef union GcAuthAdhocVerifyScratch {
	GcAuthAdhocVerify160Input verify160;
	GcAuthAdhocVerify224Input verify224;
} GcAuthAdhocVerifyScratch;

typedef union GcAuthAdhocSigningScratch {
	GcAuthAdhocSign160Input input160;
	GcAuthAdhocSign224Input input224;
	GcAuthAdhocSignature160 signature160;
	GcAuthAdhocSignature224 signature224;
} GcAuthAdhocSigningScratch;

typedef struct GcAuthAdhocSign160Request {
	const unsigned char *digest;
	GcAuthAdhocSignature160 *signature;
	int keyId;
} GcAuthAdhocSign160Request;

typedef struct GcAuthAdhocVerifyPair160 {
	const unsigned char *digest;
	const GcAuthAdhocSignature160 *signature;
} GcAuthAdhocVerifyPair160;

typedef struct GcAuthAdhocSign224Request {
	const unsigned char *digest;
	GcAuthAdhocSignature224 *signature;
	int keyId;
} GcAuthAdhocSign224Request;

typedef struct GcAuthAdhocHashMessage {
	unsigned int size;
	unsigned char payload[];
} GcAuthAdhocHashMessage;

typedef struct GcAuthAdhocHash224Input {
	unsigned int size;
	unsigned char payload[28 + 128];
} GcAuthAdhocHash224Input;

typedef union GcAuthAdhocHashScratch {
	GcAuthAdhocHash224Input input;
	GcAuthAdhocHashMessage hashMessage;
	unsigned char digest160[20];
	unsigned char digest224[28];
} GcAuthAdhocHashScratch;

typedef struct GcAuthAdhocCounterBlock {
	unsigned int counterLow;
	unsigned int counterHigh;
	unsigned int seedLow;
	unsigned int seedHigh;
} GcAuthAdhocCounterBlock;

typedef struct GcAuthAdhocCounterInput {
	unsigned int mode;
	unsigned int zero[2];
	unsigned int keyId;
	unsigned int size;
	GcAuthAdhocCounterBlock blocks[2];
} GcAuthAdhocCounterInput;

typedef struct GcAuthAdhocCounterStream {
	unsigned int mode;
	unsigned int zero[2];
	unsigned int keyId;
	unsigned int size;
	GcAuthAdhocCounterBlock blocks[];
} GcAuthAdhocCounterStream;

typedef union GcAuthAdhocCounterScratch {
	GcAuthAdhocCounterInput input;
	unsigned int keystream[8];
} GcAuthAdhocCounterScratch;

typedef struct GcAuthAdhocAuthHashInput {
	unsigned int size;
	unsigned char payload[68];
} GcAuthAdhocAuthHashInput;

typedef union GcAuthAdhocAuthScratch {
	GcAuthAdhocCounterScratch counter;
	GcAuthAdhocAuthHashInput hashInput;
	GcAuthAdhocHashMessage hashMessage;
	unsigned char digest[20];
} GcAuthAdhocAuthScratch;

typedef union GcAuthAdhocAuthData {
	unsigned char bytes[32];
	unsigned int words[8];
} GcAuthAdhocAuthData;

typedef struct GcAuthAdhocAuthTag {
	unsigned char counter[8];
	unsigned char zero[8];
	unsigned char hash[16];
} GcAuthAdhocAuthTag;

typedef union GcAuthAdhocPacketPrefix {
	struct {
		unsigned int mode;
		unsigned int zero[2];
		unsigned int keyId;
		unsigned int size;
		unsigned char entropy[8];
		unsigned char zeroFill[8];
	} fields;
	unsigned int words[9];
} GcAuthAdhocPacketPrefix;

typedef struct GcAuthAdhocCipherInput {
	GcAuthAdhocPacketPrefix prefix;
	unsigned char payload[0x900];
} GcAuthAdhocCipherInput;

typedef struct GcAuthAdhocBulkHashInput {
	unsigned int size;
	unsigned char payload[20 + 16 + 0x910];
} GcAuthAdhocBulkHashInput;

typedef struct GcAuthAdhocSignedMessage160 {
	GcAuthAdhocPublicKey160 localPublicKey;
	GcAuthAdhocPublicKey160 remotePublicKey;
	unsigned char certificatePrefix[16];
} GcAuthAdhocSignedMessage160;

typedef struct GcAuthAdhocSignedMessage224 {
	GcAuthAdhocPublicKey224 localPublicKey;
	GcAuthAdhocPublicKey224 remotePublicKey;
	unsigned char certificatePrefix[16];
} GcAuthAdhocSignedMessage224;

typedef struct GcAuthAdhocResponse160 {
	GcAuthAdhocSignedMessage160 message;
	GcAuthAdhocSignature160 signature;
	unsigned char digest[20];
	GcAuthAdhocAuthData data;
	unsigned char counter[8];
	GcAuthAdhocAuthTag tag;
	unsigned char alignmentTail[12];
} GcAuthAdhocResponse160;

typedef struct GcAuthAdhocDecryptedResponse160 {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	GcAuthAdhocResponse160 payload;
} GcAuthAdhocDecryptedResponse160;

typedef struct GcAuthAdhocResponse224 {
	GcAuthAdhocSignedMessage224 message;
	GcAuthAdhocSignature224 signature;
	unsigned char digest[28];
	GcAuthAdhocAuthData data;
	unsigned char counter[8];
	GcAuthAdhocAuthTag tag;
	unsigned char alignmentTail[4];
} GcAuthAdhocResponse224;

typedef struct GcAuthAdhocDecryptedResponse224 {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	GcAuthAdhocResponse224 payload;
} GcAuthAdhocDecryptedResponse224;

typedef struct GcAuthAdhocAuthenticatedResponse160 {
	GcAuthAdhocSignedMessage160 message;
	GcAuthAdhocSignature160 signature;
	unsigned char digest[20];
	GcAuthAdhocPeerCertificate160 certificate;
	GcAuthAdhocSignature160 certificateSignature;
	unsigned char alignmentTail[4];
} GcAuthAdhocAuthenticatedResponse160;

typedef struct GcAuthAdhocDecryptedAuthenticatedResponse160 {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	GcAuthAdhocAuthenticatedResponse160 payload;
} GcAuthAdhocDecryptedAuthenticatedResponse160;

typedef struct GcAuthAdhocAuthenticatedResponse224 {
	GcAuthAdhocSignedMessage224 message;
	GcAuthAdhocSignature224 signature;
	unsigned char digest[28];
	GcAuthAdhocPeerCertificate224 certificate;
	GcAuthAdhocSignature224 certificateSignature;
	unsigned char alignmentTail[12];
} GcAuthAdhocAuthenticatedResponse224;

typedef struct GcAuthAdhocDecryptedAuthenticatedResponse224 {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	GcAuthAdhocAuthenticatedResponse224 payload;
} GcAuthAdhocDecryptedAuthenticatedResponse224;

typedef struct GcAuthAdhocChallenge224 {
	GcAuthAdhocPublicKey224 publicKey;
	GcAuthAdhocSignedCertificate224 signedCertificate;
	unsigned char alignmentTail[8];
} GcAuthAdhocChallenge224;

typedef struct GcAuthAdhocDecryptedChallenge224 {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	GcAuthAdhocChallenge224 payload;
} GcAuthAdhocDecryptedChallenge224;

typedef struct GcAuthAdhocChallenge160 {
	GcAuthAdhocPublicKey160 publicKey;
	GcAuthAdhocSignedCertificate160 signedCertificate;
	unsigned char alignmentTail[8];
} GcAuthAdhocChallenge160;

typedef struct GcAuthAdhocDecryptedChallenge160 {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	GcAuthAdhocChallenge160 payload;
} GcAuthAdhocDecryptedChallenge160;

typedef struct GcAuthAdhocDecryptedPacket {
	unsigned char entropy[8];
	unsigned char zeroFill[8];
	unsigned char payload[];
} GcAuthAdhocDecryptedPacket;

typedef union GcAuthAdhocWorkspace {
	GcAuthAdhocCipherInput cipher;
	GcAuthAdhocDecryptedPacket decrypted;
	GcAuthAdhocDecryptedResponse160 response160;
	GcAuthAdhocDecryptedResponse224 response224;
	GcAuthAdhocDecryptedAuthenticatedResponse160 authenticatedResponse160;
	GcAuthAdhocDecryptedAuthenticatedResponse224 authenticatedResponse224;
	GcAuthAdhocDecryptedChallenge224 challenge224;
	GcAuthAdhocDecryptedChallenge160 challenge160;
	GcAuthAdhocCounterStream counterStream;
	GcAuthAdhocBulkHashInput hashInput;
	GcAuthAdhocHashMessage hashMessage;
	GcAuthAdhocCounterScratch counter;
	GcAuthAdhocAuthScratch auth;
} GcAuthAdhocWorkspace;

typedef struct GcAuthAdhocKeyPair160 {
	unsigned char privateKey[20];
	GcAuthAdhocPublicKey160 publicKey;
} GcAuthAdhocKeyPair160;

typedef struct GcAuthAdhocKeyPair224 {
	unsigned char privateKey[28];
	GcAuthAdhocPublicKey224 publicKey;
} GcAuthAdhocKeyPair224;

typedef union GcAuthAdhocGeneratedKeyPair {
	GcAuthAdhocKeyPair160 key160;
	GcAuthAdhocKeyPair224 key224;
} GcAuthAdhocGeneratedKeyPair;

typedef union GcAuthAdhocMultiplyScratch {
	GcAuthAdhocKeyPair160 input160;
	GcAuthAdhocKeyPair224 input224;
	GcAuthAdhocPublicKey160 output160;
	GcAuthAdhocPublicKey224 output224;
} GcAuthAdhocMultiplyScratch;

typedef union GcAuthAdhocContext160 {
	struct {
		GcAuthAdhocKeyPair160 keyPair;
		unsigned char certificatePrefix[16];
		unsigned char zeroFill[116];
	} initialized;
	struct {
		GcAuthAdhocPublicKey160 verificationKey;
		GcAuthAdhocPublicKey160 localPublicKey;
		GcAuthAdhocPublicKey160 remotePublicKey;
		unsigned char certificatePrefix[16];
		unsigned char sharedSecret[20];
		unsigned char zeroFill[36];
	} pending;
	struct {
		unsigned char seed[20];
		GcAuthAdhocAuthData data;
		unsigned char peerCounter[8];
		unsigned char sendCounter[8];
		unsigned char receiveCounter[8];
		unsigned char zeroFill[116];
	} ready;
} GcAuthAdhocContext160;

typedef union GcAuthAdhocContext224 {
	struct {
		GcAuthAdhocKeyPair224 keyPair;
		unsigned char certificatePrefix[16];
		unsigned char zeroFill[156];
	} initialized;
	struct {
		GcAuthAdhocPublicKey224 verificationKey;
		GcAuthAdhocPublicKey224 localPublicKey;
		GcAuthAdhocPublicKey224 remotePublicKey;
		unsigned char certificatePrefix[16];
		unsigned char sharedSecret[28];
		unsigned char zeroFill[44];
	} pending;
	struct {
		unsigned char seed[28];
		GcAuthAdhocAuthData data;
		unsigned char peerCounter[8];
		unsigned char zeroFill[188];
	} ready;
} GcAuthAdhocContext224;

typedef struct GcAuthAdhocGetSeeds160Request {
	const GcAuthAdhocContext160 *context;
	unsigned char *random;
	unsigned char *contextRandom;
	unsigned char *seed;
} GcAuthAdhocGetSeeds160Request;

typedef struct GcAuthAdhocGetSeeds224Request {
	const GcAuthAdhocContext224 *context;
	unsigned char *random;
	unsigned char *contextRandom;
	unsigned char *seed;
} GcAuthAdhocGetSeeds224Request;

typedef struct GcAuthAdhocCreateContext224Request {
	GcAuthAdhocContext224 *context;
	unsigned char *packet;
} GcAuthAdhocCreateContext224Request;

typedef struct GcAuthAdhocCreateContext160Request {
	GcAuthAdhocContext160 *context;
	unsigned char *packet;
} GcAuthAdhocCreateContext160Request;

typedef union GcAuthAdhocFinalPacket {
	unsigned char ciphertext[96];
	struct {
		GcAuthAdhocAuthData data;
		unsigned char counter[8];
		GcAuthAdhocAuthTag tag;
		unsigned char transportTail[24];
	} decoded;
} GcAuthAdhocFinalPacket;

typedef struct GcAuthAdhocFinishContext160Request {
	GcAuthAdhocContext160 *context;
	const GcAuthAdhocFinalPacket *packet;
} GcAuthAdhocFinishContext160Request;

typedef struct GcAuthAdhocFinishContext224Request {
	GcAuthAdhocContext224 *context;
	const GcAuthAdhocFinalPacket *packet;
} GcAuthAdhocFinishContext224Request;

typedef struct GcAuthAdhocVerifyResponse160Request {
	GcAuthAdhocContext160 *context;
	const unsigned char *packet;
	unsigned char *output;
} GcAuthAdhocVerifyResponse160Request;

typedef struct GcAuthAdhocAuthenticateResponse160Request {
	GcAuthAdhocContext160 *context;
	const unsigned char *packet;
	unsigned char *output;
} GcAuthAdhocAuthenticateResponse160Request;

typedef struct GcAuthAdhocVerifyResponse224Request {
	GcAuthAdhocContext224 *context;
	const unsigned char *packet;
	unsigned char *output;
} GcAuthAdhocVerifyResponse224Request;

typedef struct GcAuthAdhocAuthenticateResponse224Request {
	GcAuthAdhocContext224 *context;
	const unsigned char *packet;
	unsigned char *output;
} GcAuthAdhocAuthenticateResponse224Request;

typedef struct GcAuthAdhocAnswerChallenge224Request {
	GcAuthAdhocContext224 *context;
	const unsigned char *packet;
	unsigned char *output;
} GcAuthAdhocAnswerChallenge224Request;

typedef struct GcAuthAdhocAnswerChallenge160Request {
	GcAuthAdhocContext160 *context;
	const unsigned char *packet;
	unsigned char *output;
} GcAuthAdhocAnswerChallenge160Request;

_Static_assert(sizeof(GcAuthAdhocChallenge160) == 144,
	"challenge160 payload size");
_Static_assert(sizeof(GcAuthAdhocDecryptedChallenge160) == 160,
	"decrypted challenge160 size");

_Static_assert(sizeof(GcAuthAdhocSignedCertificate160) == 96,
	"signed certificate160 size");
_Static_assert(sizeof(GcAuthAdhocSignedCertificate224) == 128,
	"signed certificate224 size");
_Static_assert(sizeof(GcAuthAdhocChallenge224) == 192,
	"challenge224 payload size");
_Static_assert(sizeof(GcAuthAdhocDecryptedChallenge224) == 208,
	"decrypted challenge224 size");

_Static_assert(sizeof(GcAuthAdhocPeerCertificate224) == 72,
	"peer certificate224 size");
_Static_assert(sizeof(GcAuthAdhocAuthenticatedResponse224) == 352,
	"authenticated response224 payload size");
_Static_assert(sizeof(GcAuthAdhocDecryptedAuthenticatedResponse224) == 368,
	"decrypted authenticated response224 size");

_Static_assert(sizeof(GcAuthAdhocSignedMessage224) == 128,
	"signed message224 size");
_Static_assert(sizeof(GcAuthAdhocResponse224) == 288, "response224 payload size");
_Static_assert(sizeof(GcAuthAdhocDecryptedResponse224) == 304,
	"decrypted response224 size");

_Static_assert(sizeof(GcAuthAdhocCertificate160) == 184, "certificate160 size");
_Static_assert(offsetof(GcAuthAdhocCertificate160, fields.publicKey) == 96,
	"certificate160 public key offset");
_Static_assert(offsetof(GcAuthAdhocCertificate160, fields.encryptedPrivateKey) == 136,
	"certificate160 encrypted private key offset");
_Static_assert(offsetof(GcAuthAdhocCertificate160, fields.encryptedCmac) == 168,
	"certificate160 encrypted CMAC offset");
_Static_assert(sizeof(GcAuthAdhocMultiplyScratch) == 84, "multiply scratch size");
_Static_assert(sizeof(GcAuthAdhocPeerCertificate160) == 56,
	"peer certificate160 size");
_Static_assert(sizeof(GcAuthAdhocAuthenticatedResponse160) == 256,
	"authenticated response160 payload size");
_Static_assert(sizeof(GcAuthAdhocDecryptedAuthenticatedResponse160) == 272,
	"decrypted authenticated response160 size");

_Static_assert(sizeof(GcAuthAdhocKeyPair160) == 60, "key pair160 size");
_Static_assert(sizeof(GcAuthAdhocFinalPacket) == 96, "final packet size");
_Static_assert(sizeof(GcAuthAdhocSignedMessage160) == 96, "signed message160 size");
_Static_assert(sizeof(GcAuthAdhocResponse160) == 240, "response160 payload size");
_Static_assert(sizeof(GcAuthAdhocDecryptedResponse160) == 256,
	"decrypted response160 size");
_Static_assert(offsetof(GcAuthAdhocDecryptedPacket, payload) == 16,
	"decrypted payload offset");
_Static_assert(sizeof(GcAuthAdhocKeyPair224) == 84, "key pair224 size");
_Static_assert(sizeof(GcAuthAdhocContext160) == 192, "context160 size");
_Static_assert(sizeof(GcAuthAdhocContext224) == 256, "context224 size");
_Static_assert(offsetof(GcAuthAdhocContext160, ready.data) == 20,
	"context160 data offset");
_Static_assert(offsetof(GcAuthAdhocContext224, ready.data) == 28,
	"context224 data offset");
_Static_assert(sizeof(GcAuthAdhocGetSeeds160Request) == 16, "seed request size");
_Static_assert(sizeof(GcAuthAdhocSign160Input) == 52, "sign160 input size");
_Static_assert(sizeof(GcAuthAdhocSign224Input) == 60, "sign224 input size");
_Static_assert(offsetof(GcAuthAdhocSign224Input, digest) == 32,
	"signing digest offset");
_Static_assert(sizeof(GcAuthAdhocSignature160) == 40, "signature160 size");
_Static_assert(sizeof(GcAuthAdhocSignature224) == 56, "signature224 size");
_Static_assert(sizeof(GcAuthAdhocPublicKey160) == 40, "public key160 size");
_Static_assert(sizeof(GcAuthAdhocPublicKey224) == 56, "public key224 size");
_Static_assert(sizeof(GcAuthAdhocVerify160Input) == 100, "verify160 input size");
_Static_assert(sizeof(GcAuthAdhocVerify224Input) == 140, "verify224 input size");
_Static_assert(sizeof(GcAuthAdhocVerifyScratch) == 140, "verification scratch size");
_Static_assert(sizeof(GcAuthAdhocSigningScratch) == 60, "signing scratch size");
_Static_assert(sizeof(GcAuthAdhocSign160Request) == 12, "sign160 request size");
_Static_assert(sizeof(GcAuthAdhocHashScratch) == 160, "hash scratch size");
_Static_assert(sizeof(GcAuthAdhocVerifyPair160) == 8, "verify160 pair size");
_Static_assert(sizeof(GcAuthAdhocSign224Request) == 12, "sign224 request size");
_Static_assert(offsetof(GcAuthAdhocHashMessage, payload) == 4,
	"hash message payload offset");
_Static_assert(sizeof(GcAuthAdhocCounterBlock) == 16, "counter block size");
_Static_assert(offsetof(GcAuthAdhocCounterStream, blocks) == 20,
	"counter stream blocks offset");
_Static_assert(offsetof(GcAuthAdhocCounterInput, blocks) == 20,
	"counter input blocks offset");
_Static_assert(sizeof(GcAuthAdhocCounterScratch) == 52,
	"counter scratch size");
_Static_assert(sizeof(GcAuthAdhocAuthScratch) == 72, "auth scratch size");
_Static_assert(sizeof(GcAuthAdhocAuthData) == 32, "auth data size");
_Static_assert(sizeof(GcAuthAdhocAuthTag) == 32, "auth tag size");
_Static_assert(offsetof(GcAuthAdhocAuthTag, hash) == 16, "auth tag hash offset");
_Static_assert(sizeof(GcAuthAdhocPacketPrefix) == 36, "packet prefix size");
_Static_assert(sizeof(GcAuthAdhocCertificate) == 232, "certificate size");
_Static_assert(sizeof(GcAuthAdhocCipherInput) == 0x924, "cipher input size");
_Static_assert(sizeof(GcAuthAdhocWorkspace) == 0x938, "workspace size");
_Static_assert(offsetof(GcAuthAdhocCipherInput, payload) == 36,
	"packet payload offset");

/* drm_adhoc.c */
void GcAuthAdhocBuildMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
void GcAuthAdhocBuildExtendedMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);

/* drm_adhoc_response224.c */
int GcAuthAdhocVerifyResponse224(const GcAuthAdhocVerifyResponse224Request *request);

/* drm_adhoc_finish224.c */
int GcAuthAdhocFinishContext224(const GcAuthAdhocFinishContext224Request *request);

/* drm_adhoc_seeds.c */
int GcAuthAdhocGetSeeds224(const GcAuthAdhocGetSeeds224Request *request);

/* drm_adhoc_verify_request160.c */
int GcAuthAdhocVerifyFixed160(const GcAuthAdhocVerifyPair160 *request);

/* drm_adhoc_sign.c */
int GcAuthAdhocSign160(const GcAuthAdhocSign160Request *request);
int GcAuthAdhocSign224(const GcAuthAdhocSign224Request *request);

/* drm_adhoc_verify_request160.c, drm_adhoc_verify_request224.c */
int GcAuthAdhocVerifyRequest160(const GcAuthAdhocSign160Request *request);
int GcAuthAdhocVerifyRequest224(const GcAuthAdhocSign224Request *request);

/* the 160-bit exchange (drm_adhoc_*160.c, drm_adhoc_seeds.c) */
int GcAuthAdhocCreateContext160(const GcAuthAdhocCreateContext160Request *request);
int GcAuthAdhocAnswerChallenge160(const GcAuthAdhocAnswerChallenge160Request *request);
int GcAuthAdhocAuthenticateResponse160(const GcAuthAdhocAuthenticateResponse160Request *request);
int GcAuthAdhocVerifyResponse160(const GcAuthAdhocVerifyResponse160Request *request);
int GcAuthAdhocFinishContext160(const GcAuthAdhocFinishContext160Request *request);
int GcAuthAdhocGetSeeds160(const GcAuthAdhocGetSeeds160Request *request);

typedef struct GcAuthAdhocUniCastEncrypt160Request {
	GcAuthAdhocContext160 *context;
	unsigned char *data;
	unsigned int size;
	GcAuthAdhocAuthTag *tag;
	int encrypt;
} GcAuthAdhocUniCastEncrypt160Request;

/* drm_adhoc_unicast160.c */
int GcAuthAdhocUniCastEncrypt160(GcAuthAdhocContext160 *context,
		unsigned char *data, unsigned int size, GcAuthAdhocAuthTag *tag, int encrypt);
int GcAuthAdhocUniCastEncrypt160Stack(const GcAuthAdhocUniCastEncrypt160Request *request);

/* drm_adhoc_authenticate224.c */
int GcAuthAdhocAuthenticateResponse224(const GcAuthAdhocAuthenticateResponse224Request *request);

/* drm_adhoc_cbc_auth.c */
int GcAuthAdhocEncryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag, int encrypt);
int GcAuthAdhocDecryptCbcAuthData(GcAuthAdhocWorkspace *scratch,
		unsigned char *data, unsigned int size, uint64_t *counter,
		const uint64_t *minimumCounter, const unsigned int *seed,
		const GcAuthAdhocAuthTag *tag, int decrypt, int checkCounter);

/* drm_adhoc_cert.c */
int GcAuthAdhocReadCertificate224(void);

/* drm_adhoc_cert160.c */
int GcAuthAdhocReadCertificate160(void);

/* drm_adhoc_challenge224.c */
int GcAuthAdhocAnswerChallenge224(const GcAuthAdhocAnswerChallenge224Request *request);

/* drm_adhoc_clear.c */
int GcAuthAdhocClearState(void);

/* drm_adhoc_counter.c */
int GcAuthAdhocGenerateCbcCounterStream(GcAuthAdhocCounterStream *stream,
		unsigned int size, uint64_t *counter, const unsigned int *seed);

/* drm_adhoc_create224.c */
int GcAuthAdhocCreateContext224(const GcAuthAdhocCreateContext224Request *request);

/* drm_adhoc_hash224.c */
int GcAuthAdhocHash224Spans(const unsigned char *first,
		unsigned int firstSize, const unsigned char *second,
		unsigned int secondSize);

/* drm_adhoc_init.c */
int GcAuthInitializeAdhocBb(void);

/* drm_adhoc_packet.c */
int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size);

/* drm_adhoc_sign.c */
int GcAuthAdhocHashSpans(GcAuthAdhocHashMessage *message,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize);
int GcAuthAdhocEncryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter,
		const unsigned int *seed, GcAuthAdhocAuthTag *tag);
int GcAuthAdhocDecryptAuthData(GcAuthAdhocAuthScratch *scratch,
		GcAuthAdhocAuthData *data, uint64_t *counter, int unused,
		const unsigned int *seed, const GcAuthAdhocAuthTag *tag);
int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize);

/* drm_adhoc_state.c */
int GcAuthAdhocInitializeState(void);

/* drm_adhoc_verify160.c */
int GcAuthAdhocVerify160(const GcAuthAdhocPublicKey160 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature160 *signature);

/* drm_adhoc_verify224.c */
int GcAuthAdhocVerify224(const GcAuthAdhocPublicKey224 *publicKey,
		const unsigned char *digest, const GcAuthAdhocSignature224 *signature);

#endif
