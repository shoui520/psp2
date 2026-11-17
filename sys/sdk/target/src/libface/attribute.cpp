/*
 * attribute.cpp: sceFaceAttribute (smile, eyes open, gender, age group, glasses) and the
 * attribute dictionary's loader.
 */
#include <string.h>

#include "face_internal.h"

enum {
	FACE_ATTRIB_NODES = 8		/* the attribute list's nodes, at the start of work memory */
};

static __attribute__((noinline)) int faceLoadAttribDict(FaceDictHeader *dict, FaceAttribDict **attrib);

int sceFaceAttribute(const unsigned char *imgPtr, int width, int height, int rowstride,
		     const SceFaceAttribDictPtr attribDictPtr,
		     const SceFaceDetectionResult *detectedFace,
		     const SceFacePartsResult detectedPartsArray[], int numDetectedParts,
		     SceFaceAttribResult resultAttribArray[], int resultAttribArraySize,
		     int *resultAttribNum, void *workMemory, int workMemorySize)
{
	FaceDictHeader *header = (FaceDictHeader *)attribDictPtr;
	FaceAttribDict *dict;
	FacePoint rEye, lEye, nose, mouth;
	FaceRegion face;
	int ret;

	if (imgPtr == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (rowstride < width)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (width <= 0)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (height <= 0)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (rowstride <= 0)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (detectedFace == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (detectedPartsArray == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (numDetectedParts != SCE_FACE_PARTS_NUM_MAX)
		return SCE_FACE_ERROR_IMPERF_PARTS;
	if (resultAttribArray == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (resultAttribArraySize <= 0)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (resultAttribNum == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	ret = faceLoadAttribDict(header, &dict);
	if (ret != SCE_OK)
		return ret;
	if (header->kind == *(const uint32_t *)"J7  " && resultAttribArraySize < 1)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (header->kind == *(const uint32_t *)"ALL3" && resultAttribArraySize < SCE_FACE_ATTRIB_NUM_MAX)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (header->kind == *(const uint32_t *)"ALL1" && resultAttribArraySize < SCE_FACE_ATTRIB_NUM_MAX)
		return SCE_FACE_ERROR_INVALID_PARAM;

	int64_t needed = FaceAttribEstimator::getWorkingMemorySize(width, height, rowstride, dict);
	int poolSize = FACE_ATTRIB_NODES * sizeof(FaceAttribNode);

	if (workMemory == NULL || workMemorySize <= 0 ||
	    (uint64_t)workMemorySize < (uint64_t)(needed + poolSize))
		return SCE_FACE_ERROR_NO_MEMORY;
	if (!(detectedFace->faceW >= 0.0f && detectedFace->faceW <= 2.0f &&
	      detectedFace->faceH >= 0.0f && detectedFace->faceH <= 2.0f))
		return SCE_FACE_ERROR_OUT_OF_RANGE;

	face.x = detectedFace->faceX;
	face.y = detectedFace->faceY;
	face.w = detectedFace->faceW;
	face.h = detectedFace->faceH;
	face.roll = detectedFace->faceRoll;
	face.pitch = detectedFace->facePitch;
	face.yaw = detectedFace->faceYaw;
	face.partsId = 0xffff;
	face.label = 0;
	face.level = 0;
	face.valid = 1;

	unsigned int found = 0;

	for (int i = 0; i < numDetectedParts; i++) {
		const SceFacePartsResult *p = &detectedPartsArray[i];

		if (p->partsId != SCE_FACE_PARTS_ID_UNDEF &&
		    (p->partsX < -0.5f || p->partsX > 1.5f || p->partsY < -0.5f || p->partsY > 1.5f))
			return SCE_FACE_ERROR_OUT_OF_RANGE;
		switch (p->partsId) {
		case SCE_FACE_PARTS_ID_R_EYE_CENTER:
			rEye.pointId = FACE_POINT_R_EYE_CENTER;
			found |= 1;
			rEye.x = p->partsX;
			rEye.y = p->partsY;
			rEye.score = p->score;
			break;
		case SCE_FACE_PARTS_ID_L_EYE_CENTER:
			lEye.pointId = FACE_POINT_L_EYE_CENTER;
			found |= 2;
			lEye.x = p->partsX;
			lEye.y = p->partsY;
			lEye.score = p->score;
			break;
		case SCE_FACE_PARTS_ID_NOSE_CENTER:
			nose.pointId = FACE_POINT_NOSE_CENTER;
			found |= 4;
			nose.x = p->partsX;
			nose.y = p->partsY;
			nose.score = p->score;
			break;
		case SCE_FACE_PARTS_ID_MOUTH_CENTER:
			mouth.pointId = FACE_POINT_MOUTH_CENTER;
			found |= 8;
			mouth.x = p->partsX;
			mouth.y = p->partsY;
			mouth.score = p->score;
			break;
		default:
			break;
		}
	}
	if (found != 0xf)
		return SCE_FACE_ERROR_IMPERF_PARTS;

	FaceAttribPool pool;
	FaceAttribList attribs(&pool);
	FaceAttribNode *nodes = (FaceAttribNode *)workMemory;

	for (int i = 0; i < FACE_ATTRIB_NODES; i++)
		pool.add(&nodes[i]);

	FaceAttribEstimator estimator;

	estimator.setDict(dict);
	estimator.setWorkMemory((char *)workMemory + poolSize, workMemorySize - poolSize);
	ret = estimator.estimate(imgPtr, width, height, rowstride, &face, &rEye, &lEye, &nose, &mouth,
				 &attribs);
	switch (ret) {
	case FACE_OK:
		break;
	case FACE_ERROR_WORK_MEMORY:
	case FACE_ERROR_NO_MEMORY:
		return SCE_FACE_ERROR_NO_MEMORY;
	case FACE_ERROR_PARAM:
	case FACE_ERROR_ARGUMENT:
		return SCE_FACE_ERROR_INVALID_PARAM;
	case FACE_ERROR_DICT:
		return SCE_FACE_ERROR_INVALID_DICT;
	case FACE_ERROR_RANGE:
		return SCE_FACE_ERROR_OUT_OF_RANGE;
	default:
		return SCE_FACE_ERROR_INVALID_PARAM;
	}
	if (ret != FACE_OK)
		return ret;

	int count = 0;
	SceFaceAttribResult *result = resultAttribArray;

	for (FaceAttribList::iterator it = attribs.begin(); it != attribs.end(); ++it) {
		const FaceAttrib &attrib = *it;

		if (attrib.id == FACE_ATTRIB_SMILE)
			result->attribId = SCE_FACE_ATTRIB_ID_SMILE;
		else if (attrib.id == FACE_ATTRIB_REYEOPEN)
			result->attribId = SCE_FACE_ATTRIB_ID_REYEOPEN;
		else if (attrib.id == FACE_ATTRIB_LEYEOPEN)
			result->attribId = SCE_FACE_ATTRIB_ID_LEYEOPEN;
		else if (attrib.id == FACE_ATTRIB_GENDER)
			result->attribId = SCE_FACE_ATTRIB_ID_GENDER;
		else if (attrib.id == FACE_ATTRIB_ADULT)
			result->attribId = SCE_FACE_ATTRIB_ID_ADULT;
		else if (attrib.id == FACE_ATTRIB_BABY)
			result->attribId = SCE_FACE_ATTRIB_ID_BABY;
		else if (attrib.id == FACE_ATTRIB_ELDER)
			result->attribId = SCE_FACE_ATTRIB_ID_ELDER;
		else if (attrib.id == FACE_ATTRIB_GLASS)
			result->attribId = SCE_FACE_ATTRIB_ID_GLASS;
		else
			result->attribId = SCE_FACE_ATTRIB_ID_UNDEF;
		result->score = attrib.score;
		result++;
		if (++count == resultAttribArraySize)
			break;
	}
	*resultAttribNum = count;
	return SCE_OK;
}

static __attribute__((noinline)) int faceLoadAttribDict(FaceDictHeader *dict, FaceAttribDict **attrib)
{
	uint32_t attr, pd31, pd32;

	if (dict == NULL)
		return SCE_FACE_ERROR_INVALID_DICT;
	memcpy(&attr, "ATTR", 4);
	if (dict->magic != attr)
		return SCE_FACE_ERROR_INVALID_DICT;

	uint32_t version = dict->version;

	memcpy(&pd31, "PD31", 4);
	memcpy(&pd32, "PD32", 4);
	if (version != pd31 && version != pd32)
		return SCE_FACE_ERROR_INVALID_DICT;
	if (*(const int32_t *)((const char *)dict + dict->size - 4) != dict->size)
		return SCE_FACE_ERROR_INVALID_DICT;

	FaceAttribDict *c = (FaceAttribDict *)((int32_t *)dict + 8);

	if (version == pd31)
		c = (FaceAttribDict *)((int32_t *)dict + 5);
	if (dict->relocated == 1) {
		*attrib = c;
		return SCE_OK;
	}

	c->stages = c->stageArray;

	char *p = (char *)&c->stageArray[c->numStages];

	for (int i = 0; i < c->numStages; i++) {
		FaceAttribBlock *b = (FaceAttribBlock *)p;
		FaceWeakSet *set = &b->set;
		int count = set->count;

		FaceWeakRecord *records = b->records;

		set->records = records;

		FaceFeature **features = (FaceFeature **)&records[count];

		set->features = features;

		char *f = (char *)&features[count];

		for (int j = 0; j < set->count; j++) {
			FaceFeature *feature;
			char *e = f + sizeof(FaceFeatureRecord);

			switch (((FaceFeatureRecord *)f)->type) {
			case FACE_FEATURE_PIXEL_DIFF:
				feature = new (f) FacePixelDiffFeature;
				f = e + sizeof(FacePixelDiff) * feature->count;
				feature->elements = e;
				break;
			case FACE_FEATURE_PIXEL_DIFF2: {
				feature = new (f) FacePixelDiff2Feature;
				int n = feature->count;

				feature->elements = e;
				f = e + sizeof(FacePixelDiff2) * n;
				break;
			}
			case FACE_FEATURE_PIXEL_DIFF_PAIR: {
				feature = new (f) FacePixelDiffPairFeature;
				int n = feature->count;

				feature->elements = e;
				f = e + sizeof(FacePixelDiffPair) * n;
				break;
			}
			case FACE_FEATURE_PIXEL_DIFF_PAIR_ALT: {
				feature = new (f) FacePixelDiffPairAltFeature;
				int ref = (int)feature->elements;

				f = e;
				feature->elements = features[j + ref]->elements;
				break;
			}
			case FACE_FEATURE_PIXEL_DIFF2_PAIR:
				feature = new (f) FacePixelDiff2PairFeature;
				f = e + sizeof(FacePixelDiff2Pair) * feature->count;
				feature->elements = e;
				break;
			case FACE_FEATURE_PIXEL_DIFF2_PAIR_ALT: {
				feature = new (f) FacePixelDiff2PairAltFeature;
				int ref = (int)feature->elements;

				f = e;
				feature->elements = features[j + ref]->elements;
				break;
			}
			default:
				return SCE_FACE_ERROR_INVALID_DICT;
			}
			set->features[j] = feature;
		}
		c->stages[i].classifier = new (b) FaceBoostClassifier(set);

		FaceScoreTableRecord *t = (FaceScoreTableRecord *)f;
		const int32_t *keys = t->data;
		int n = t->count;

		p = (char *)&keys[2 * n];
		c->stages[i].table = new (t) FaceScoreTable(n, keys, keys + n);
	}
	dict->relocated = 1;
	*attrib = c;
	return SCE_OK;
}
