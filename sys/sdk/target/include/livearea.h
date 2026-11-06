/*
 * livearea.h: the application's LiveArea(TM) content, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/livearea.h). SceLiveAreaUtil exports the
 * functions.
 */
#ifndef _SCE_LIVEAREA_H
#define _SCE_LIVEAREA_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_LIVEAREA_OK				(0)
#define SCE_LIVEAREA_BUSY			(1)
#define SCE_LIVEAREA_ERROR_STATE		(-2146418687)	/* 0x80104001 */
#define SCE_LIVEAREA_ERROR_PARAM		(-2146418686)	/* 0x80104002 */
#define SCE_LIVEAREA_ERROR_XML_NOT_FOUND	(-2146418685)	/* 0x80104003 */
#define SCE_LIVEAREA_ERROR_XML_FORMAT		(-2146418684)	/* 0x80104004 */
#define SCE_LIVEAREA_ERROR_FILE_NOT_FOUND	(-2146418683)	/* 0x80104005 */
#define SCE_LIVEAREA_ERROR_FILE_NUM		(-2146418682)	/* 0x80104006 */
#define SCE_LIVEAREA_ERROR_FILE_TOTAL_SIZE	(-2146418681)	/* 0x80104007 */
#define SCE_LIVEAREA_ERROR_FILE_FORMAT		(-2146418680)	/* 0x80104008 */
#define SCE_LIVEAREA_ERROR_XML_SIZE		(-2146418679)	/* 0x80104009 */
#define SCE_LIVEAREA_ERROR_FRAME_NOT_FOUND	(-2146418678)	/* 0x8010400a */
#define SCE_LIVEAREA_ERROR_DIRPATH		(-2146418677)	/* 0x8010400b */
#define SCE_LIVEAREA_ERROR_INTERNAL		(-2146418433)	/* 0x801040ff */

#define SCE_LIVEAREA_MAX_FS_PATH_LEN		(255)
#define SCE_LIVEAREA_MAX_FRAME_STR_LEN		(10239)
#define SCE_LIVEAREA_MAX_USER_DATA_LEN		(1023)

#define SCE_LIVEAREA_FLAG_NONE			(0)
#define SCE_LIVEAREA_FLAG_TEMP			(1)

#define SCE_LIVEAREA_FORMAT_VER_CURRENT		"01.00"

SceInt32 sceLiveAreaReplaceAllSync(const char *dirpath);
SceInt32 sceLiveAreaReplaceAllAsync(const char *dirpath);
SceInt32 sceLiveAreaUpdateFrameSync(const char *formatVer, const char *frameXmlStr,
				    SceInt32 frameXmlLen, const char *dirpathTop, SceUInt32 flag);
SceInt32 sceLiveAreaUpdateFrameAsync(const char *formatVer, const char *frameXmlStr,
				     SceInt32 frameXmlLen, const char *dirpathTop, SceUInt32 flag);
SceInt32 sceLiveAreaGetStatus();
SceInt32 sceLiveAreaGetRevision(SceInt64 *revision);
SceInt32 sceLiveAreaGetFrameRevision(const char *frameId, SceInt64 *revision);
SceInt32 sceLiveAreaGetFrameUserData(const char *frameId, char *userData, SceUInt32 userDataSize);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_LIVEAREA_H */
