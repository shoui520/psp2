/*
 * age.cpp: the age range API (sceFaceAgeRangeEstimate, sceFaceAgeRangeIntegrate) and the age
 * dictionary's loader.
 */
#include <string.h>

#include "face_internal.h"

int sceFaceAgeRangeEstimate(const unsigned char *imgPtr, int width, int height, int rowstride,
			    const SceFaceAgeDictPtr ageDictPtr,
			    const SceFaceDetectionResult *detectedFace,
			    const SceFacePartsResult detectedPartsArray[], int detectedPartsNum,
			    SceFaceAgeRangeResult *resultAge, void *workMemory, int workMemorySize)
{
	FaceCascade *dict;
	FaceAgeRange range;
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
	if (detectedPartsNum != SCE_FACE_PARTS_NUM_MAX)
		return SCE_FACE_ERROR_IMPERF_PARTS;
	if (resultAge == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	ret = faceLoadAgeDict((FaceDictHeader *)ageDictPtr, &dict);
	if (ret != SCE_OK)
		return ret;

	uint64_t needed = FaceAgeEstimator::getWorkingMemorySize(width, height, rowstride, dict);

	if (workMemory == NULL || workMemorySize <= 0 || (uint64_t)(int64_t)workMemorySize < needed)
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
	for (int i = 0; i < detectedPartsNum; i++) {
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

	FaceAgeEstimator estimator;

	estimator.setDict(dict);
	estimator.setWorkMemory(workMemory, workMemorySize);
	ret = estimator.estimate(imgPtr, width, height, rowstride, &face, &rEye, &lEye, &nose, &mouth,
				 &range);
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
	resultAge->minAge = range.minAge;
	resultAge->maxAge = range.maxAge;
	return SCE_OK;
}

/* the age range as a normal distribution (its mean and width), added to the running average */
static __attribute__((noinline)) void faceAddAgeDistribution(SceFaceAgeDistrData *distr, float mean, float range)
{
	float gauss[SCE_FACE_AGE_BIN_SIZE];
	float sigma = range * 0.5f;

	if (sigma == 0.0f)
		return;

	static const float sqrtTwoPi = powf(6.2831855f, 0.5f);
	float norm = 1.0f / (sqrtTwoPi * sigma);
	float twoVar = sigma * 2.0f * sigma;

	for (int i = 0; i < SCE_FACE_AGE_BIN_SIZE; i++) {
		float d = (float)i - mean;

		gauss[i] = norm * expf(-(d * d) / twoVar);
	}

	float n = (float)distr->numIntegrated;
	float n1 = n + 1.0f;

	for (int i = 0; i < SCE_FACE_AGE_BIN_SIZE; i++)
		distr->score[i] = (distr->score[i] * n + gauss[i]) / n1;
	distr->numIntegrated = (int)n1;
}

/* the age with the highest score */
static __attribute__((noinline)) void faceFindAgePeak(const SceFaceAgeDistrData *distr, int *age, float *score)
{
	int best = 0;
	float bestScore = 0.0f;

	for (int i = 0; i < SCE_FACE_AGE_BIN_SIZE; i++) {
		if (distr->score[i] > bestScore) {
			best = i;
			bestScore = distr->score[i];
		}
	}
	*age = best;
	*score = bestScore;
}

int sceFaceAgeRangeIntegrate(const SceFaceAgeRangeResult *ageRange,
			     SceFaceAgeDistrData *ageDistrData, int *ageResult)
{
	float score;

	if (ageRange == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (ageDistrData == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (ageResult == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;

	float mean = (float)(ageRange->maxAge + ageRange->minAge) * 0.5f;
	float range = (float)(ageRange->maxAge - ageRange->minAge);

	/* the width of the estimates the dictionary gives, per decade */
	if (mean >= 0.0f && mean < 10.0f)
		range *= 1.010101f;
	else if (mean >= 10.0f && mean < 20.0f)
		range *= 0.8084074f;
	else if (mean >= 20.0f && mean < 30.0f)
		range *= 1.216545f;
	else if (mean >= 30.0f && mean < 40.0f)
		range *= 1.519757f;
	else if (mean >= 40.0f && mean < 50.0f)
		range *= 0.8090615f;
	else if (mean >= 50.0f && mean < 60.0f)
		range *= 0.9363296f;
	else if (mean >= 60.0f)
		range *= 1.074114f;
	faceAddAgeDistribution(ageDistrData, mean, range);
	faceFindAgePeak(ageDistrData, ageResult, &score);
	return SCE_OK;
}

int faceLoadAgeDict(FaceDictHeader *dict, FaceCascade **cascade)
{
	if (dict == NULL)
		return SCE_FACE_ERROR_INVALID_DICT;
	uint32_t aget, pd31, pd32;

	memcpy(&aget, "AGET", 4);
	memcpy(&pd31, "PD31", 4);
	memcpy(&pd32, "PD32", 4);
	if (dict->magic != aget)
		return SCE_FACE_ERROR_INVALID_DICT;
	uint32_t version = dict->version;

	if (version != pd31 && version != pd32)
		return SCE_FACE_ERROR_INVALID_DICT;
	if (*(const int32_t *)((const char *)dict + dict->size - 4) != dict->size)
		return SCE_FACE_ERROR_INVALID_DICT;

	FaceCascade *c = (FaceCascade *)((int32_t *)dict + 8);

	if (version == pd31)
		c = (FaceCascade *)((int32_t *)dict + 5);
	if (dict->relocated == 1) {
		*cascade = c;
		return SCE_OK;
	}

	c->stages = c->stageArray;

	char *p = (char *)&c->stageArray[c->numStages];

	for (int i = 0; i < c->numStages; i++) {
		if (c->stages[i].shared != 0) {
			c->stages[i].classifier = c->stages[c->stages[i].classifierStage].classifier;
			c->stages[i].table = c->stages[c->stages[i].tableStage].table;
			continue;
		}

		FaceStageBlock *b = (FaceStageBlock *)p;
		FaceWeakSet *set = &b->set;

		set->records = b->records;

		char *q = (char *)&b->records[set->count];

		for (int j = 0; j < set->count; j++) {
			FaceWeakRecord *r = &set->records[j];
			int32_t *keys = NULL, *values = NULL;

			if (r->count > 0) {
				keys = (int32_t *)q;
				values = keys + r->count;
				q += 8 * r->count;
			}
			r->keys = keys;
			r->values = values;
		}
		set->features = (FaceFeature **)q;

		char *f = q + 4 * set->count;

		for (int j = 0; j < set->count; j++) {
			FaceFeature *feature;
			int size;

			if (((FaceFeatureRecord *)f)->type > FACE_FEATURE_PIXEL_DIFF) {
				if (((FaceFeatureRecord *)f)->type > FACE_FEATURE_PIXEL_DIFF2)
					return SCE_FACE_ERROR_INVALID_DICT;
				feature = new (f) FacePixelDiff2Feature;
				size = sizeof(FacePixelDiff2) * feature->count;
				feature->elements = f + sizeof(FaceFeatureRecord);
			} else {
				if (((FaceFeatureRecord *)f)->type <= 0)
					return SCE_FACE_ERROR_INVALID_DICT;
				feature = new (f) FacePixelDiffFeature;
				size = sizeof(FacePixelDiff) * feature->count;
				feature->elements = f + sizeof(FaceFeatureRecord);
			}
			f += sizeof(FaceFeatureRecord) + size;
			set->features[j] = feature;
		}
		c->stages[i].classifier = new (b) FaceStageClassifier(set);
		c->stages[i].table = (const int16_t *)f;
		p = f + sizeof(int16_t) * set->count;
	}
	dict->relocated = 1;
	*cascade = c;
	return SCE_OK;
}
