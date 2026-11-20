/*
 * apputil/apputil_psp.h: the application utility's PSP save data calls, under Sony's names and
 * with Sony's definitions (SDK Release 03.570.011, target/include/apputil/apputil_psp.h).
 */
#ifndef _SCE_APPUTIL_PSP_H
#define _SCE_APPUTIL_PSP_H

#include <apputil.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_APPUTIL_ERROR_PSP_SAVEDATA_DATA_BROKEN		(-2146433424)	/* 0x80100670 */
#define SCE_APPUTIL_ERROR_PSP_SAVEDATA_FILETYPE_MISMATCH	(-2146433423)	/* 0x80100671 */
#define SCE_APPUTIL_ERROR_PSP_SAVEDATA_DATABUF_SIZE		(-2146433422)	/* 0x80100672 */

#define SCE_APPUTIL_PSP_SAVEDATA_FILENAME_SIZE		(13)
#define SCE_APPUTIL_PSP_SAVEDATA_SECUREFILEID_SIZE	(16)
#define SCE_APPUTIL_PSP_SAVEDATA_DATA_BUF_SIZE_MAX	(2*1024*1024)
#define SCE_APPUTIL_PSP_SAVEDATA_DIRNAME_LIST_MAXNUM	(256)

typedef enum {
	SCE_APPUTIL_PSP_SAVEDATA_VERSION_0 = 0,
	SCE_APPUTIL_PSP_SAVEDATA_VERSION_1,
	SCE_APPUTIL_PSP_SAVEDATA_VERSION_2
} SceAppUtilPspSaveDataVersion;

typedef enum {
	SCE_APPUTIL_PSP_SAVEDATA_TYPE_SECUREFILE = 0,
	SCE_APPUTIL_PSP_SAVEDATA_TYPE_NORMALFILE,
} SceAppUtilPspSaveDataType;

typedef enum {
	SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE	= 32,
	SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_TITLE_SIZE		= 128,
	SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_SD_TITLE_SIZE		= 128,
	SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DETAIL_SIZE		= 1024
} SceAppUtilPspSaveDataParamSfoSize;

typedef struct SceAppUtilPspSaveDataParamSfo {
	SceChar8 saveDataDirectory[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE];
	SceChar8 title[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_TITLE_SIZE];
	SceChar8 saveDataTitle[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_SD_TITLE_SIZE];
	SceChar8 detail[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DETAIL_SIZE];
	SceUChar8 parentalLev;
	SceChar8 reserved[3];
} SceAppUtilPspSaveDataParamSfo;

typedef struct SceAppUtilPspSaveDataParams {
	SceChar8 dirName[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE];
	SceChar8 fileName[SCE_APPUTIL_PSP_SAVEDATA_FILENAME_SIZE];
	SceChar8 reserved[3];
	SceAppUtilPspSaveDataType fileType;
	SceAppUtilPspSaveDataVersion dataVersion;
	const SceUChar8 *secureFileId;
	SceAppUtilPspSaveDataParamSfo *paramSfo;
	void *dataBuf;
	SceSize dataBufSize;
	SceChar8 reserved2[4];
} SceAppUtilPspSaveDataParams;

typedef struct SceAppUtilPspSaveDataDirName {
	SceChar8 data[SCE_APPUTIL_PSP_SAVEDATA_PARAMSFO_DIRECTORY_SIZE];
} SceAppUtilPspSaveDataDirName;

SceInt32 sceAppUtilPspSaveDataGetDirNameList(const SceAppUtilPspSaveDataDirName *prefix,
					     SceAppUtilPspSaveDataDirName *dirNameList,
					     SceUInt32 dirNameListNum, SceUInt32 *hitNum);
SceInt32 sceAppUtilPspSaveDataLoad(SceAppUtilPspSaveDataParams *loadParam);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_APPUTIL_PSP_H */
