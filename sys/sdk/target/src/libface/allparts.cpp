/*
 * allparts.cpp: sceFaceAllParts, the 55 facial points: the parts detector's points, then the
 * shape dictionary's model fitted to them.
 */
#include <string.h>

#include "face_internal.h"

enum {
	FACE_ALLPARTS_NODES = 110		/* the point list's nodes, at the start of work memory */
};

/* the result index of each of the parts dictionary's points (0xff: not a result) */
static const unsigned char faceAllPartsIndex[60] = {
	0xff,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,
	  11,   12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,
	  23,   24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,
	  35,   36,   37, 0xff, 0xff,   38,   39,   40, 0xff, 0xff,   41,   42,
	  43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54
};

/* the parts id of each result */
static const unsigned char faceAllPartsId[SCE_FACE_ALLPARTS_NUM_MAX] = {
	SCE_FACE_PARTS_FACE_ID_ALL_00, SCE_FACE_PARTS_FACE_ID_ALL_01,
	SCE_FACE_PARTS_FACE_ID_ALL_02, SCE_FACE_PARTS_FACE_ID_ALL_03,
	SCE_FACE_PARTS_FACE_ID_ALL_04, SCE_FACE_PARTS_FACE_ID_ALL_05,
	SCE_FACE_PARTS_FACE_ID_ALL_06, SCE_FACE_PARTS_FACE_ID_ALL_07,
	SCE_FACE_PARTS_FACE_ID_ALL_08, SCE_FACE_PARTS_FACE_ID_ALL_09,
	SCE_FACE_PARTS_FACE_ID_ALL_10, SCE_FACE_PARTS_FACE_ID_ALL_11,
	SCE_FACE_PARTS_R_EYEBROW_ID_ALL_12, SCE_FACE_PARTS_R_EYEBROW_ID_ALL_13,
	SCE_FACE_PARTS_R_EYEBROW_ID_ALL_14, SCE_FACE_PARTS_R_EYE_ID_ALL_15,
	SCE_FACE_PARTS_R_EYE_ID_ALL_16, SCE_FACE_PARTS_R_EYE_ID_ALL_17,
	SCE_FACE_PARTS_R_EYE_ID_ALL_18, SCE_FACE_PARTS_R_EYE_ID_ALL_19,
	SCE_FACE_PARTS_R_EYE_ID_ALL_20, SCE_FACE_PARTS_R_EYE_ID_ALL_21,
	SCE_FACE_PARTS_R_EYE_ID_ALL_22, SCE_FACE_PARTS_R_PUPIL_ID_ALL_23,
	SCE_FACE_PARTS_L_EYEBROW_ID_ALL_24, SCE_FACE_PARTS_L_EYEBROW_ID_ALL_25,
	SCE_FACE_PARTS_L_EYEBROW_ID_ALL_26, SCE_FACE_PARTS_L_EYE_ID_ALL_27,
	SCE_FACE_PARTS_L_EYE_ID_ALL_28, SCE_FACE_PARTS_L_EYE_ID_ALL_29,
	SCE_FACE_PARTS_L_EYE_ID_ALL_30, SCE_FACE_PARTS_L_EYE_ID_ALL_31,
	SCE_FACE_PARTS_L_EYE_ID_ALL_32, SCE_FACE_PARTS_L_EYE_ID_ALL_33,
	SCE_FACE_PARTS_L_EYE_ID_ALL_34, SCE_FACE_PARTS_L_PUPIL_ID_ALL_35,
	SCE_FACE_PARTS_NOSE_ID_ALL_36, SCE_FACE_PARTS_NOSE_ID_ALL_37,
	SCE_FACE_PARTS_NOSTRIL_ID_ALL_40, SCE_FACE_PARTS_NOSTRIL_ID_ALL_41,
	SCE_FACE_PARTS_NOSTRIL_ID_ALL_42, SCE_FACE_PARTS_R_LIP_ID_ALL_45,
	SCE_FACE_PARTS_U_LIP_ID_ALL_46, SCE_FACE_PARTS_U_LIP_ID_ALL_47,
	SCE_FACE_PARTS_U_LIP_ID_ALL_48, SCE_FACE_PARTS_L_LIP_ID_ALL_49,
	SCE_FACE_PARTS_D_LIP_ID_ALL_50, SCE_FACE_PARTS_D_LIP_ID_ALL_51,
	SCE_FACE_PARTS_D_LIP_ID_ALL_52, SCE_FACE_PARTS_UC_LIP_ID_ALL_53,
	SCE_FACE_PARTS_UC_LIP_ID_ALL_54, SCE_FACE_PARTS_UC_LIP_ID_ALL_55,
	SCE_FACE_PARTS_DC_LIP_ID_ALL_56, SCE_FACE_PARTS_DC_LIP_ID_ALL_57,
	SCE_FACE_PARTS_DC_LIP_ID_ALL_58
};

static __attribute__((noinline)) int faceFitShape(SceFacePartsResult *parts, int numModes, const float *modes,
			const float *mean, const float *variance);
static __attribute__((noinline)) int faceLoadShapeDict(FaceDictHeader *dict, float yaw, int *numModes, const float **modes,
			     const float **mean, const float **variance);

int sceFaceAllParts(const unsigned char *imgPtr, int width, int height, int rowstride,
		    const SceFacePartsDictPtr partsDictPtr, const SceFaceShapeDictPtr shapeDictPtr,
		    int xScanStep, int yScanStep, const SceFaceDetectionResult *detectedFace,
		    SceFacePartsResult resultPartsArray[], int resultPartsArraySize,
		    int *resultPartsNum, void *workMemory, int workMemorySize)
{
	FacePartsDict *dict;
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
	if (xScanStep <= 0 || width < xScanStep)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (yScanStep <= 0 || height < yScanStep)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (detectedFace == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (resultPartsArray == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (resultPartsArraySize < SCE_FACE_ALLPARTS_NUM_MAX)
		return SCE_FACE_ERROR_INVALID_PARAM;
	if (resultPartsNum == NULL)
		return SCE_FACE_ERROR_INVALID_PARAM;
	ret = faceLoadPartsDict((FaceDictHeader *)partsDictPtr, &dict);
	if (ret != SCE_OK)
		return ret;

	int64_t needed = FacePartsDetector::getWorkingMemorySize(width, height, rowstride, dict);
	int poolSize = FACE_ALLPARTS_NODES * sizeof(FacePointNode);

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
	face.label = 0xffff;
	face.level = 0;
	face.valid = 1;

	FacePointPool pool;
	FacePointList points(&pool);
	FacePointNode *nodes = (FacePointNode *)workMemory;

	for (int i = 0; i < FACE_ALLPARTS_NODES; i++)
		pool.add(&nodes[i]);

	{
		FacePartsDetector detector;

		detector.setDict(dict);
		detector.setWorkMemory((char *)workMemory + poolSize, workMemorySize - poolSize);
		ret = detector.detect(imgPtr, width, height, rowstride, &face, &points);
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
	}

	for (int i = 0; i < resultPartsArraySize; i++) {
		resultPartsArray[i].partsId = SCE_FACE_PARTS_ID_UNDEF;
		resultPartsArray[i].partsX = 0.0f;
		resultPartsArray[i].partsY = 0.0f;
		resultPartsArray[i].score = 0.0f;
	}

	int found = 0;

	for (FacePointList::iterator it = points.begin(); it != points.end(); ++it) {
		const FacePoint &point = *it;
		int id = point.pointId;
		unsigned int partsId = id + SCE_FACE_PARTS_ID_ALL_BASE - 1;
		int index = faceAllPartsIndex[id];

		if (index < 0 || index > resultPartsArraySize)
			continue;
		resultPartsArray[index].partsId = partsId;
		resultPartsArray[index].partsX = point.x;
		resultPartsArray[index].partsY = point.y;
		resultPartsArray[index].score = point.score;
		found++;
	}
	if (shapeDictPtr != NULL) {
		int numModes;
		const float *modes, *mean, *variance;

		ret = faceLoadShapeDict((FaceDictHeader *)shapeDictPtr, detectedFace->faceYaw,
					&numModes, &modes, &mean, &variance);
		if (ret != SCE_OK)
			return ret;
		if (faceFitShape(resultPartsArray, numModes, modes, mean, variance) == SCE_OK)
			found = SCE_FACE_ALLPARTS_NUM_MAX;
	}
	*resultPartsNum = found;
	return SCE_OK;
}

/* the points the detector found, with the model's mean and modes for each */
static __attribute__((noinline)) int faceCollectShapePoints(const SceFacePartsResult *parts, int numModes, const float *modes,
				  const float *mean, float *basis, float *meanPoints, float *points)
{
	int n = 0;

	for (int i = 0; i < SCE_FACE_ALLPARTS_NUM_MAX; i++) {
		if (parts[i].partsId != SCE_FACE_PARTS_ID_UNDEF) {
			points[0] = parts[i].partsX;
			points[1] = parts[i].partsY;
			for (int m = 0; m < numModes; m++) {
				basis[2 * n * numModes + m] = modes[2 * i * numModes + m];
				basis[(2 * n + 1) * numModes + m] = modes[(2 * i + 1) * numModes + m];
			}
			meanPoints[0] = mean[2 * i];
			meanPoints[1] = mean[2 * i + 1];
			meanPoints += 2;
			points += 2;
			n++;
		}
	}
	return n;
}

/* the similarity transform (scale and rotation a, b; translation tx, ty) taking p to q */
static inline void faceEstimateSimilarity(const float *p, const float *q, int n, float *a, float *b,
					  float *tx, float *ty)
{
	float sqx = 0.0f, sqy = 0.0f, spx = 0.0f, spy = 0.0f;
	float spp = 0.0f, cnt = 0.0f, s1 = 0.0f, s2 = 0.0f;

	for (int k = 0; k < n; k++) {
		float qx = q[2 * k], qy = q[2 * k + 1];
		float px = p[2 * k], py = p[2 * k + 1];

		cnt += 1.0f;
		sqx += qx;
		sqy += qy;
		spx += px;
		spy += py;
		spp += py * py + px * px;
		s1 += qy * py + qx * px;
		s2 += qy * px - qx * py;
	}

	float d = (spx * spx + spy * spy - spp * cnt) /
		  (spy * spy * (spy * spy) + spx * spx * (spx * spx) + spp * spp * (cnt * cnt) +
		   spx * spx * 2.0f * (spy * spy) - spx * spx * 2.0f * (spp * cnt) -
		   spy * spy * 2.0f * (spp * cnt));
	float dp = d * spp, dy = d * spy, dx = d * spx, dc = d * cnt;

	*a = sqy * dy + sqx * dx - s1 * dc;
	*b = dx * sqy - sqx * dy - s2 * dc;
	*tx = s1 * dx - sqx * dp - s2 * dy;
	*ty = s1 * dy - sqy * dp + s2 * dx;
}

/*
 * The shape model fitted to the points found: the points aligned to the model, the modes'
 * weights by least squares (clamped to the model's range), the model's 55 points back in the
 * image.
 */
static __attribute__((noinline)) int faceFitShape(SceFacePartsResult *parts, int numModes, const float *modes,
			const float *mean, const float *variance)
{
	int pivot[FACE_SHAPE_MODES_MAX];
	float weight[FACE_SHAPE_MODES_MAX];
	float aligned[2 * FACE_SHAPE_POINTS];
	float meanPoints[2 * FACE_SHAPE_POINTS];
	float points[2 * FACE_SHAPE_POINTS];
	float shape[2 * FACE_SHAPE_POINTS];
	float normal[FACE_SHAPE_MODES_MAX * FACE_SHAPE_MODES_MAX];
	float basis[2 * FACE_SHAPE_POINTS * FACE_SHAPE_MODES_MAX];
	float pseudoInverse[FACE_SHAPE_MODES_MAX * 2 * FACE_SHAPE_POINTS];
	float a, b, tx, ty;

	int n = faceCollectShapePoints(parts, numModes, modes, mean, basis, meanPoints, points);

	if (n < SCE_FACE_ALLPARTS_NUM_MAX * 0.8)
		return SCE_FACE_ERROR_IMPERF_PARTS;

	faceEstimateSimilarity(points, meanPoints, n, &a, &b, &tx, &ty);
	for (int k = 0; k < n; k++) {
		float x = points[2 * k], y = points[2 * k + 1];

		aligned[2 * k] = a * x - b * y + tx;
		aligned[2 * k + 1] = a * y + b * x + ty;
	}
	faceEstimateSimilarity(meanPoints, points, n, &a, &b, &tx, &ty);

	/* the normal matrix of the modes at the points found, inverted in place */
	for (int i = 0; i < numModes; i++) {
		for (int j = 0; j < numModes; j++) {
			float sum = 0.0f;

			for (int k = 0; k < 2 * n; k++)
				sum += basis[k * numModes + i] * basis[k * numModes + j];
			normal[i * numModes + j] = sum;
		}
	}
	for (int c = 0; c < numModes; c++) {
		float max = 0.0f;
		int row = 0;

		for (int r = c; r < numModes; r++) {
			float v = fabsf(normal[r * numModes + c]);

			if (v > max) {
				max = v;
				row = r;
			}
		}
		if (fabsf(max) < 0.000001f)
			return SCE_FACE_ERROR_INVALID_PARAM;
		pivot[c] = row;
		if (c != row) {
			for (int j = 0; j < numModes; j++) {
				float t = normal[c * numModes + j];

				normal[c * numModes + j] = normal[row * numModes + j];
				normal[row * numModes + j] = t;
			}
		}

		float inv = 1.0f / normal[c * numModes + c];

		normal[c * numModes + c] = 1.0f;
		for (int j = 0; j < numModes; j++)
			normal[c * numModes + j] *= inv;
		for (int r = 0; r < numModes; r++) {
			if (r == c)
				continue;

			float f = normal[r * numModes + c];

			normal[r * numModes + c] = 0.0f;
			for (int j = 0; j < numModes; j++)
				normal[r * numModes + j] -= f * normal[c * numModes + j];
		}
	}
	for (int c = numModes - 1; c >= 0; c--) {
		int p = pivot[c];

		if (c != p) {
			for (int r = 0; r < numModes; r++) {
				float t = normal[r * numModes + c];

				normal[r * numModes + c] = normal[r * numModes + p];
				normal[r * numModes + p] = t;
			}
		}
	}

	for (int i = 0; i < numModes; i++) {
		for (int k = 0; k < 2 * n; k++) {
			float sum = 0.0f;

			for (int j = 0; j < numModes; j++)
				sum += normal[i * numModes + j] * basis[k * numModes + j];
			pseudoInverse[i * 2 * n + k] = sum;
		}
	}
	for (int i = 0; i < numModes; i++) {
		float sum = 0.0f;

		for (int k = 0; k < 2 * n; k++)
			sum += pseudoInverse[i * 2 * n + k] * (aligned[k] - meanPoints[k]);
		if (sum > variance[i])
			sum = variance[i];
		if (sum < -variance[i])
			sum = -variance[i];
		weight[i] = sum;
	}
	for (int k = 0; k < 2 * FACE_SHAPE_POINTS; k++) {
		shape[k] = mean[k];
		for (int j = 0; j < numModes; j++)
			shape[k] += modes[k * numModes + j] * weight[j];
	}
	for (int k = 0; k < FACE_SHAPE_POINTS; k++) {
		float x = shape[2 * k], y = shape[2 * k + 1];

		points[2 * k] = a * x - b * y + tx;
		points[2 * k + 1] = a * y + b * x + ty;
	}
	for (int k = 0; k < FACE_SHAPE_POINTS; k++) {
		parts[k].partsId = faceAllPartsId[k];
		parts[k].partsX = points[2 * k];
		parts[k].partsY = points[2 * k + 1];
		parts[k].score = -1.0f;
	}
	return SCE_OK;
}

/* the shape dictionary's model for the face's yaw */
static __attribute__((noinline)) int faceLoadShapeDict(FaceDictHeader *dict, float yaw, int *numModes, const float **modes,
			     const float **mean, const float **variance)
{
	uint32_t allp, shp1, shp2;

	if (dict == NULL)
		return SCE_FACE_ERROR_INVALID_DICT;
	memcpy(&allp, "ALLP", 4);
	memcpy(&shp1, "SHP1", 4);
	memcpy(&shp2, "SHP2", 4);
	if (dict->magic != allp)
		return SCE_FACE_ERROR_INVALID_DICT;
	if (dict->version != shp1 && dict->version != shp2)
		return SCE_FACE_ERROR_INVALID_DICT;
	if (*(const int32_t *)((const char *)dict + dict->size - 4) != dict->size)
		return SCE_FACE_ERROR_INVALID_DICT;

	FaceShapeModel *model = (FaceShapeModel *)((int32_t *)dict + 8);

	if (dict->version == shp1)
		model = (FaceShapeModel *)((int32_t *)dict + 5);

	int bin;

	if (yaw < -22.5f)
		bin = 0;
	else if (yaw < 0.0f)
		bin = 1;
	else if (yaw <= 22.5f)
		bin = 2;
	else
		bin = 3;
	model += bin;
	*numModes = model->numModes;
	*modes = model->modes;
	*mean = model->mean;
	*variance = model->variance;
	return SCE_OK;
}
