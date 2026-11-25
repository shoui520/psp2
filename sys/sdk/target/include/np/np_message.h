/*
 * np/np_message.h: NP messages, under Sony's names and with Sony's definitions (SDK Release
 * 03.570.011, target/include/np/np_message.h), as user code needs them.
 */
#ifndef _SCE_NP_MESSAGE_H_
#define _SCE_NP_MESSAGE_H_

#include <scetypes.h>
#include <np/np_common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef SceInt32 SceNpMessageType;
#define SCE_NP_MESSAGE_TYPE_INVALID			(0)
#define SCE_NP_MESSAGE_TYPE_APP_DATA_ATTACHMENT		(1)
#define SCE_NP_MESSAGE_TYPE_INVITATION_ATTACHMENT	(2)
#define SCE_NP_MESSAGE_TYPE_STORE_LINK_ATTACHMENT	(3)

#define SCE_NP_MESSAGE_MAX_MESSAGE_ID_SIZE		(70)	/* bytes of a message id */
#define SCE_NP_MESSAGE_MAX_PREVIEW_SIZE			(128)
#define SCE_NP_MESSAGE_MAX_RECIPIENTS			(16)
#define SCE_NP_MESSAGE_MAX_BODY_SIZE			(512)
#define SCE_NP_MESSAGE_MAX_ATTACHMENT_SIZE		(1024*1024)
#define SCE_NP_MESSAGE_MAX_INDEX_ICON_SIZE		(64*1024)

typedef struct SceNpMessageId {
	SceChar8 uid[SCE_NP_MESSAGE_MAX_MESSAGE_ID_SIZE];
	SceChar8 term;
	SceChar8 padding[1];
} SceNpMessageId;

typedef struct SceNpMessageEntry {
	SceNpMessageId messageId;
	SceDateTime dateTime;
	SceNpId from;
	SceChar8 preview[SCE_NP_MESSAGE_MAX_PREVIEW_SIZE];
	SceDateTime expirationDate;
	SceUInt16 messageFlag;
	SceChar8 reserved[30];
} SceNpMessageEntry;

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_MESSAGE_H_ */
