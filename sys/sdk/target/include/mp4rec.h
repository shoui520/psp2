/*
 * Sony PlayStation Vita MP4 Video Recorder public SDK header.
 */
#ifndef _SCE_MP4REC_H
#define _SCE_MP4REC_H

#include <scetypes.h>

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Defines */
#define SCE_MP4REC_PATH_MAX                     (512)
#define SCE_MP4REC_VIDEO_TIMESCALE              (30000)
#define SCE_MP4REC_AUDIO_TIMESCALE              (48000)
#define SCE_MP4REC_VIDEO_SAMPLE_DURATION        (1001)
#define SCE_MP4REC_AUDIO_SAMPLE_DURATION        (1024)
#define SCE_MP4REC_MIN_HEAP_SIZE                (1024 * 1024 * 4)
#define SCE_MP4REC_DURATION_MIN                 (1000 * 5)
#define SCE_MP4REC_DURATION_MAX                 (1000 * 60 * 30)

#define SCE_MP4REC_PIXEL_RGBA8888               (0x0U)
#define SCE_MP4REC_PIXEL_BGRA8888               (0x8U)
#define SCE_MP4REC_PIXEL_YUV420_RASTER          (0x10U)
#define SCE_MP4REC_PIXEL_YUV420_PACKED_RASTER   (0x20U)

#define SCE_MP4REC_CSC_COEFFICIENT_DEFAULT      (0)
#define SCE_MP4REC_CSC_COEFFICIENT_ITU601       (1)
#define SCE_MP4REC_CSC_COEFFICIENT_ITU709       (2)

#define SCE_MP4REC_OPTION_ENABLE                (1 << 31)

#define SCE_MP4REC_THUMBNAIL_NUM_MAX            (2)
#define SCE_MP4REC_THUMBNAIL_WIDTH_REQUIRED     (160)
#define SCE_MP4REC_THUMBNAIL_HEIGHT_REQUIRED    (120)
#define SCE_MP4REC_THUMBNAIL_WIDTH_MAX          (432)
#define SCE_MP4REC_THUMBNAIL_HEIGHT_MAX         (240)

#define SCE_MP4REC_CHAPTER_NUM_MAX              (999)

#define SCE_MP4REC_TITLE_SIZE_MAX               (256)
#define SCE_MP4REC_DESCRIPTION_SIZE_MAX         (2000)
#define SCE_MP4REC_COPYRIGHT_NOTICE_SIZE_MAX    (2000)

/* Basic errors */
#define SCE_MP4REC_ERROR_INVALID_ARG                        (0x80108301)
#define SCE_MP4REC_ERROR_OUT_OF_MEMORY                      (0x80108302)
#define SCE_MP4REC_ERROR_INVALID_RECMODE                    (0x80108303)
#define SCE_MP4REC_ERROR_INVALID_BUFSIZE                    (0x80108304)
#define SCE_MP4REC_ERROR_NOT_INITIALIZED                    (0x80108305)
#define SCE_MP4REC_ERROR_DURATION_LIMIT                     (0x80108306)
#define SCE_MP4REC_ERROR_INTERNAL                           (0x80108307)
#define SCE_MP4REC_ERROR_NOSPACE                            (0x80108308)
#define SCE_MP4REC_ERROR_STATE                              (0x80108309)

/* Video encode errors */
#define SCE_MP4REC_ERROR_VIDEOENC_INVALID_PARAM             (0x80108342)
#define SCE_MP4REC_ERROR_VIDEOENC_OUT_OF_MEMORY             (0x80108343)
#define SCE_MP4REC_ERROR_VIDEOENC_INVALID_STATE             (0x80108344)
#define SCE_MP4REC_ERROR_VIDEOENC_UNSUPPORT_IMAGE_SIZE      (0x80108345)
#define SCE_MP4REC_ERROR_VIDEOENC_INVALID_COLOR_FORMAT      (0x80108346)
#define SCE_MP4REC_ERROR_VIDEOENC_NOT_PHY_CONTINUOUS_MEMORY (0x80108347)
#define SCE_MP4REC_ERROR_VIDEOENC_ALREADY_USED              (0x80108348)
#define SCE_MP4REC_ERROR_VIDEOENC_INVALID_POINTER           (0x80108349)
#define SCE_MP4REC_ERROR_VIDEOENC_INITIALIZE                (0x8010834B)
#define SCE_MP4REC_ERROR_VIDEOENC_NOT_INITIALIZE            (0x8010834C)
#define SCE_MP4REC_ERROR_VIDEOENC_INVALID_ARGUMENT_SIZE     (0x8010834E)

/* AVC codec errors */
#define SCE_MP4REC_ERROR_AVCENC_INVALID_PARAM               (0x80108352)
#define SCE_MP4REC_ERROR_AVCENC_OUT_OF_MEMORY               (0x80108353)
#define SCE_MP4REC_ERROR_AVCENC_INVALID_STATE               (0x80108354)
#define SCE_MP4REC_ERROR_AVCENC_UNSUPPORT_IMAGE_SIZE        (0x80108355)
#define SCE_MP4REC_ERROR_AVCENC_INVALID_COLOR_FORMAT        (0x80108356)
#define SCE_MP4REC_ERROR_AVCENC_NOT_PHY_CONTINUOUS_MEMORY   (0x80108357)
#define SCE_MP4REC_ERROR_AVCENC_ALREADY_USED                (0x80108358)
#define SCE_MP4REC_ERROR_AVCENC_INVALID_POINTER             (0x80108359)
#define SCE_MP4REC_ERROR_AVCENC_PICTURE_BUFFER_FULL         (0x8010835A)
#define SCE_MP4REC_ERROR_AVCENC_INITIALIZE                  (0x8010835B)
#define SCE_MP4REC_ERROR_AVCENC_NOT_INITIALIZE              (0x8010835C)
#define SCE_MP4REC_ERROR_AVCENC_INVALID_ENCODE_PARAM        (0x8010835D)
#define SCE_MP4REC_ERROR_AVCENC_INVALID_ARGUMENT_SIZE       (0x8010835E)
#define SCE_MP4REC_ERROR_AVCENC_MAXAUSIZE                   (0x80108361)

/* AAC codec errors */
#define SCE_MP4REC_ERROR_AACENC_INVALIDARG                  (0x80108381)
#define SCE_MP4REC_ERROR_AACENC_INVALIDHANDLE               (0x80108382)
#define SCE_MP4REC_ERROR_AACENC_OUTOFMEMORY                 (0x80108383)
#define SCE_MP4REC_ERROR_AACENC_INTERNAL                    (0x80108384)
#define SCE_MP4REC_ERROR_AACENC_FRAME_LIMIT                 (0x80108385)
#define SCE_MP4REC_ERROR_AACENC_NOTIMPL                     (0x80108386)

/* AV recorder errors */
#define SCE_MP4REC_ERROR_AVREC_OUT_OF_MEMORY                (0x801083A1)
#define SCE_MP4REC_ERROR_AVREC_FATAL                        (0x801083A2)
#define SCE_MP4REC_ERROR_AVREC_INVALID_VALUE                (0x801083A3)
#define SCE_MP4REC_ERROR_AVREC_VENC_BUFFER_FULL             (0x801083A4)
#define SCE_MP4REC_ERROR_AVREC_FILE_ACCESS                  (0x801083A5)
#define SCE_MP4REC_ERROR_AVREC_FILE_OPERATION               (0x801083A6)
#define SCE_MP4REC_ERROR_AVREC_NOT_IMPLEMENT                (0x801083A7)
#define SCE_MP4REC_ERROR_AVREC_BUFFER_FULL                  (0x801083A8)
#define SCE_MP4REC_ERROR_AVREC_SAMPLE_NUM_BUFFER_FULL       (0x801083A9)
#define SCE_MP4REC_ERROR_AVREC_VIDEO_BUFFER_FULL            (0x801083AA)
#define SCE_MP4REC_ERROR_AVREC_VIDEO_SAMPLE_NUM_BUFFER_FULL (0x801083AB)
#define SCE_MP4REC_ERROR_AVREC_AUDIO_BUFFER_FULL            (0x801083AC)
#define SCE_MP4REC_ERROR_AVREC_AUDIO_SAMPLE_NUM_BUFFER_FULL (0x801083AD)
#define SCE_MP4REC_ERROR_AVREC_VENC_RET_INVALID_POINTER     (0x801083AE)

#define SCE_MP4REC_ERROR_AVREC_NAACENC_INVALIDARG           (0x801083B1)
#define SCE_MP4REC_ERROR_AVREC_NAACENC_INVALIDHANDLE        (0x801083B2)
#define SCE_MP4REC_ERROR_AVREC_NAACENC_OUTOFMEMORY          (0x801083B3)
#define SCE_MP4REC_ERROR_AVREC_NAACENC_INTERNAL             (0x801083B4)
#define SCE_MP4REC_ERROR_AVREC_NAACENC_FRAME_LIMIT          (0x801083B5)
#define SCE_MP4REC_ERROR_AVREC_NAACENC_NOTIMPL              (0x801083B6)
#define SCE_MP4REC_ERROR_AVREC_NAACENC_FATAL                (0x801083B7)

#define SCE_MP4REC_ERROR_AVREC_FF4_ARGS                     (0x801083C1)
#define SCE_MP4REC_ERROR_AVREC_FF4_STATE                    (0x801083C2)
#define SCE_MP4REC_ERROR_AVREC_FF4_NOMEM                    (0x801083C3)
#define SCE_MP4REC_ERROR_AVREC_FF4_FILE                     (0x801083C4)
#define SCE_MP4REC_ERROR_AVREC_FF4_NOTFOUND                 (0x801083C5)
#define SCE_MP4REC_ERROR_AVREC_FF4_OVERFLOW                 (0x801083C6)
#define SCE_MP4REC_ERROR_AVREC_FF4_FORMAT                   (0x801083C7)
#define SCE_MP4REC_ERROR_AVREC_FF4_INTERNAL                 (0x801083C8)
#define SCE_MP4REC_ERROR_AVREC_FF4_FATAL                    (0x801083C9)

/* Video export errors */
#define SCE_MP4REC_ERROR_EXPORT_PARAM                       (0x801083E1)
#define SCE_MP4REC_ERROR_EXPORT_FILE_NOT_FOUND              (0x801083E2)
#define SCE_MP4REC_ERROR_EXPORT_CONTENT_FULL                (0x801083E3)
#define SCE_MP4REC_ERROR_EXPORT_NO_MEMORY                   (0x801083E4)
#define SCE_MP4REC_ERROR_EXPORT_SERVER_DOWN                 (0x801083E5)
#define SCE_MP4REC_ERROR_EXPORT_TOO_MANY_CLIENT             (0x801083E6)
#define SCE_MP4REC_ERROR_EXPORT_MEDIA_FULL                  (0x801083E7)
#define SCE_MP4REC_ERROR_EXPORT_CREATE_FILE                 (0x801083E8)
#define SCE_MP4REC_ERROR_EXPORT_NOT_SUPPORTED_FORMAT        (0x801083E9)
#define SCE_MP4REC_ERROR_EXPORT_CANCELED                    (0x801083EA)
#define SCE_MP4REC_ERROR_EXPORT_MEDIA_NOT_EXIST             (0x801083EB)
#define SCE_MP4REC_ERROR_EXPORT_DB_CORRUPTED                (0x801083EC)
#define SCE_MP4REC_ERROR_EXPORT_FILE_EXIST                  (0x801083ED)
#define SCE_MP4REC_ERROR_EXPORT_INTERNAL                    (0x801083EE)
#define SCE_MP4REC_ERROR_EXPORT_NOT_INITIALIZED             (0x801083EF)
#define SCE_MP4REC_ERROR_EXPORT_OPEN_FAILED                 (0x801083F0)
#define SCE_MP4REC_ERROR_EXPORT_NOT_ENOUGH_AREA             (0x801083F1)
#define SCE_MP4REC_ERROR_EXPORT_NOT_VALID_STR               (0x801083F2)
#define SCE_MP4REC_ERROR_EXPORT_NOT_VALID_FILENAME          (0x801083F3)

/* Recording modes */
#define SCE_MP4REC_MODE_640X368_2MBPS                       (0x00000000)
#define SCE_MP4REC_MODE_640X368_1MBPS                       (0x00000001)
#define SCE_MP4REC_MODE_480X272_2MBPS                       (0x00000002)
#define SCE_MP4REC_MODE_480X272_1MBPS                       (0x00000003)
#define SCE_MP4REC_MODE_368X208_2MBPS                       (0x00000004)
#define SCE_MP4REC_MODE_368X208_1MBPS                       (0x00000005)

/* Recording mode options */
#define SCE_MP4REC_MODE_OPTION_ITU601                       (0x00010000)
#define SCE_MP4REC_MODE_OPTION_ITU709                       (0x00020000)

/* Metadata Language Types */
#define SCE_MP4REC_LANG_JAPANESE                            (0x2a0e)
#define SCE_MP4REC_LANG_ENGLISH                             (0x15c7)
#define SCE_MP4REC_LANG_FRENCH                              (0x1a41)
#define SCE_MP4REC_LANG_SPANISH                             (0x4e01)
#define SCE_MP4REC_LANG_GERMAN                              (0x10b5)
#define SCE_MP4REC_LANG_ITALIAN                             (0x2681)
#define SCE_MP4REC_LANG_DUTCH                               (0x3984)
#define SCE_MP4REC_LANG_PORTUGUESE                          (0x41f2)
#define SCE_MP4REC_LANG_RUSSIAN                             (0x4ab3)
#define SCE_MP4REC_LANG_KOREAN                              (0x2df2)
#define SCE_MP4REC_LANG_CHINESE                             (0x690f)
#define SCE_MP4REC_LANG_FINNISH                             (0x192e)
#define SCE_MP4REC_LANG_SWEDISH                             (0x4ee5)
#define SCE_MP4REC_LANG_DANISH                              (0x102e)
#define SCE_MP4REC_LANG_NORWEGIAN                           (0x39f2)
#define SCE_MP4REC_LANG_POLISH                              (0x41ec)
#define SCE_MP4REC_LANG_TURKISH                             (0x52b2)

typedef struct SceMp4RecCtrl {
	SceUInt32 size;
	void *pHeapBase;
	SceUInt32 heapSize;
	SceInt32 handle;
} SceMp4RecCtrl;

typedef struct SceMp4RecInitParam {
	SceUInt32 size;
	SceUInt32 recMode;
	void *pEncoderPhyContPtr;
	SceUInt32 encoderPhyContSize;
	void *pAvRecPhyContPtr;
	SceUInt32 avRecPhyContSize;
	SceInt32 affinityForWriteThread;
	SceInt32 priorityForWriteThread;
} SceMp4RecInitParam;

typedef struct SceMp4RecThumbnailInfo {
	SceUInt32 pngSize;
	void *pPng;
} SceMp4RecThumbnailInfo;

typedef struct SceMp4RecMetadata {
	SceUInt32 size;
	SceUInt32 lang;
	SceUInt32 titleSize;
	void *pTitle;
	SceUInt32 descriptionSize;
	void *pDescription;
	SceUInt32 copyrightNoticeSize;
	void *pCopyrightNotice;
	SceUInt32 parentalLevel;
	SceUInt32 numOfChapter;
	SceUInt32 *pTopOfChapterTbl;
	SceUInt32 numOfThumbnail;
	SceMp4RecThumbnailInfo thumbnailInfo[SCE_MP4REC_THUMBNAIL_NUM_MAX];
} SceMp4RecMetadata;

typedef struct SceMp4RecTermParam {
	SceUInt32 size;
	SceBool isCancel;
	char reserved[512];
	SceMp4RecMetadata *pMetadata;
} SceMp4RecTermParam;

typedef struct SceMp4RecFrameOptionRGBA {
	SceUInt8 alpha;
	SceUInt8 cscCoefficient;
	SceUInt8 reserved[14];
} SceMp4RecFrameOptionRGBA;

typedef union SceMp4RecFrameOption {
	SceUInt8 reserved[16];
	SceMp4RecFrameOptionRGBA rgba;
} SceMp4RecFrameOption;

typedef struct SceMp4RecFrame {
	SceUInt32 pixelType;
	SceUInt32 framePitch;
	SceUInt32 frameWidth;
	SceUInt32 frameHeight;
	SceMp4RecFrameOption opt;
	void *pPicture[2];
} SceMp4RecFrame;

typedef struct SceMp4RecPicture {
	SceSize size;
	SceMp4RecFrame frame;
} SceMp4RecPicture;

SceInt32 sceMp4RecRecorderInit(void);
SceInt32 sceMp4RecRecorderEnd(void);
SceInt32 sceMp4RecCreateRecorder(SceMp4RecCtrl *pCtrl);
SceInt32 sceMp4RecDeleteRecorder(SceMp4RecCtrl *pCtrl);
SceInt32 sceMp4RecQueryPhysicalMemSize(SceMp4RecCtrl *pCtrl, SceUInt32 recMode,
	SceUInt32 *pEncodeWorkMemSize, SceUInt32 *pAvRecMemSize);
SceInt32 sceMp4RecInit(SceMp4RecCtrl *pCtrl, SceMp4RecInitParam *pParam);
SceInt32 sceMp4RecAddVideoSample(SceMp4RecCtrl *pCtrl, void *pBuffer, SceInt32 bufsize);
SceInt32 sceMp4RecAddAudioSample(SceMp4RecCtrl *pCtrl, void *pBuffer, SceInt32 bufsize);
SceInt32 sceMp4RecTerm(SceMp4RecCtrl *pCtrl, SceMp4RecTermParam *pParam);
SceInt32 sceMp4RecCsc(SceMp4RecPicture *pDst, const SceMp4RecPicture *pSrc);

#if defined(_LANGUAGE_C_PLUS_PLUS) || defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* _SCE_MP4REC_H */
