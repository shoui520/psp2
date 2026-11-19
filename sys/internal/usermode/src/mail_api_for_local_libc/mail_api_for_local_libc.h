/*
 * SceMailApiForLocalLibc: the message the local mail API stores, and its error codes.
 */
#ifndef _MAIL_API_FOR_LOCAL_LIBC_H
#define _MAIL_API_FOR_LOCAL_LIBC_H

#include <scetypes.h>
#include <rtc.h>

#define SCE_MAIL_API_FOR_LOCAL_ERROR_INVALID_ARGUMENT	(0x80107F01)
#define SCE_MAIL_API_FOR_LOCAL_ERROR_NO_MEMORY		(0x80107F02)
#define SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_MANY_MESSAGES	(0x80107F03)
#define SCE_MAIL_API_FOR_LOCAL_ERROR_NOT_MOUNTED	(0x80107F04)
#define SCE_MAIL_API_FOR_LOCAL_ERROR_IO_READ		(0x80107F05)
#define SCE_MAIL_API_FOR_LOCAL_ERROR_IO_WRITE		(0x80107F06)
#define SCE_MAIL_API_FOR_LOCAL_ERROR_TOO_LARGE		(0x80107F07)

#ifdef __cplusplus
extern "C" {
#endif

/* an attachment: a file to copy, or data to store */
typedef struct MailLocalAttachment {
	SceBool isFile;
	const void *pData;	/* the file's path when isFile */
	SceSize size;
} MailLocalAttachment;

typedef struct MailLocalMessage {
	const char *pTitle;
	const char *pText;
	const SceRtcTick *pTime;
	const char *pAddress;
	const MailLocalAttachment *pBody;
	const MailLocalAttachment *pImage;
	const SceUInt32 *pFlags;
	const MailLocalAttachment *pThumbnail;
	const MailLocalAttachment *pIcon;
} MailLocalMessage;

SceInt32 mail_api_for_local_libc_2A03C7B4(const MailLocalMessage *pMessage);

#ifdef __cplusplus
}
#endif

#endif /* _MAIL_API_FOR_LOCAL_LIBC_H */
