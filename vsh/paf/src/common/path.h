#ifndef PAF_COMMON_PATH_H
#define PAF_COMMON_PATH_H

#include "../std/string.h"

#define PathSplit ScePafCommon_09B0ACB5
#define PathSplitExt ScePafCommon_B504F2E1
#define PathJoin ScePafCommon_92A62443
#define PathNormalize ScePafCommon_19056E57

void PathSplit(const PafString *path, PafString *base, PafString *file);
void PathSplitExt(const PafString *path, PafString *filepath, PafString *ext);
void PathJoin(PafString *result, const PafString *left, const PafString *right);
/* Descriptive name; the original export has no recovered Sony spelling. */
void PathNormalize(PafString *result, const PafString *path);

typedef struct PathSegment {
	size_t start;
	size_t len;
} PathSegment;

typedef struct PathSegmentVector {
	PathSegment *_data;
	size_t _size;
	size_t _capacity;
	unsigned char _allocator;
} PathSegmentVector;

typedef char PathSegmentSizeCheck[sizeof(PathSegment) == 8 ? 1 : -1];
typedef char PathSegmentVectorSizeCheck[sizeof(PathSegmentVector) == 16 ? 1 : -1];

int PathSegmentVectorReallocate(PathSegmentVector *self, size_t capacity);

#endif
