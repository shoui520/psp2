/*
 * libhandwriting/libhandwriting_api.h: SceHandwriting public types and API,
 * mirroring Sony's SDK layout (target/include/libhandwriting/libhandwriting_api.h).
 */
#ifndef _SCE_LIBHANDWRITING_API_H
#define _SCE_LIBHANDWRITING_API_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SceHandwritingDic {
	SCE_HANDWRITING_DIC_ALNUM,
	SCE_HANDWRITING_DIC_ALNUM_NATIVE,
	SCE_HANDWRITING_DIC_HIRA,
	SCE_HANDWRITING_DIC_KATA,
	SCE_HANDWRITING_DIC_KANJI,
	SCE_HANDWRITING_DIC_KANJI2,
	SCE_HANDWRITING_DIC_ALL,
	SCE_HANDWRITING_DIC_ALL2,
	SCE_HANDWRITING_DIC_ALNUMLAT1,
	SCE_HANDWRITING_DIC_KOREAN,
	SCE_HANDWRITING_DIC_HANGUL,
	SCE_HANDWRITING_DIC_GERMAN,
	SCE_HANDWRITING_DIC_DUTCH,
	SCE_HANDWRITING_DIC_FRENCH,
	SCE_HANDWRITING_DIC_ITALIAN,
	SCE_HANDWRITING_DIC_SPANISH,
	SCE_HANDWRITING_DIC_PORTUGUESE,
	SCE_HANDWRITING_DIC_RUSSIAN,
	SCE_HANDWRITING_DIC_DAN_NOR,
	SCE_HANDWRITING_DIC_SWE_FIN,
	SCE_HANDWRITING_DIC_ICELANDIC,
	SCE_HANDWRITING_DIC_POLISH,
	SCE_HANDWRITING_DIC_SIMPLIFIED_CHINESE,
	SCE_HANDWRITING_DIC_TRADITIONAL_CHINESE
} SceHandwritingDic;

#define SCE_HANDWRITING_MODE_SYMBOL		0x0001
#define SCE_HANDWRITING_MODE_NUMBER		0x0002
#define SCE_HANDWRITING_MODE_UALPHA		0x0004
#define SCE_HANDWRITING_MODE_LALPHA		0x0008
#define SCE_HANDWRITING_MODE_HIRA		0x0010
#define SCE_HANDWRITING_MODE_KATA		0x0020
#define SCE_HANDWRITING_MODE_GREEK		0x0040
#define SCE_HANDWRITING_MODE_HANGUL		0x0080
#define SCE_HANDWRITING_MODE_HANJA		0x0100
#define SCE_HANDWRITING_MODE_KANJI1		0x0400
#define SCE_HANDWRITING_MODE_KANJI2		0x0800
#define SCE_HANDWRITING_MODE_URUSSIAN		0x0080
#define SCE_HANDWRITING_MODE_LRUSSIAN		0x0100
#define SCE_HANDWRITING_MODE_HONGKONG		0x0080
#define SCE_HANDWRITING_MODE_ALL		0x1FFF

#pragma pack(push, 2)
typedef struct SceHandwritingPoint {
	SceUChar8 x;
	SceUChar8 y;
} SceHandwritingPoint;
#pragma pack(pop)

#define SCE_HANDWRITING_MAX_CANDIDATES		(32)
#define SCE_HANDWRITING_MAX_STROKE		(31)
#define SCE_HANDWRITING_MAX_POINTS		(1024)
#define SCE_HANDWRITING_MAX_REGISTER_COUNT	(32767)
#define SCE_HANDWRITING_REGISTER_BLKSIZE	(512)
#define SCE_HANDWRITING_MAX_REGISTER_CANDIDATES	(32)

typedef struct SceHandwritingData {
	SceWChar32 codes[SCE_HANDWRITING_MAX_CANDIDATES];
	SceHandwritingPoint points[SCE_HANDWRITING_MAX_POINTS];
} SceHandwritingData;

typedef struct SceHandwritingPointsData {
	SceHandwritingPoint points[SCE_HANDWRITING_MAX_POINTS];
} SceHandwritingPointsData;

typedef struct SceHandwritingRegisterData {
	SceWChar32 codes[SCE_HANDWRITING_MAX_REGISTER_CANDIDATES];
} SceHandwritingRegisterData;

SceInt32 sceHandwritingGetBufferSize(SceUInt32 dic);
SceInt32 sceHandwritingInit(SceUChar8 maxX, SceUChar8 maxY,
			    SceUInt32 dic, ScePVoid dicBuffer);
SceInt32 sceHandwritingTerm(void);
SceInt32 sceHandwritingRecognize(SceHandwritingData *data);
SceInt32 sceHandwritingSetMode(SceUInt32 mode);

SceInt32 sceHandwritingRegisterInit(SceUInt32 count, ScePVoid regBuffer);
SceInt32 sceHandwritingRegisterTerm(void);
SceInt32 sceHandwritingRegisterSet(SceWChar32 code, const SceHandwritingPointsData *data);
SceInt32 sceHandwritingRegisterGetResult(SceHandwritingRegisterData *data);
SceInt32 sceHandwritingRegisterDelete(SceWChar32 code);
SceInt32 sceHandwritingRegisterInfo(SceWChar32 *codes);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_LIBHANDWRITING_API_H */
