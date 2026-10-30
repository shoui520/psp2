/* Size assertions for SNC, whose C99 and C++ front ends have no _Static_assert. */
#ifndef PAF_COMPILER_H
#define PAF_COMPILER_H

#if defined(__SNC__)
#define PAF_ASSERT_NAME_(line) pafSizeCheck##line
#define PAF_ASSERT_NAME(line) PAF_ASSERT_NAME_(line)
#define _Static_assert(condition, message) \
	typedef char PAF_ASSERT_NAME(__LINE__)[(condition) ? 1 : -1]
#endif

#endif
