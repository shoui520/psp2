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

#define SCE_NP_ABOUT_ME_MAX_LENGTH		63
#define SCE_NP_AVATAR_IMAGE_MAX_SIZE_LARGE	(200 * 1024U)
#define SCE_NP_AVATAR_IMAGE_MAX_SIZE_MIDDLE	(100 * 1024U)
#define SCE_NP_AVATAR_IMAGE_MAX_SIZE_SMALL	(10 * 1024U)
#define SCE_NP_AVATAR_IMAGE_MAX_SIZE		SCE_NP_AVATAR_IMAGE_MAX_SIZE_LARGE

/* NP USER INFO */
typedef struct SceNpUserInformation {
	SceNpId userId;
	SceNpAvatarUrl icon;		/* Avatar URL */
	SceUChar8 reserved[52];
} SceNpUserInformation;

/* My Languages */
typedef struct SceNpMyLanguages {
	SceInt32 language1;		/* It will always be set */
	SceInt32 language2;		/* Negative when not set */
	SceInt32 language3;		/* Negative when not set */
	SceUChar8 padding[4];
} SceNpMyLanguages;

/* NP Avatar Image(for Large Size) */
typedef struct SceNpAvatarImage {
	SceUChar8 data[SCE_NP_AVATAR_IMAGE_MAX_SIZE];
	SceSize size;
	SceUChar8 reserved[12];
} SceNpAvatarImage;

typedef enum {
	SCE_NP_AVATAR_SIZE_LARGE,
	SCE_NP_AVATAR_SIZE_MIDDLE,
	SCE_NP_AVATAR_SIZE_SMALL
} SceNpAvatarSizeType;

typedef struct SceNpAboutMe {
	char data[SCE_NP_ABOUT_ME_MAX_LENGTH + 1];
} SceNpAboutMe;

typedef SceInt32 SceNpPlatformType;
#define SCE_NP_PLATFORM_TYPE_NONE		0
#define SCE_NP_PLATFORM_TYPE_PS3		1
#define SCE_NP_PLATFORM_TYPE_VITA		2
#define SCE_NP_PLATFORM_TYPE_PS4		3

/* Np Peer Address */
typedef struct SceNpPeerAddress {
	SceNpOnlineId onlineId;
	SceNpPlatformType platform;
} SceNpPeerAddress;

/* NP title ID */
#define SCE_NP_TITLE_ID_LEN		12
typedef struct SceNpTitleId {
	char id[SCE_NP_TITLE_ID_LEN + 1];
	SceUChar8 padding[3];
} SceNpTitleId;

/* NP title secret */
#define SCE_NP_TITLE_SECRET_SIZE		128
typedef struct SceNpTitleSecret {
	SceUChar8 data[SCE_NP_TITLE_SECRET_SIZE];
} SceNpTitleSecret;

/* NP Service Label */
typedef SceUInt32 SceNpServiceLabel;
#define SCE_NP_DEFAULT_SERVICE_LABEL (0x00000000)
#define SCE_NP_INVALID_SERVICE_LABEL (0xFFFFFFFF)

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
