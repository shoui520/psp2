#ifndef PAF_COMMON_PATH_H
#define PAF_COMMON_PATH_H

#include "../std/string.h"

#define PathSplit _ZN3paf6common4path5SplitERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERS7_SA_
#define PathSplitExt _ZN3paf6common4path8SplitExtERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEERS7_SA_
#define PathJoin _ZN3paf6common4path4JoinERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEES9_
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
