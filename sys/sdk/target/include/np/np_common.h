/*
 * np/np_common.h: the NP library's common definitions, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/np/np_common.h), as user code needs them.
 */
#ifndef _SCE_NP_NP_COMMON_H_
#define _SCE_NP_NP_COMMON_H_

#include <scetypes.h>
#include <np/np_npid.h>

/* Communication ID */
typedef struct SceNpCommunicationId {
	char data[9];
	char term;
	SceUChar8 num;
	char dummy;
} SceNpCommunicationId;

/* Communication Passphrase */
#define SCE_NP_COMMUNICATION_PASSPHRASE_SIZE		(128)
typedef struct SceNpCommunicationPassphrase {
	SceUChar8 data[SCE_NP_COMMUNICATION_PASSPHRASE_SIZE];
} SceNpCommunicationPassphrase;

/* Communication Signature */
#define SCE_NP_COMMUNICATION_SIGNATURE_SIZE		(160)
typedef struct SceNpCommunicationSignature {
	SceUChar8 data[SCE_NP_COMMUNICATION_SIGNATURE_SIZE];
} SceNpCommunicationSignature;

/* Communication Config */
typedef struct SceNpCommunicationConfig {
	const SceNpCommunicationId		*commId;
	const SceNpCommunicationPassphrase	*commPassphrase;
	const SceNpCommunicationSignature	*commSignature;
} SceNpCommunicationConfig;

/* NP country code (ISO 3166-1 two-letter system) */
typedef struct SceNpCountryCode {
	char data[2];
	char term;
	char padding[1];
} SceNpCountryCode;

/* NP Avatar URL */
#define SCE_NP_AVATAR_URL_MAX_LENGTH		(127)
typedef struct SceNpAvatarUrl {
	char data[SCE_NP_AVATAR_URL_MAX_LENGTH];
	char term;
} SceNpAvatarUrl;

typedef SceInt32 SceNpPlatformType;
#define SCE_NP_PLATFORM_TYPE_NONE		0
#define SCE_NP_PLATFORM_TYPE_PS3		1
#define SCE_NP_PLATFORM_TYPE_VITA		2
#define SCE_NP_PLATFORM_TYPE_PS4		3

/* NP Client ID */
#define SCE_NP_CLIENT_ID_MAX_LEN 128
typedef struct SceNpClientId {
	char id[SCE_NP_CLIENT_ID_MAX_LEN + 1];
	SceUInt8 padding[7];
} SceNpClientId;

/* NP Authorization Code */
#define SCE_NP_AUTHORIZATION_CODE_MAX_LEN 128
typedef struct SceNpAuthorizationCode {
	char code[SCE_NP_AUTHORIZATION_CODE_MAX_LEN + 1];
	SceUInt8 padding[7];
} SceNpAuthorizationCode;

/* NP Session ID */
#define SCE_NP_SESSION_ID_MAX_SIZE		(45)
typedef struct SceNpSessionId {
	char data[SCE_NP_SESSION_ID_MAX_SIZE];
	char term;
	char padding[2];
} SceNpSessionId;

#ifdef __cplusplus
extern "C" {
#endif

int sceNpCmpNpId(const SceNpId *npid1, const SceNpId *npid2);
int sceNpCmpNpIdInOrder(const SceNpId *npid1, const SceNpId *npid2, int *order);
int sceNpCmpOnlineId(const SceNpId *npid1, const SceNpId *npid2);
int sceNpGetPlatformType(const SceNpId *npId);
int sceNpSetPlatformType(SceNpId *npId, SceNpPlatformType platformType);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_NP_COMMON_H_ */
