/*
 * libface.h: SceFace, the SDK's face detection, parts, attribute, age, identification and
 * shape library (target/sce_module/libface.suprx), declared as the SDK declares it.
 */

#ifndef _SCE_LIBFACE_H
#define _SCE_LIBFACE_H

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <sceerror.h>

#define SCE_FACE_ERROR_NO_MEMORY		(-2138374143) /* 0x808B0001 */
#define SCE_FACE_ERROR_INVALID_PARAM	(-2138374142) /* 0x808B0002 */
#define SCE_FACE_ERROR_INVALID_DICT		(-2138374141) /* 0x808B0003 */
#define SCE_FACE_ERROR_IMPERF_PARTS		(-2138374140) /* 0x808B0004 */
#define SCE_FACE_ERROR_OUT_OF_RANGE		(-2138374139) /* 0x808B0005 */
#define SCE_FACE_ERROR_IMPERF_SHAPE		(-2138374138) /* 0x808B0006 */

typedef void *SceFaceDetectionDictPtr;

#define SCE_FACE_DETECT_FRONTAL_DICT				"face_detect_frontal.fdt"
#define SCE_FACE_DETECT_ROLL_DICT					"face_detect_roll.fdt"
#define SCE_FACE_DETECT_YAW_DICT					"face_detect_yaw.fdt"
#define SCE_FACE_DETECT_PITCH_DICT					"face_detect_pitch.fdt"
#define SCE_FACE_DETECT_ROLL_YAW_DICT				"face_detect_roll_yaw.fdt"
#define SCE_FACE_DETECT_ROLL_YAW_PITCH_DICT			"face_detect_roll_yaw_pitch.fdt"
#define SCE_FACE_DETECT_FRONTAL_DICT_SIZE			(43488)
#define SCE_FACE_DETECT_ROLL_DICT_SIZE				(78648)
#define SCE_FACE_DETECT_YAW_DICT_SIZE				(134752)
#define SCE_FACE_DETECT_PITCH_DICT_SIZE				(124728)
#define SCE_FACE_DETECT_ROLL_YAW_DICT_SIZE			(186444)
#define SCE_FACE_DETECT_ROLL_YAW_PITCH_DICT_SIZE	(267508)

#define SCE_FACE_DETECT_RESULT_NORMAL	0
#define SCE_FACE_DETECT_RESULT_PRECISE	1
#define SCE_FACE_DETECT_RESULT_FAST		2

#define SCE_FACE_DETECT_SEARCH_ALL_FACE			0
#define SCE_FACE_DETECT_SEARCH_FACE_NUM_LIMIT	1

typedef struct SceFaceDetectionResult {
	float	faceX;
	float	faceY;
	float	faceW;
	float	faceH;
	float	faceRoll;
	float	facePitch;
	float	faceYaw;
	float	score;
} SceFaceDetectionResult;

typedef struct SceFaceDetectionParam {
	int     version;
	int     size;
	float	magBegin;
	float	magStep;
	float	magEnd;
	int		xScanStart;
	int		yScanStart;
	int		xScanStep;
	int		yScanStep;
	float	xScanOver;
	float	yScanOver;
	float	thresholdScore;
	int		resultPrecision;
	int		searchType;
} SceFaceDetectionParam;

int sceFaceDetection(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceDetectionDictPtr	detectDictPtr,
	float							magBegin,
	float							magStep,
	float							magEnd,
	int								xScanStep,
	int								yScanStep,
	float							thresholdScore,
	int								resultPrecision,
	SceFaceDetectionResult			resultFaceArray[],
	int								resultFaceArraySize,
	int								*resultFaceNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceDetectionGetDefaultParam(SceFaceDetectionParam *param);

int sceFaceDetectionEx(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceDetectionDictPtr	detectDictPtr,
	const SceFaceDetectionParam		*detectParam,
	SceFaceDetectionResult			resultFaceArray[],
	int								resultFaceArraySize,
	int								*resultFaceNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceDetectionLocal(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceDetectionDictPtr	detectDictPtr,
	float							magStep,
	float							xExpandRegion,
	float							yExpandRegion,
	int								xScanStep,
	int								yScanStep,
	float							thresholdScore,
	const SceFaceDetectionResult	referenceFaceArray[],
	int								referenceFaceArraySize,
	SceFaceDetectionResult			resultFaceArray[],
	int								resultFaceArraySize,
	int								*resultFaceNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceDetectionGetWorkingMemorySize(
	int						width,
	int						height,
	int						rowstride,
	SceFaceDetectionDictPtr	detectDictPtr
);

typedef void *SceFacePartsDictPtr;
typedef void *SceFaceShapeDictPtr;
typedef void *SceFacePartsCheckDictPtr;

#define SCE_FACE_PARTS_ROLL_DICT			"face_parts_roll.pdt"
#define SCE_FACE_PARTS_ROLL_YAW_DICT		"face_parts_roll_yaw.pdt"
#define SCE_FACE_ALLPARTS_DICT				"face_allparts.pdt"
#define SCE_FACE_ALLPARTS_SHAPE_DICT		"face_allparts_shape.dat"
#define SCE_FACE_PARTS_CHECK_DICT			"face_parts_check.cdt"

#define SCE_FACE_PARTS_ROLL_DICT_SIZE		(40644)
#define SCE_FACE_PARTS_ROLL_YAW_DICT_SIZE	(193748)
#define SCE_FACE_ALLPARTS_DICT_SIZE			(1609168)
#define SCE_FACE_ALLPARTS_SHAPE_DICT_SIZE	(55080)
#define SCE_FACE_PARTS_CHECK_DICT_SIZE		(1868)

#define SCE_FACE_PARTS_NUM_MAX			4
#define SCE_FACE_PARTS_ID_UNDEF			0
#define SCE_FACE_PARTS_ID_R_EYE_CENTER	1
#define SCE_FACE_PARTS_ID_L_EYE_CENTER	2
#define SCE_FACE_PARTS_ID_NOSE_CENTER	3
#define SCE_FACE_PARTS_ID_MOUTH_CENTER	4

#define SCE_FACE_PARTS_R_EYE_CENTER_INDEX	0
#define SCE_FACE_PARTS_L_EYE_CENTER_INDEX	1
#define SCE_FACE_PARTS_NOSE_CENTER_INDEX	2
#define SCE_FACE_PARTS_MOUTH_CENTER_INDEX	3

#define SCE_FACE_ALLPARTS_NUM_MAX		55
#define SCE_FACE_PARTS_ID_ALL_BASE		100
#define SCE_FACE_PARTS_FACE_ID_ALL_00		(SCE_FACE_PARTS_ID_ALL_BASE+0)
#define SCE_FACE_PARTS_FACE_ID_ALL_01		(SCE_FACE_PARTS_ID_ALL_BASE+1)
#define SCE_FACE_PARTS_FACE_ID_ALL_02		(SCE_FACE_PARTS_ID_ALL_BASE+2)
#define SCE_FACE_PARTS_FACE_ID_ALL_03		(SCE_FACE_PARTS_ID_ALL_BASE+3)
#define SCE_FACE_PARTS_FACE_ID_ALL_04		(SCE_FACE_PARTS_ID_ALL_BASE+4)
#define SCE_FACE_PARTS_FACE_ID_ALL_05		(SCE_FACE_PARTS_ID_ALL_BASE+5)
#define SCE_FACE_PARTS_FACE_ID_ALL_06		(SCE_FACE_PARTS_ID_ALL_BASE+6)
#define SCE_FACE_PARTS_FACE_ID_ALL_07		(SCE_FACE_PARTS_ID_ALL_BASE+7)
#define SCE_FACE_PARTS_FACE_ID_ALL_08		(SCE_FACE_PARTS_ID_ALL_BASE+8)
#define SCE_FACE_PARTS_FACE_ID_ALL_09		(SCE_FACE_PARTS_ID_ALL_BASE+9)
#define SCE_FACE_PARTS_FACE_ID_ALL_10		(SCE_FACE_PARTS_ID_ALL_BASE+10)
#define SCE_FACE_PARTS_FACE_ID_ALL_11		(SCE_FACE_PARTS_ID_ALL_BASE+11)
#define SCE_FACE_PARTS_R_EYEBROW_ID_ALL_12	(SCE_FACE_PARTS_ID_ALL_BASE+12)
#define SCE_FACE_PARTS_R_EYEBROW_ID_ALL_13	(SCE_FACE_PARTS_ID_ALL_BASE+13)
#define SCE_FACE_PARTS_R_EYEBROW_ID_ALL_14	(SCE_FACE_PARTS_ID_ALL_BASE+14)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_15		(SCE_FACE_PARTS_ID_ALL_BASE+15)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_16		(SCE_FACE_PARTS_ID_ALL_BASE+16)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_17		(SCE_FACE_PARTS_ID_ALL_BASE+17)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_18		(SCE_FACE_PARTS_ID_ALL_BASE+18)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_19		(SCE_FACE_PARTS_ID_ALL_BASE+19)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_20		(SCE_FACE_PARTS_ID_ALL_BASE+20)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_21		(SCE_FACE_PARTS_ID_ALL_BASE+21)
#define SCE_FACE_PARTS_R_EYE_ID_ALL_22		(SCE_FACE_PARTS_ID_ALL_BASE+22)
#define SCE_FACE_PARTS_R_PUPIL_ID_ALL_23	(SCE_FACE_PARTS_ID_ALL_BASE+23)
#define SCE_FACE_PARTS_L_EYEBROW_ID_ALL_24	(SCE_FACE_PARTS_ID_ALL_BASE+24)
#define SCE_FACE_PARTS_L_EYEBROW_ID_ALL_25	(SCE_FACE_PARTS_ID_ALL_BASE+25)
#define SCE_FACE_PARTS_L_EYEBROW_ID_ALL_26	(SCE_FACE_PARTS_ID_ALL_BASE+26)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_27		(SCE_FACE_PARTS_ID_ALL_BASE+27)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_28		(SCE_FACE_PARTS_ID_ALL_BASE+28)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_29		(SCE_FACE_PARTS_ID_ALL_BASE+29)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_30		(SCE_FACE_PARTS_ID_ALL_BASE+30)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_31		(SCE_FACE_PARTS_ID_ALL_BASE+31)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_32		(SCE_FACE_PARTS_ID_ALL_BASE+32)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_33		(SCE_FACE_PARTS_ID_ALL_BASE+33)
#define SCE_FACE_PARTS_L_EYE_ID_ALL_34		(SCE_FACE_PARTS_ID_ALL_BASE+34)
#define SCE_FACE_PARTS_L_PUPIL_ID_ALL_35	(SCE_FACE_PARTS_ID_ALL_BASE+35)
#define SCE_FACE_PARTS_NOSE_ID_ALL_36		(SCE_FACE_PARTS_ID_ALL_BASE+36)
#define SCE_FACE_PARTS_NOSE_ID_ALL_37		(SCE_FACE_PARTS_ID_ALL_BASE+37)

#define SCE_FACE_PARTS_NOSTRIL_ID_ALL_40	(SCE_FACE_PARTS_ID_ALL_BASE+40)
#define SCE_FACE_PARTS_NOSTRIL_ID_ALL_41	(SCE_FACE_PARTS_ID_ALL_BASE+41)
#define SCE_FACE_PARTS_NOSTRIL_ID_ALL_42	(SCE_FACE_PARTS_ID_ALL_BASE+42)

#define SCE_FACE_PARTS_R_LIP_ID_ALL_45		(SCE_FACE_PARTS_ID_ALL_BASE+45)
#define SCE_FACE_PARTS_U_LIP_ID_ALL_46		(SCE_FACE_PARTS_ID_ALL_BASE+46)
#define SCE_FACE_PARTS_U_LIP_ID_ALL_47		(SCE_FACE_PARTS_ID_ALL_BASE+47)
#define SCE_FACE_PARTS_U_LIP_ID_ALL_48		(SCE_FACE_PARTS_ID_ALL_BASE+48)
#define SCE_FACE_PARTS_L_LIP_ID_ALL_49		(SCE_FACE_PARTS_ID_ALL_BASE+49)
#define SCE_FACE_PARTS_D_LIP_ID_ALL_50		(SCE_FACE_PARTS_ID_ALL_BASE+50)
#define SCE_FACE_PARTS_D_LIP_ID_ALL_51		(SCE_FACE_PARTS_ID_ALL_BASE+51)
#define SCE_FACE_PARTS_D_LIP_ID_ALL_52		(SCE_FACE_PARTS_ID_ALL_BASE+52)
#define SCE_FACE_PARTS_UC_LIP_ID_ALL_53		(SCE_FACE_PARTS_ID_ALL_BASE+53)
#define SCE_FACE_PARTS_UC_LIP_ID_ALL_54		(SCE_FACE_PARTS_ID_ALL_BASE+54)
#define SCE_FACE_PARTS_UC_LIP_ID_ALL_55		(SCE_FACE_PARTS_ID_ALL_BASE+55)
#define SCE_FACE_PARTS_DC_LIP_ID_ALL_56		(SCE_FACE_PARTS_ID_ALL_BASE+56)
#define SCE_FACE_PARTS_DC_LIP_ID_ALL_57		(SCE_FACE_PARTS_ID_ALL_BASE+57)
#define SCE_FACE_PARTS_DC_LIP_ID_ALL_58		(SCE_FACE_PARTS_ID_ALL_BASE+58)

#define SCE_FACE_PARTS_FACE_ALL_00_INDEX		0
#define SCE_FACE_PARTS_FACE_ALL_01_INDEX		1
#define SCE_FACE_PARTS_FACE_ALL_02_INDEX		2
#define SCE_FACE_PARTS_FACE_ALL_03_INDEX		3
#define SCE_FACE_PARTS_FACE_ALL_04_INDEX		4
#define SCE_FACE_PARTS_FACE_ALL_05_INDEX		5
#define SCE_FACE_PARTS_FACE_ALL_06_INDEX		6
#define SCE_FACE_PARTS_FACE_ALL_07_INDEX		7
#define SCE_FACE_PARTS_FACE_ALL_08_INDEX		8
#define SCE_FACE_PARTS_FACE_ALL_09_INDEX		9
#define SCE_FACE_PARTS_FACE_ALL_10_INDEX		10
#define SCE_FACE_PARTS_FACE_ALL_11_INDEX		11
#define SCE_FACE_PARTS_R_EYEBROW_ALL_12_INDEX	12
#define SCE_FACE_PARTS_R_EYEBROW_ALL_13_INDEX	13
#define SCE_FACE_PARTS_R_EYEBROW_ALL_14_INDEX	14
#define SCE_FACE_PARTS_R_EYE_ALL_15_INDEX		15
#define SCE_FACE_PARTS_R_EYE_ALL_16_INDEX		16
#define SCE_FACE_PARTS_R_EYE_ALL_17_INDEX		17
#define SCE_FACE_PARTS_R_EYE_ALL_18_INDEX		18
#define SCE_FACE_PARTS_R_EYE_ALL_19_INDEX		19
#define SCE_FACE_PARTS_R_EYE_ALL_20_INDEX		20
#define SCE_FACE_PARTS_R_EYE_ALL_21_INDEX		21
#define SCE_FACE_PARTS_R_EYE_ALL_22_INDEX		22
#define SCE_FACE_PARTS_R_PUPIL_ALL_23_INDEX		23
#define SCE_FACE_PARTS_L_EYEBROW_ALL_24_INDEX	24
#define SCE_FACE_PARTS_L_EYEBROW_ALL_25_INDEX	25
#define SCE_FACE_PARTS_L_EYEBROW_ALL_26_INDEX	26
#define SCE_FACE_PARTS_L_EYE_ALL_27_INDEX		27
#define SCE_FACE_PARTS_L_EYE_ALL_28_INDEX		28
#define SCE_FACE_PARTS_L_EYE_ALL_29_INDEX		29
#define SCE_FACE_PARTS_L_EYE_ALL_30_INDEX		30
#define SCE_FACE_PARTS_L_EYE_ALL_31_INDEX		31
#define SCE_FACE_PARTS_L_EYE_ALL_32_INDEX		32
#define SCE_FACE_PARTS_L_EYE_ALL_33_INDEX		33
#define SCE_FACE_PARTS_L_EYE_ALL_34_INDEX		34
#define SCE_FACE_PARTS_L_PUPIL_ALL_35_INDEX		35
#define SCE_FACE_PARTS_NOSE_ALL_36_INDEX		36
#define SCE_FACE_PARTS_NOSE_ALL_37_INDEX		37

#define SCE_FACE_PARTS_NOSTRIL_ALL_40_INDEX		38
#define SCE_FACE_PARTS_NOSTRIL_ALL_41_INDEX		39
#define SCE_FACE_PARTS_NOSTRIL_ALL_42_INDEX		40

#define SCE_FACE_PARTS_R_LIP_ALL_45_INDEX		41
#define SCE_FACE_PARTS_U_LIP_ALL_46_INDEX		42
#define SCE_FACE_PARTS_U_LIP_ALL_47_INDEX		43
#define SCE_FACE_PARTS_U_LIP_ALL_48_INDEX		44
#define SCE_FACE_PARTS_L_LIP_ALL_49_INDEX		45
#define SCE_FACE_PARTS_D_LIP_ALL_50_INDEX		46
#define SCE_FACE_PARTS_D_LIP_ALL_51_INDEX		47
#define SCE_FACE_PARTS_D_LIP_ALL_52_INDEX		48
#define SCE_FACE_PARTS_UC_LIP_ALL_53_INDEX		49
#define SCE_FACE_PARTS_UC_LIP_ALL_54_INDEX		50
#define SCE_FACE_PARTS_UC_LIP_ALL_55_INDEX		51
#define SCE_FACE_PARTS_DC_LIP_ALL_56_INDEX		52
#define SCE_FACE_PARTS_DC_LIP_ALL_57_INDEX		53
#define SCE_FACE_PARTS_DC_LIP_ALL_58_INDEX		54

typedef struct SceFacePartsResult {
	unsigned int	partsId;
	float			partsX;
	float			partsY;
	float			score;
} SceFacePartsResult;

int sceFaceParts(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFacePartsDictPtr		partsDictPtr,
	int								xScanStep,
	int								yScanStep,
	const SceFaceDetectionResult	*detectedFace,
	SceFacePartsResult				resultPartsArray[],
	int								resultPartsArraySize,
	int								*resultPartsNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFacePartsEx(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFacePartsDictPtr		partsDictPtr,
	const SceFacePartsCheckDictPtr	partsCheckerDictPtr,
	int								xScanStep,
	int								yScanStep,
	const SceFaceDetectionResult	*detectedFace,
	SceFacePartsResult				resultPartsArray[],
	int								resultPartsArraySize,
	int								*resultPartsNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceAllParts(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFacePartsDictPtr		partsDictPtr,
	const SceFaceShapeDictPtr		shapeDictPtr,
	int								xScanStep,
	int								yScanStep,
	const SceFaceDetectionResult	*detectedFace,
	SceFacePartsResult				resultPartsArray[],
	int								resultPartsArraySize,
	int								*resultPartsNum,
	void							*workMemory,
	int								workMemorySize
);

typedef struct SceFacePose {
	float	faceRoll;
	float	facePitch;
	float	faceYaw;
} SceFacePose;

typedef struct SceFaceRegion {
	float	faceRegionX[4];
	float	faceRegionY[4];
} SceFaceRegion;

int sceFaceEstimatePoseRegion(
	int								width,
	int								height,
	const SceFaceDetectionResult	*detectedFace,
	const SceFacePartsResult		detectedPartsArray[],
	int								detectedPartsNum,
	SceFacePose						*resultFacePose,
	SceFaceRegion					*resultFaceRegion
);

int sceFacePartsResultCheck(
	const SceFaceDetectionResult		*detectedFace,
	const SceFacePartsResult			detectedPartsArray[],
	int									detectedPartsNum,
	const SceFacePartsCheckDictPtr		partsCheckerDictPtr,
	bool								*good
);

int sceFacePartsGetWorkingMemorySize(
	int								width,
	int								height,
	int								rowstride,
	const SceFacePartsDictPtr		partsDictPtr
);

int sceFaceAllPartsGetWorkingMemorySize(
	int								width,
	int								height,
	int								rowstride,
	const SceFacePartsDictPtr		partsDictPtr
);

typedef void *SceFaceAttribDictPtr;
typedef void *SceFaceAgeDictPtr;
#define SCE_FACE_ATTRIB_SMILE_DICT		"face_attrib_smile.adt"
#define SCE_FACE_ATTRIB_DICT			"face_attrib.adt"
#define SCE_FACE_AGE_DICT				"face_age.adt"

#define SCE_FACE_ATTRIB_SMILE_DICT_SIZE	(25064)
#define SCE_FACE_ATTRIB_DICT_SIZE		(124368)
#define SCE_FACE_AGE_DICT_SIZE			(921304)

#define SCE_FACE_ATTRIB_NUM_MAX		8
#define SCE_FACE_ATTRIB_ID_UNDEF	0
#define SCE_FACE_ATTRIB_ID_SMILE	1
#define SCE_FACE_ATTRIB_ID_REYEOPEN	2
#define SCE_FACE_ATTRIB_ID_LEYEOPEN	3
#define SCE_FACE_ATTRIB_ID_GENDER	4
#define SCE_FACE_ATTRIB_ID_ADULT	5
#define SCE_FACE_ATTRIB_ID_BABY		6
#define SCE_FACE_ATTRIB_ID_ELDER	7
#define SCE_FACE_ATTRIB_ID_GLASS	8

#define SCE_FACE_ATTRIB_SMILE_INDEX		0
#define SCE_FACE_ATTRIB_REYEOPEN_INDEX	1
#define SCE_FACE_ATTRIB_LEYEOPEN_INDEX	2
#define SCE_FACE_ATTRIB_GENDER_INDEX	3
#define SCE_FACE_ATTRIB_ADULT_INDEX		4
#define SCE_FACE_ATTRIB_BABY_INDEX		5
#define SCE_FACE_ATTRIB_ELDER_INDEX		6
#define SCE_FACE_ATTRIB_GLASS_INDEX		7

typedef struct SceFaceAttribResult {
	unsigned int	attribId;
	float			score;
} SceFaceAttribResult;

typedef struct SceFaceAgeRangeResult {
	unsigned char maxAge;
	unsigned char minAge;
} SceFaceAgeRangeResult;

#define SCE_FACE_AGE_BIN_SIZE	(81)
typedef struct SceFaceAgeDistrData {
	float		score[SCE_FACE_AGE_BIN_SIZE];
	int			numIntegrated;
} SceFaceAgeDistrData;

int sceFaceAttribute(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceAttribDictPtr		attribDictPtr,
	const SceFaceDetectionResult	*detectedFace,
	const SceFacePartsResult		detectedPartsArray[],
	int								numDetectedParts,
	SceFaceAttribResult				resultAttribArray[],
	int								resultAttribArraySize,
	int								*resultAttribNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceAgeRangeEstimate(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceAgeDictPtr			ageDictPtr,
	const SceFaceDetectionResult	*detectedFace,
	const SceFacePartsResult		detectedPartsArray[],
	int								detectedPartsNum,
	SceFaceAgeRangeResult			*resultAge,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceAgeRangeIntegrate(
	const SceFaceAgeRangeResult		*ageRange,
	SceFaceAgeDistrData				*ageDistrData,
	int								*ageResult
);

int sceFaceAttributeGetWorkingMemorySize(
	int								width,
	int								height,
	int								rowstride,
	const SceFaceAttribDictPtr		attribDictPtr
);

int sceFaceAgeGetWorkingMemorySize(
	int								width,
	int								height,
	int								rowstride,
	const SceFaceAgeDictPtr			ageDictPtr
);

typedef void *SceFaceIdentifyDictPtr;

#define SCE_FACE_IDENTIFY_DICT			"face_identify.idt"
#define SCE_FACE_IDENTIFY_DICT_SIZE		(154736)

#define SCE_FACE_IDENTIFY_FEATURE_SIZE	(4096)
typedef struct SceFaceIdentifyFeature {
	unsigned char	data[SCE_FACE_IDENTIFY_FEATURE_SIZE];
} SceFaceIdentifyFeature;

int sceFaceIdentifyGetFeature(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceIdentifyDictPtr	identifyDictPtr,
	const SceFaceDetectionResult	*detectedFace,
	const SceFacePartsResult		detectedPartsArray[],
	int								detectedPartsNum,
	SceFaceIdentifyFeature			*resultFeature,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceIdentifySimilarity(
	const SceFaceIdentifyFeature	*extractedFeature,
	const SceFaceIdentifyFeature	registeredFeatureArray[],
	int								registeredFeatureNum,
	const SceFaceIdentifyDictPtr	identifyDictPtr,
	float							*maxScore,
	int								*maxScoreId,
	float							resultScoreArray[]
);

int sceFaceIdentifyGetWorkingMemorySize(
	int								width,
	int								height,
	int								rowstride,
	const SceFaceIdentifyDictPtr	identifyDictPtr
);

#define SCE_FACE_SHAPE_DICT_FRONTAL "face_shape_frontal.shp"
#define SCE_FACE_SHAPE_DICT_FRONTAL_SIZE (44560)

#define SCE_FACE_SHAPE_POINT_NUM_MAX (46)
#define SCE_FACE_SHAPE_POINT_NUM_FRONTAL (46)

#define SCE_FACE_SHAPE_MODEL_NUM_MAX (1)
#define SCE_FACE_SHAPE_MODEL_ID_FRONTAL (0)

#define SCE_FACE_SHAPE_SCORE_LOST_THRES_MIN (40)
#define SCE_FACE_SHAPE_SCORE_LOST_THRES_MAX (100)
#define SCE_FACE_SHAPE_SCORE_LOST_THRES_DEFAULT (55)

//Data
//----------------------------------
typedef void *SceFaceShapeModelDictPtr;

typedef struct tagSceFaceShapeResult {
	int modelID;

	int pointNum;
	float pointX[SCE_FACE_SHAPE_POINT_NUM_MAX];
	float pointY[SCE_FACE_SHAPE_POINT_NUM_MAX];
	int isLost[SCE_FACE_SHAPE_POINT_NUM_MAX];

	float fourPointX[SCE_FACE_PARTS_NUM_MAX];
	float fourPointY[SCE_FACE_PARTS_NUM_MAX];

	float rectCenterX;
	float rectCenterY;

	float rectWidth;
	float rectHeight;

	float faceRoll;
	float facePitch;
	float faceYaw;

	float score;

	unsigned char data[4096];
} SceFaceShapeResult;

//Function
//----------------------------------
int sceFaceShapeGetWorkingMemorySize(
	int								width,
	int								height,
	int								rowstride,
	const SceFaceShapeModelDictPtr	shapeDictPtr,
	int								maxFaceWidth,
	int								maxFaceHeight,
	bool							isVideoInput
);

int sceFaceShapeFit(
	const unsigned char				*imgPtr,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceShapeModelDictPtr	shapeDictPtr,
	SceFaceShapeResult				*shape,
	float							lostThreshold,
	const SceFaceDetectionResult	*detectedFace,
	const SceFacePartsResult		detectedPartsArray[],
	int								detectedPartsNum,
	void							*workMemory,
	int								workMemorySize
);

int sceFaceShapeTrack(
	const unsigned char				*imgPtrCur,
	const unsigned char				*imgPtrPrv,
	int								width,
	int								height,
	int								rowstride,
	const SceFaceShapeModelDictPtr	shapeDictPtr,
	SceFaceShapeResult				*shape,
	float							lostThreshold,
	void							*workMemory,
	int								workMemorySize
);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif
