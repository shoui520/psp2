#include <sce_atomic.h>

#define CommonNextSequence ScePafCommon_AC2F3710

static volatile int64_t s_sequence __attribute__((aligned(8)));

uint64_t CommonNextSequence(void)
{
	return (uint64_t)sceAtomicIncrement64AcqRel(&s_sequence);
}
