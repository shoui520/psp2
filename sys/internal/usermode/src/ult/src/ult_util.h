/*
 * ult_util.h: what every part of libult uses: the checks that stop at a breakpoint, the
 * option parameters' header, the objects' names and the atomic counters.
 *
 * A check names the place it is made at (Sony's file and line, as its message prints them)
 * and the condition as Sony wrote it.
 */
#ifndef ULT_UTIL_H
#define ULT_UTIL_H

#include <scetypes.h>
#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <ult.h>

extern "C" {
#include <libkernel/libkernel.h>
}

/* what Sony's build inlines wherever it is used */
#define ULT_INLINE	__attribute__((always_inline))

#define ULT_STRING2(x)	#x
#define ULT_STRING(x)	ULT_STRING2(x)
#define ULT_WHERE(file, line)	file ":" ULT_STRING(line) ":"

/* a failed check prints where it was made and what failed, and stops (each file its own copy) */
static inline void ultAssert(const char *where, const char *what, bool cond)
{
	if (!cond) {
		sceClibPrintf("%s[ult] Assertion Failed:\t%s\n", where, what);
		__breakpoint(0);
		for (;;)
			;
	}
}

static inline int ultAssertEqual(const char *where, const char *what, int actual, int expected)
{
	if (actual != expected) {
		sceClibPrintf("%s[ult] Assertion Failed:\t%s:The actual value is %d(%#x), "
			      "but expected value is %d(%#x)\n",
			      where, what, actual, actual, expected, expected);
		__breakpoint(0);
		for (;;)
			;
	}
	return actual;
}

/*
 * Each file's checks: XX_ASSERT(line, cond) and XX_ASSERT_EQUAL(line, actual, expected), the
 * condition's text taken before any macro in it is expanded.
 */
#define ULT_ASSERT_AT(file, line, text, cond) \
	ultAssert(ULT_WHERE(file, line), text, (cond))
#define ULT_ASSERT_EQUAL_AT(file, line, text, actual, expected) \
	ultAssertEqual(ULT_WHERE(file, line), text, (actual), (expected))

/* the build version every object's creation takes is the SDK's, from this one on */
#define ULT_BUILD_VERSION_MIN	0x00910000
/* the version libult's own option parameters carry (its SDK's _SCE_ULT_VERSION) */
#define ULT_VERSION		(SCE_PSP2_SDK_VERSION & 0xffff0000)

/* ult_util.cpp */
/* the objects and their option parameters are 8-byte aligned */
bool isAligned8(const void *p);

/* an option parameter's header: 'libu' 'ltma' 'gic\0' and the version it was initialized for */
#define ULT_OPT_PARAM_MAGIC0	0x6c696275	/* 'libu' */
#define ULT_OPT_PARAM_MAGIC1	0x6c746d61	/* 'ltma' */
#define ULT_OPT_PARAM_MAGIC2	0x67696300	/* 'gic\0' */

struct OptParamHeader {
	uint32_t magic[3];
	uint32_t buildVersion;
};

int initializeOptParamHeader(OptParamHeader *header, uint32_t buildVersion);
int checkOptParamHeader(const OptParamHeader *header);

/* an object's name: SCE_ULT_MAX_NAME_LENGTH characters at most */
int setName(char *dst, const char *name);
int getName(const char *name, char *dst);

/*
 * The threads waiting in a waiting queue, from the address word of its head: their number,
 * and up to max of them described in list (when not NULL); getWaitThreadInfoByCause only
 * those waiting on cause.
 */
uint32_t getWaitThreadInfo(uint32_t head, SceUltWaitThreadInfo *list, uint32_t max);
int32_t getWaitThreadInfoByCause(uint32_t head, SceUltWaitThreadInfo *list, uint32_t max,
				 const void *cause);
/* every entry of a list given for them must have its size set */
int checkWaitThreadInfoList(const SceUltWaitThreadInfo *list, uint32_t max);

#define ULT_UTIL_ASSERT(line, cond) \
	ULT_ASSERT_AT("ult_util.h", line, #cond, cond)

/* a count of users up to a limit; -1 once closed to them */
class BoundedCounter {
public:
	volatile int32_t m_count;
	int32_t m_max;

	ULT_INLINE void initialize(int32_t max)
	{
		m_count = 0;
		m_max = max;
	}

	/* one more user: SCE_ULT_ERROR_STATE once closed or at the limit */
	ULT_INLINE int increment()
	{
		int32_t c;

		do {
			c = __ldrex(&m_count);
			if (c < 0 || (uint32_t)c >= (uint32_t)m_max)
				return SCE_ULT_ERROR_STATE;
		} while (__strex(c + 1, &m_count) != 0);
		return SCE_OK;
	}

	ULT_INLINE void decrement()
	{
		int32_t c;

		do {
			c = __ldrex(&m_count);
			ULT_UTIL_ASSERT(167, c > 0);
		} while (__strex(c - 1, &m_count) != 0);
	}

	/* no more users: SCE_ULT_ERROR_STATE while there are some */
	ULT_INLINE int close()
	{
		int32_t c;

		do {
			c = __ldrex(&m_count);
			if (c > 0)
				return SCE_ULT_ERROR_STATE;
		} while (__strex(-1, &m_count) != 0);
		return SCE_OK;
	}
};

#endif
