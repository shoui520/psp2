/*
 * hwr_internal.h: internal types, globals, and declarations for SceHandwriting.
 */
#ifndef HWR_INTERNAL_H
#define HWR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libhandwriting.h>

#define HWR_MAX_STROKES			32
#define HWR_MAX_SEGMENTS		320
#define HWR_MAX_POINTS			320
#define HWR_MAX_PACKED_SEGS		64
#define HWR_MAX_CANDIDATES		32
#define HWR_MAX_COARSE_CANDIDATES	256
#define HWR_INVALID_SEG			0xFFFFu
#define HWR_INVALID_PTR			((uintptr_t)-1)

typedef struct HwrPoint {
	uint8_t x;
	uint8_t y;
} HwrPoint;

typedef struct HwrRawStroke {
	uint16_t pointCount;
	HwrPoint points[256];
} HwrRawStroke;

typedef struct HwrWorkBuffer {
	uint32_t codes[SCE_HANDWRITING_MAX_CANDIDATES];
	uint16_t scores[SCE_HANDWRITING_MAX_CANDIDATES];
	uint8_t pointBytes[2048];
} HwrWorkBuffer;

typedef struct HwrSegment {
	uint16_t startPt;
	uint16_t endPt;
	uint16_t prevSeg;
	uint16_t nextSeg;
	uint16_t length;
	int8_t direction;
	uint8_t normLength;
	uint8_t penDown;
	uint8_t reserved;
} HwrSegment;

typedef struct HwrStrokeBuffer {
	uint16_t strokeCount;
	uint16_t activeSegCount;
	uint16_t totalSegCount;
	uint16_t pointCount;
	uint16_t totalLength;
	HwrPoint points[HWR_MAX_POINTS];
	HwrSegment segments[HWR_MAX_SEGMENTS];
	uint16_t strokeFirstPt[HWR_MAX_STROKES];
	uint16_t strokeFirstSeg[HWR_MAX_STROKES];
	uint16_t strokeSegCount[HWR_MAX_STROKES];
	uint16_t strokeTotalLen[HWR_MAX_STROKES];
	uint16_t strokeMaxSegLen[HWR_MAX_STROKES];
	uint8_t strokeMinX[HWR_MAX_STROKES];
	uint8_t strokeMinY[HWR_MAX_STROKES];
	uint8_t strokeMaxX[HWR_MAX_STROKES];
	uint8_t strokeMaxY[HWR_MAX_STROKES];
	uint8_t charMinX;
	uint8_t charMinY;
	uint8_t charMaxX;
	uint8_t charMaxY;
	uint16_t charWidth;
	uint16_t charHeight;
	uint16_t charScale;
	uint16_t strokeGroup[HWR_MAX_STROKES];
} HwrStrokeBuffer;

struct HwrDictCharEntry;
struct HwrStrokeModel;

typedef struct HwrCoarseHeader {
	uint16_t templateCount;
	uint16_t maxCandidates;
	uint8_t *minStrokes;
	uint8_t *featureVectors;
	uint32_t *groupPool;
} HwrCoarseHeader;

typedef struct HwrDictCharEntry {
	uint16_t code;
	uint8_t category;
	uint8_t splitDirMask;
	uint8_t minStrokes;
	uint8_t maxStrokes;
	uint16_t flags;
	uint16_t extraCount;
	uint8_t layoutRules[2];
	struct HwrStrokeModel *firstModel;
	struct HwrDictCharEntry *leftRadical;
	struct HwrDictCharEntry *rightRadical;
} HwrDictCharEntry;

typedef struct HwrStrokeModel {
	struct HwrStrokeModel *nextByBucket;
	struct HwrStrokeModel *nextByChar;
	uint8_t *ruleBytecode;
	HwrDictCharEntry *ownerChar;
	uint16_t flags;
	uint16_t recordSize;
	uint8_t modelFlags;
	uint8_t strictStrokeCount;
	uint8_t minAspect;
	uint8_t maxAspect;
	uint8_t minStrokes;
	uint8_t maxStrokes;
	uint8_t reserved;
	uint8_t segmentCount;
	uint8_t segments[1];
} HwrStrokeModel;

typedef struct HwrDictHeader {
	uint16_t charCount;
	uint16_t version;
	uint16_t modelCount;
	uint8_t headerReserved[30];
	HwrDictCharEntry *charTable;
	HwrStrokeModel *modelTable;
	void *ruleTable;
	HwrCoarseHeader *coarseHeader;
	HwrStrokeModel *strokeBuckets[4];
	void *extraTable;
} HwrDictHeader;

typedef struct HwrCandidate {
	void *pattern;
	uint16_t distance;
	uint16_t code;
	uint16_t strokeCount;
	uint16_t subIndex;
} HwrCandidate;

typedef struct HwrCandidateList {
	HwrCandidate *items;
	uint16_t capacity;
	uint16_t count;
	uint16_t maxScore;
	uint16_t worstIndex;
} HwrCandidateList;

typedef struct HwrCandidateStage {
	HwrCandidate entries[HWR_MAX_STROKES][16];
	HwrCandidateList lists[HWR_MAX_STROKES];
} HwrCandidateStage;

typedef struct HwrUserPatternHeader {
	uint32_t internalCode;
	uint32_t reservedHeader;
	uint16_t segmentCount;
	uint16_t reservedShort;
	uint8_t minX;
	uint8_t minY;
	uint8_t maxX;
	uint8_t maxY;
	uint32_t reservedTail;
} HwrUserPatternHeader;

typedef struct HwrUserEntry {
	uint16_t used;
	uint8_t reserved;
	uint8_t checksum;
	uint32_t unicode;
	HwrUserPatternHeader header;
	uint32_t segments[HWR_MAX_PACKED_SEGS];
	uint8_t entryPadding[512 - 28 - HWR_MAX_PACKED_SEGS * 4];
} HwrUserEntry;

typedef struct HwrUserResult {
	uint16_t code;
	int16_t score;
} HwrUserResult;

typedef struct HwrPackedFeature {
	uint8_t segments[HWR_MAX_PACKED_SEGS];
	const uint8_t *segPtr;
	uint16_t segCount;
	uint16_t firstSeg;
	uint16_t lastSeg;
	uint16_t startStroke;
	uint16_t strokeCount;
	uint16_t strokeStartSeg[HWR_MAX_STROKES];
	uint16_t strokeEndSeg[HWR_MAX_STROKES];
} HwrPackedFeature;

typedef struct HwrDpAlignment {
	uint16_t startSeg[HWR_MAX_PACKED_SEGS + 1];
	uint16_t mainSeg[HWR_MAX_PACKED_SEGS + 1];
	uint16_t endSeg[HWR_MAX_PACKED_SEGS + 1];
} HwrDpAlignment;

typedef struct HwrCoarseMatch {
	uint16_t templateIdx;
	uint16_t distance;
} HwrCoarseMatch;

typedef struct HwrBBoxEntry {
	uint16_t maxY;
	uint16_t maxX;
	uint16_t minY;
	uint16_t minX;
} HwrBBoxEntry;

typedef struct HwrRuleCheckItem {
	HwrStrokeModel *model;
	uint16_t reserved;
	uint16_t listIndex;
	uint16_t padA;
	uint16_t padB;
} HwrRuleCheckItem;

typedef struct HwrGridWork {
	int16_t mapX[192];
	int16_t mapY[192];
	uint16_t featureHist[64];
} HwrGridWork;

typedef union HwrSharedWorkspace {
	HwrBBoxEntry bboxCache[528];
	HwrGridWork grid;
} HwrSharedWorkspace;

typedef struct HwrUnicodeMapEntry {
	uint16_t internalCode;
	uint16_t reserved;
	uint32_t unicode;
} HwrUnicodeMapEntry;

typedef struct HwrVersionAuthData {
	uint8_t keyRows[16][12];
	char versionId[16];
} HwrVersionAuthData;

typedef struct HwrKeyChunkDesc {
	const uint8_t *key;
	int8_t offset;
	int8_t reverse;
	uint16_t reserved;
} HwrKeyChunkDesc;

typedef struct HwrEngineConfig {
	uint16_t engineState;
	uint16_t prevContextCode;
	uint16_t prevPrevContextCode;
	uint16_t frameWidth;
	uint16_t frameHeight;
	uint32_t frameArea;
	uint16_t charCategoryMask;
	uint16_t contextMode;
	uint16_t matchOption;
	uint16_t extraStrokeTol;
	uint16_t missingStrokeTol;
} HwrEngineConfig;

/* Static lookup tables */
extern const char g_hwrLicenseKeyInput[17];
extern const char g_hwrLicenseKeyExpected[17];
extern const int16_t g_hwrProjectionTable[4096];
extern const uint16_t g_hwrSquareTable[162];
extern const uint8_t g_hwrSegThresholdTable[64];
extern const uint32_t g_hwrCosSqThresholdTable[8];
extern const int8_t g_hwrDirCostTable[16];
extern const int8_t g_hwrFirstSegDiscountTable[16];
extern const uint16_t g_hwrDpPruneTable[96];
extern const int8_t g_hwrDirAbsDiffTable[36];
extern const int8_t g_hwrDirSignedDiffTable[36];
extern const int16_t g_hwrGridPosCostTable[64];
extern const HwrVersionAuthData g_hwrVersionAuth;
extern const uint8_t g_hwrVersionCipher[24];

/* Engine state globals */
extern uint16_t g_hwrModeMask;
extern uint16_t g_hwrRevTriangular[33];
extern uint16_t g_hwrTriangular[33];
extern char g_hwrLicenseKeyBuf[24];
extern HwrRawStroke g_hwrRawStrokeBuf;
extern HwrCandidate g_hwrRecogResults[HWR_MAX_CANDIDATES];
extern HwrUserEntry *g_hwrUserRegBuf;
extern HwrEngineConfig g_hwrCfg;
extern int16_t g_hwrUserRegCapacity;
extern uint8_t g_hwrNormWidth;
extern uint8_t g_hwrNormHeight;
extern uint8_t g_hwrCoarseFallback[16];
extern HwrCoarseHeader *g_hwrCoarseDict;
extern const uint16_t *g_hwrSquareTablePtr;
extern const int16_t *g_hwrProjectionTablePtr;
extern const uint8_t *g_hwrSegThresholdPtr;
extern const uint32_t *g_hwrCosSqThresholdPtr;
extern const int8_t *g_hwrDirCostPtr;
extern const int8_t *g_hwrFirstSegDiscountPtr;
extern const uint16_t *g_hwrDpPrunePtr;
extern const int8_t *g_hwrDirAbsDiffPtr;
extern const int8_t *g_hwrDirSignedDiffPtr;
extern const uint16_t *g_hwrTriangularPtr;
extern const uint16_t *g_hwrRevTriangularPtr;
extern HwrDictHeader *g_hwrDictHeader;
extern HwrStrokeModel *g_hwrStrokeBuckets[4];
extern uint16_t g_hwrFineMatchPass;
extern uint16_t g_hwrRulePenalty;
extern uint16_t g_hwrRawByteCount;
extern HwrStrokeBuffer *g_hwrStrokeBuf;
extern uint8_t g_hwrDpPathBuf[4096];
extern uint16_t g_hwrCoarseResultCount;
extern uint16_t g_hwrCoarseTemplateCount;
extern HwrCoarseMatch g_hwrCoarseMatches[HWR_MAX_COARSE_CANDIDATES];
extern const uint8_t *g_hwrCoarseFeatures;
extern const uint32_t *g_hwrCoarseGroupPool;
extern const uint8_t *g_hwrCoarseMinStrokes;
extern const uint32_t *g_hwrCoarseGroupPtrs[30000];
extern HwrCandidateStage g_hwrStageStorage;
extern HwrCandidateStage *g_hwrStagePtr;
extern void *g_hwrDictBase;
extern uint8_t g_hwrGridBoundsX[8];
extern uint8_t g_hwrGridBoundsY[8];
extern uint8_t g_hwrResampledPoints[136];
extern HwrCandidateList g_hwrFinalList;
extern uint16_t g_hwrInterStrokeDirMask[36];
extern HwrDictCharEntry *g_hwrCoarseCharPtrs[HWR_MAX_COARSE_CANDIDATES];
extern uint8_t g_hwrCoarseRankIndex[HWR_MAX_COARSE_CANDIDATES];
extern uint16_t g_hwrCoarseCharCodes[HWR_MAX_COARSE_CANDIDATES];
extern HwrPackedFeature g_hwrPackedFeat;
extern HwrCandidateList *g_hwrActiveCandList;
extern HwrStrokeBuffer g_hwrStrokeBufStorage;
extern uint16_t g_hwrCoarseCharCount;
extern uint16_t g_hwrCoarseMaxCandidates;
extern HwrSharedWorkspace g_hwrWork;
extern HwrRuleCheckItem g_hwrRuleCheckQueue[HWR_MAX_CANDIDATES];
extern HwrRuleCheckItem *g_hwrRuleCheckQueuePtr;
extern uint16_t g_hwrRuleCheckCount;
extern HwrDpAlignment g_hwrDpAlign;
extern uint16_t g_hwrDpMatchCallCount;
extern uint16_t g_hwrRuleEvalCallCount;
extern HwrUserPatternHeader g_hwrUserRefHeader;
extern HwrUserPatternHeader g_hwrUserInputHeader;
extern uint32_t g_hwrUserRefSegments[HWR_MAX_PACKED_SEGS];
extern uint32_t g_hwrUserInputSegments[HWR_MAX_PACKED_SEGS];
extern HwrUserResult g_hwrUserResults[HWR_MAX_CANDIDATES];
extern int32_t g_hwrUserResultCount;

/* hwr_engine.c */
void *hwrMemcpy(void *dst, const void *src, SceSize len);
void *hwrMspaceCreate(void);
void hwrMspaceDestroy(void);
int hwrSetLicenseKey(const char *key);
int hwrEngineInit(uint16_t width, uint16_t height, void *dicBuf);
int hwrEngineTerm(void);
int hwrFeedStrokePoints(const uint8_t *pointBytes);
int hwrEngineRecognize(HwrWorkBuffer *workBuf);
uint16_t hwrBuildCategoryMask(uint16_t mode);
int hwrEngineSetMode(uint16_t mode);
int hwrUserRegInit(uint16_t capacity, HwrUserEntry *regBuf);
int hwrUserRegTerm(void);
int hwrUserRegAdd(uint32_t unicode, const uint8_t *pointBytes);
int hwrUserRegGetResult(uint32_t *codes, int16_t *scores);
int hwrUserRegDelete(uint32_t unicode);
int hwrUserRegGetInfo(uint32_t *codes);
void hwrInitTables(void);
int hwrCoreInit(uint16_t width, uint16_t height, uint16_t extraTol,
		uint16_t missingTol, void *dicBuf, int unusedA, int unusedB);
int hwrSessionBegin(void);
int hwrSessionEnd(void);
int hwrSessionAddStroke(HwrRawStroke *stroke, uint16_t packNow);
int hwrSessionRecognize(HwrCandidate *outResults, uint16_t maxOut, int16_t ctxCode);
int hwrCoreSetParams(uint16_t catMask, uint16_t matchOpt, uint16_t ctxMode,
		     uint16_t width, uint16_t height,
		     uint16_t extraTol, uint16_t missingTol);
int hwrCoreTerm(void);
void hwrResetMatchState(void);
int hwrPackStrokeFeatures(uint16_t lastStrokeIdx);
void hwrSetActiveCandList(HwrCandidateList *list);
unsigned int hwrDpMatch(HwrDpAlignment *align, const uint8_t *modelSegs,
			uint16_t modelSegCount, uint16_t maxCost);
void hwrMatchAllCoarseModels(uint16_t flags, uint16_t mode);

/* hwr_stroke.c */
int hwrStrokeInit(void);
int hwrStrokeTerm(void);
void hwrStrokeReset(void);
int hwrNormalizeSegLengths(void);
int hwrCompactSegments(void);
int hwrRebuildStrokeMetrics(void);
int hwrSimplifyStrokeCorners(void);
int hwrUpdateSingleStrokeMetrics(uint16_t startSeg);
int hwrTrimShortEndpoints(void);
int hwrMergeTinySegments(void);
int hwrCollapseZigzags(void);
int hwrMergeSameDirection(void);
int hwrProcessStroke(HwrRawStroke *stroke);
int hwrStrokesIntersect(uint16_t firstStroke, uint16_t secondStroke);
int hwrVectorDirectionAndLength(int16_t xStart, int16_t yStart,
				int16_t xEnd, int16_t yEnd, uint16_t *outLen);
int hwrVectorDirection(int16_t xStart, int16_t yStart, int16_t xEnd, int16_t yEnd);

/* hwr_code.c */
int hwrOctagonalDistance(int16_t xStart, int16_t yStart, int16_t xEnd, int16_t yEnd);
int hwrDescrambleLicenseKey(char *dst, const char *src, int len);
int hwrVerifyLicenseKey(void);
int hwrVerifyAuth(void);
uint32_t hwrInternalToUnicode(uint16_t internalCode);
uint16_t hwrUnicodeToInternal(uint32_t unicode);
int hwrStageInit(void);
int hwrStageTerm(void);
int hwrStageReset(void);
int hwrRunStrokeMatchPass(uint16_t strokeCount, uint16_t flags);
void hwrRelocateDict(void);
int hwrDictLoad(void *dicBuf);
int hwrDictUnload(void);

/* hwr_match.c */
int hwrComputeVectorAngle(const HwrPoint *firstStart, const HwrPoint *firstEnd,
			  const HwrPoint *secondStart, const HwrPoint *secondEnd);
int hwrGetStrokeRangeBBox(uint16_t startStroke, uint16_t strokeCount,
			  int16_t *minX, int16_t *minY, int16_t *maxX, int16_t *maxY);
int hwrEvaluateRules(const uint8_t *ruleBuf, uint16_t startStroke,
		     uint16_t strokeCount, uint16_t segOffset, int16_t passArg);
int hwrMatchModelList(HwrStrokeModel *model, uint16_t startStroke,
		      uint16_t strokeCount, uint16_t bestLimit);
int hwrClearBBoxCache(void);
int hwrExtractUserPattern(HwrUserPatternHeader *header, uint32_t *outSegments);
void hwrResetUserResults(void);
int hwrCopyUserResults(uint32_t *outCodes, int16_t *outScores);
int hwrUserDpMatch(void);
int hwrMatchUserEntries(void);
int hwrRefineCandidates(HwrCandidateList *finalList, HwrCandidateList *stageList);
int hwrCheckRadicalLayout(const HwrDictCharEntry *entry, uint16_t leftStart,
			  uint16_t leftCount, uint16_t rightStart, uint16_t rightCount);
int hwrMatchRadicalTree(const HwrDictCharEntry *entry, uint16_t startStroke,
			uint16_t strokeCount);
int hwrRecognizeCandidates(HwrCandidate *outResults, uint16_t maxOut);
void hwrInsertRuleCandidate(uint16_t baseIdx, uint16_t code, uint16_t penalty);
void hwrInsertCandidate(HwrCandidateList *list, void *pattern, uint16_t distance,
			uint16_t code, uint16_t strokeCount, int16_t subIndex,
			int16_t mode);
void hwrSortCandidates(HwrCandidateList *list, int16_t low, int16_t high);
int hwrGetCharCategory(uint16_t code);

/* hwr_util.c */
int hwrCoarseInit(void);
int hwrCoarseTerm(void);
int hwrRunCoarseMatch(void);
int hwrGetCoarsePriority(uint16_t code);
char *hwrStrcpy(char *dst, const char *src);
int hwrStrlen(const char *str);
int hwrStrcmp(const char *lhs, const char *rhs);
uint8_t *hwrMemset(uint8_t *dst, uint8_t value, uint32_t len);
int hwrMemcmp(const char *lhs, const char *rhs, int len);
void *hwrMemcpyFast(void *dst, const void *src, uint32_t len);
int hwrVerifyVersionSignature(void);
int hwrSearchCoarseTemplates(const uint16_t *hist, uint16_t strokeCount);
void hwrComputeGridBoundaries(const uint16_t *projection, uint8_t *bounds);
void hwrBuildGridMaps(uint16_t pointCount, const uint8_t *pointBytes);
void hwrExtractDirectionalFeature(uint16_t pointCount, const uint8_t *pointBytes);
void hwrBuildCoarseInputFeature(void);

#endif /* HWR_INTERNAL_H */
