#ifndef MSIF_AUTH_H
#define MSIF_AUTH_H

#include "library.h"
#include "msif_ecdsa.h"
#include <sysmem/aes.h>

union MsifEcdsaParameters {
	struct {
		uint8_t publicX[28];
		uint8_t publicY[28];
		uint8_t prime[28];
		uint8_t coefficientA[28];
		uint8_t coefficientB[28];
		uint8_t order[28];
		uint8_t baseX[28];
		uint8_t baseY[28];
	};
	uint8_t bytes[224];
	uint64_t words[28];
};

struct MsifParameterCbcContext {
	SceAesFullContext aes;
	uint8_t chain[32];
};

_Static_assert(sizeof(union MsifEcdsaParameters) == 224,
	       "media ECDSA parameters");
_Static_assert(sizeof(struct MsifParameterCbcContext) == 1008,
	       "media parameter CBC state");

union MsifMacBlock {
	uint64_t value;
	uint8_t bytes[8];
};

struct MsifMediaAuthData {
	uint8_t mediaId[16];
	uint8_t mediaParameters[7];
	uint8_t capacityExponent;
	uint8_t cardChallenge[8];
};

union MsifMediaAuthMessage {
	struct {
		struct MsifMediaAuthData card;
		uint64_t hostChallenge;
	};
	uint64_t blocks[5];
};

union MsifAuthTransfer {
	struct {
		uint8_t hostChallenge[8];
		uint8_t generation;
		uint8_t reserved[23];
	} request;
	struct {
		struct MsifMediaAuthData card;
		uint8_t mac[8];
		uint8_t serviceData[24];
	} response;
	struct {
		uint8_t mac[8];
		uint8_t reserved[24];
	} proof;
};

_Static_assert(sizeof(struct MsifMediaAuthData) == 32, "media-auth card data");
_Static_assert(sizeof(union MsifMediaAuthMessage) == 40, "media-auth MAC message");
_Static_assert(sizeof(union MsifAuthTransfer) == 64, "media-auth transfer buffer");

int get_ecdsa_param(struct MsifEcdsaPoint *publicKey, struct MsifEcdsaCurve *curve);

int MsifComputeMediaMac(uint64_t *mac, const uint64_t *data, unsigned int length);
int MsifGetSystemMediaId(uint8_t *mediaId);
int MsifAuthenticateMedia(struct MsproalHandle *handle, uint8_t *mediaId);
int msif_check_mid(struct MsproalHandle *handle, const uint8_t *mediaId);
int msif_check_capacity(int capacityExponent);

#endif
