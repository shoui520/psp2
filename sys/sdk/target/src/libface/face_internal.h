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
	uint32_t kind;			/* the attribute dictionary's set: "ALL1", "ALL3", "J7  " */
	int32_t relocated;		/* 1 once the loader has turned offsets into pointers */
};

/* a face as the engines take it: the API's result, its score reset */
struct FaceRegion {
	float x, y, w, h;
	float roll, pitch, yaw;
	int32_t partsId;		/* 0xffff: the whole face */
	int32_t label;			/* 0 (age), 0xffff (all parts) */
	int16_t level;
	int16_t valid;
};

/* a facial point as the engines take it */
struct FacePoint {
	int32_t pointId;		/* the dictionaries' point numbering */
	float x, y;
	float score;
};

/* the engines' result lists: a list takes its nodes from a pool the caller fills */
template <class T> struct FaceListNode {
	FaceListNode *prev;
	FaceListNode *next;
	T value;
};

template <class T> class FacePool {
public:
	int32_t reserved;
	FaceListNode<T> free;		/* the free nodes */
	int32_t blockSize;

	FacePool() : reserved(0), blockSize(16)
	{
		free.prev = &free;
		free.next = &free;
	}
	void add(FaceListNode<T> *node)
	{
		node->prev = free.prev;
		node->next = &free;
		free.prev->next = node;
		free.prev = node;
	}
};

template <class T> class FaceList {
public:
	FaceListNode<T> head;
	FacePool<T> *pool;

	class iterator {
	public:
		FaceListNode<T> *node;

		iterator(FaceListNode<T> *n) : node(n) {}
		bool operator!=(const iterator &other) const
		{
			return node != other.node;
		}
		iterator &operator++()
		{
			node = node->next;
			return *this;
		}
		T &operator*() const
		{
			return node->value;
		}
	};

	FaceList(FacePool<T> *p) : pool(p)
	{
		head.prev = &head;
		head.next = &head;
	}
	~FaceList()
	{
		clear();
	}
	bool empty() const
	{
		return head.prev == &head;
	}
	void clear()
	{
		while (!empty()) {
			FaceListNode<T> *node = head.next;
			FaceListNode<T> *prev = node->prev, *next = node->next;

			prev->next = next;
			next->prev = prev;
			pool->add(node);
		}
	}
	iterator begin()
	{
		return iterator(head.next);
	}
	iterator end()
	{
		return iterator(&head);
	}
};

typedef FaceListNode<FacePoint> FacePointNode;
typedef FacePool<FacePoint> FacePointPool;
typedef FaceList<FacePoint> FacePointList;

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

/* two pixel pairs' differences sharing one weight and threshold (the second pair: the Alt feature) */
struct FacePixelDiffPair {
	uint16_t offsetA, offsetB;
	uint16_t offsetA2, offsetB2;
	int16_t weight;
	int16_t threshold;
};

/* the same against two thresholds */
struct FacePixelDiff2Pair {
	uint16_t offsetA, offsetB;
	uint16_t offsetA2, offsetB2;
	int16_t weight1, weight2;
	int16_t threshold1, threshold2;
};

enum {
	FACE_FEATURE_PIXEL_DIFF_PAIR = 3,	/* FacePixelDiffPair elements */
	FACE_FEATURE_PIXEL_DIFF_PAIR_ALT = 4,	/* another feature's FacePixelDiffPair elements */
	FACE_FEATURE_PIXEL_DIFF2_PAIR = 5,	/* FacePixelDiff2Pair elements */
	FACE_FEATURE_PIXEL_DIFF2_PAIR_ALT = 6	/* another feature's FacePixelDiff2Pair elements */
};

class FacePixelDiffPairFeature : public FaceFeature {
public:
	int evaluate(const uint8_t *img, int sum) const;
	const void *element(int i) const;
	int extentX() const { return 0; }
	int extentY() const;
};

class FacePixelDiffPairAltFeature : public FacePixelDiffPairFeature {
public:
	int evaluate(const uint8_t *img, int sum) const;
};

class FacePixelDiff2PairFeature : public FaceFeature {
public:
	int evaluate(const uint8_t *img, int sum) const;
	const void *element(int i) const;
	int extentX() const { return 0; }
	int extentY() const;
};

class FacePixelDiff2PairAltFeature : public FacePixelDiff2PairFeature {
public:
	int evaluate(const uint8_t *img, int sum) const;
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

/* a boosted classifier over a weak set (sets of at most 91 weak classifiers) */
class FaceClassifier {
public:
	virtual ~FaceClassifier();
};

class FaceBoostClassifier : public FaceClassifier {
public:
	FaceWeakSet *m_set;

	FaceBoostClassifier();
	FaceBoostClassifier(FaceWeakSet *set);
	~FaceBoostClassifier();
};

/* a classifier's score lookup: count keys, then count values */
class FaceScoreTable {
public:
	int32_t m_count;
	const int32_t *m_keys;
	const int32_t *m_values;

	FaceScoreTable(int32_t count, const int32_t *keys, const int32_t *values);
};

struct FaceScoreTableRecord {
	int32_t count;
	int32_t reserved[2];		/* room for the object's pointers */
	int32_t data[];			/* count keys, count values */
};

/* the attribute dictionary: a stage per attribute */
struct FaceAttribStage {
	int32_t reserved;
	int32_t width;			/* the stage's window */
	int32_t height;
	int32_t reserved2[7];
	FaceBoostClassifier *classifier;
	FaceScoreTable *table;
};

struct FaceAttribDict {
	int32_t numStages;
	FaceAttribStage *stages;
	FaceAttribStage stageArray[];
};

/* a stage's block: room for its classifier object, then its weak set */
struct FaceAttribBlock {
	uint8_t classifier[8];
	FaceWeakSet set;
	FaceWeakRecord records[];
};

/* an attribute the estimator found: the dictionary's attribute number and its score */
struct FaceAttrib {
	int32_t id;
	float score;
};

typedef FaceListNode<FaceAttrib> FaceAttribNode;
typedef FacePool<FaceAttrib> FaceAttribPool;
typedef FaceList<FaceAttrib> FaceAttribList;

/* the dictionary's attribute numbers */
enum {
	FACE_ATTRIB_GENDER = 2,
	FACE_ATTRIB_ADULT = 3,
	FACE_ATTRIB_BABY = 4,
	FACE_ATTRIB_ELDER = 5,
	FACE_ATTRIB_GLASS = 10,
	FACE_ATTRIB_SMILE = 23,
	FACE_ATTRIB_REYEOPEN = 25,
	FACE_ATTRIB_LEYEOPEN = 26
};

class FaceAttribEstimator {
public:
	FaceAttribDict *m_dict;
	void *m_workMemory;
	int32_t m_workMemorySize;
	int32_t m_workMemorySet;
	int32_t m_reserved[3];

	FaceAttribEstimator();
	~FaceAttribEstimator();
	int setDict(FaceAttribDict *dict);
	int setWorkMemory(void *workMemory, int32_t workMemorySize);
	int estimate(const unsigned char *imgPtr, int width, int height, int rowstride,
		     const FaceRegion *face, const FacePoint *rEye, const FacePoint *lEye,
		     const FacePoint *nose, const FacePoint *mouth, FaceAttribList *attribs);
	static int64_t getWorkingMemorySize(int width, int height, int rowstride, FaceAttribDict *dict);
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

/* the parts dictionary's body, relocated by faceLoadPartsDict (parts.cpp) */
struct FacePartsDict;

int faceLoadPartsDict(FaceDictHeader *dict, FacePartsDict **parts);

/* the facial parts detector over a parts dictionary */
class FacePartsDetector {
public:
	FacePartsDict *m_dict;
	uint32_t m_engine[31];		/* PLACEHOLDER: laid out with the parts engine */

	FacePartsDetector();
	~FacePartsDetector();
	int setDict(FacePartsDict *dict);
	int setWorkMemory(void *workMemory, int32_t workMemorySize);
	int detect(const unsigned char *imgPtr, int width, int height, int rowstride,
		   const FaceRegion *face, FacePointList *points);
	static int64_t getWorkingMemorySize(int width, int height, int rowstride,
					    FacePartsDict *dict);
};

/* a shape dictionary's model for one range of yaw */
enum {
	FACE_SHAPE_POINTS = SCE_FACE_ALLPARTS_NUM_MAX,
	FACE_SHAPE_MODES_MAX = 30
};

struct FaceShapeModel {
	int32_t numModes;
	float modes[2 * FACE_SHAPE_POINTS * FACE_SHAPE_MODES_MAX];	/* numModes per coordinate */
	float mean[2 * FACE_SHAPE_POINTS];
	float variance[FACE_SHAPE_MODES_MAX];
};

#endif
