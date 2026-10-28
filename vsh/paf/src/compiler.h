/* C11 size assertions for the C99 compiler shipped with the PSP2 SDK. */
#ifndef PAF_COMPILER_H
#define PAF_COMPILER_H

#if defined(__SNC__) && !defined(__cplusplus)
#define PAF_ASSERT_NAME_(line) pafSizeCheck##line
#define PAF_ASSERT_NAME(line) PAF_ASSERT_NAME_(line)
#define _Static_assert(condition, message) \
	typedef char PAF_ASSERT_NAME(__LINE__)[(condition) ? 1 : -1]
#endif

#endif
