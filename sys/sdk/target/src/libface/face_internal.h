/*
 * face_internal.h: SceFace's internal types. Nothing in the binary names them (no debug
 * records, no RTTI): the names are descriptive (agent namecheck lists them).
 */
#ifndef FACE_INTERNAL_H
#define FACE_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <libface.h>

/* placement new, as the SDK's <new> defines it (the build keeps the tree's C headers first) */
__attribute__((noinline)) inline void *operator new(size_t, void *where) throw()
{
	return where;
}

/* the internal codes the engines return, mapped to SCE_FACE_ERROR_* by the API */
enum {
	FACE_OK = 0,
	FACE_ERROR_WORK_MEMORY = -1,
	FACE_ERROR_NO_MEMORY = -2,
	FACE_ERROR_PARAM = -3,
	FACE_ERROR_DICT = -4,
	FACE_ERROR_ARGUMENT = -5,
	FACE_ERROR_RANGE = -6
};

/* a dictionary file: a header, then the body the loader relocates in place */
#define FACE_DICT_MAGIC_AGE	0x54454741	/* "AGET" */
#define FACE_DICT_VERSION_PD31	0x31334450	/* "PD31" */
#define FACE_DICT_VERSION_PD32	0x32334450	/* "PD32" */

struct FaceDictHeader {
	uint32_t magic;
	int32_t size;			/* the file's size, also its last word */
	uint32_t version;		/* PD31: the body follows; PD32: three more words first */
	int32_t reserved;
	int32_t relocated;		/* 1 once the loader has turned offsets into pointers */
};

/* a face as the engines take it: the API's result, its score reset */
struct FaceRegion {
	float x, y, w, h;
	float roll, pitch, yaw;
	int32_t partsId;		/* 0xffff: the whole face */
	float score;
	int16_t level;
	int16_t valid;
};

/* a facial point as the engines take it */
struct FacePoint {
	int32_t pointId;		/* the dictionaries' point numbering */
	float x, y;
	float score;
};

/* the dictionaries' point numbers of the four parts sceFaceParts finds */
enum {
	FACE_POINT_NOSE_CENTER = 38,
	FACE_POINT_R_EYE_CENTER = 60,
	FACE_POINT_L_EYE_CENTER = 61,
	FACE_POINT_MOUTH_CENTER = 62
};

/* a feature record as the dictionary stores it, before the loader makes it an object */
struct FaceFeatureRecord {
	int32_t vtable;			/* room for the object's vtable pointer */
	int32_t type;
	int32_t count;
	int32_t bias;
	int32_t elements;		/* room for the elements pointer */
};

/*
 * A weak classifier's feature: a record in the dictionary, made an object in place by the
 * loader (its constructor sets only the vtable). Its elements follow the 20-byte record.
 */
class FaceFeature {
public:
	int32_t type;			/* FACE_FEATURE_* */
	int32_t count;			/* elements */
	int32_t bias;			/* added to evaluate's sum */
	const void *elements;

	virtual ~FaceFeature() {}
	virtual int evaluate(const uint8_t *img, int sum) const = 0;
	virtual const void *element(int i) const = 0;
	virtual int extentX() const = 0;
	virtual int extentY() const = 0;
};

enum {
	FACE_FEATURE_PIXEL_DIFF = 1,		/* FacePixelDiff elements */
	FACE_FEATURE_PIXEL_DIFF2 = 2		/* FacePixelDiff2 elements */
};

/* two pixels' difference against a threshold: weight added when above */
struct FacePixelDiff {
	uint16_t offsetA, offsetB;
	int16_t weight;
	int16_t threshold;
};

/* the same against two thresholds, each with its weight */
struct FacePixelDiff2 {
	uint16_t offsetA, offsetB;
	int16_t weight1, weight2;
	int16_t threshold1, threshold2;
};

class FacePixelDiffFeature : public FaceFeature {
public:
	~FacePixelDiffFeature();
	int evaluate(const uint8_t *img, int sum) const;
	const void *element(int i) const;
	int extentX() const { return 0; }
	int extentY() const;
};

class FacePixelDiff2Feature : public FaceFeature {
public:
	~FacePixelDiff2Feature();
	int evaluate(const uint8_t *img, int sum) const;
	const void *element(int i) const;
	int extentX() const { return 0; }
	int extentY() const;
};

/* a weak classifier's lookup record */
struct FaceWeakRecord {
	int32_t reserved[3];
	int32_t count;
	int32_t *keys;			/* count words after the records */
	int32_t *values;		/* count words after the keys */
	int32_t reserved2;
};

/* a stage's weak classifiers, as the dictionary stores them */
struct FaceWeakSet {
	int32_t count;
	int32_t reserved[2];
	FaceWeakRecord *records;	/* the count records after this */
	FaceFeature **features;		/* count pointers after the records' tables */
	int32_t reserved2[3];
};

/* the object the loader puts in front of a stage's weak set */
class FaceStageClassifier {
public:
	FaceStageClassifier(FaceWeakSet *set);
	virtual ~FaceStageClassifier();
	FaceWeakSet *m_set;
};

/* a stage of the cascade; a shared stage names another stage's classifier and table */
struct FaceStage {
	int32_t reserved[5];
	int32_t shared;
	int32_t reserved2;
	union {
		FaceStageClassifier *classifier;
		int32_t classifierStage;	/* shared: the stage that has it */
	};
	union {
		const int16_t *table;		/* a weight per weak classifier, after the features */
		int32_t tableStage;
	};
};

/* a stage's block: room for its classifier object, then its weak set */
struct FaceStageBlock {
	uint8_t classifier[8];
	FaceWeakSet set;
	FaceWeakRecord records[];
};

/* the body of a cascade dictionary */
struct FaceCascade {
	int32_t numStages;
	FaceStage *stages;
	FaceStage stageArray[];
};

/* the age estimator (an attribute classifier over the age dictionary's stages) */
struct FaceAgeRange {
	uint8_t minAge;
	uint8_t maxAge;
};

class FaceAgeEstimator {
public:
	FaceCascade *m_dict;
	void *m_workMemory;
	int32_t m_workMemorySize;
	int32_t m_workMemorySet;
	int32_t m_reserved[3];

	FaceAgeEstimator();
	~FaceAgeEstimator();
	void setDict(FaceCascade *dict);
	int setWorkMemory(void *workMemory, int32_t workMemorySize);
	int estimate(const unsigned char *imgPtr, int width, int height, int rowstride,
		     const FaceRegion *face, const FacePoint *rEye, const FacePoint *lEye,
		     const FacePoint *nose, const FacePoint *mouth, FaceAgeRange *result);
	static uint64_t getWorkingMemorySize(int width, int height, int rowstride, FaceCascade *dict);
};

int faceLoadAgeDict(FaceDictHeader *dict, FaceCascade **cascade);

#endif
