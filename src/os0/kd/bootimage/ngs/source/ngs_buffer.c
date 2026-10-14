/* Audio-buffer references used while assigning module input/output buffers. */

#include <stddef.h>

typedef struct NgsBufferReference {
	int referenceCount;
	unsigned int moduleIndex;
	unsigned int outputIndex;
	struct NgsBufferReference *alias;
} NgsBufferReference;

typedef char NgsBufferReferenceSize[(sizeof(NgsBufferReference) == 16) ? 1 : -1];

void ngsReleaseBufferReference(NgsBufferReference *reference,
			       NgsBufferReference **alias)
{
	NgsBufferReference *first;
	NgsBufferReference *second;
	NgsBufferReference *third;

	--reference->referenceCount;
	first = *alias;
	if (first != NULL && reference->referenceCount == 0) {
		--first->referenceCount;
		second = first->alias;
		if (second != NULL && first->referenceCount == 0) {
			--second->referenceCount;
			third = second->alias;
			if (third != NULL && second->referenceCount == 0) {
				ngsReleaseBufferReference(third, &third->alias);
				second->alias = NULL;
			}
			first->alias = NULL;
		}
		*alias = NULL;
	}
}
