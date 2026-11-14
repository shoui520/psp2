/*
 * assert.h: the SDK's assertion macro, with Sony's psp2 definitions (SDK Release 03.570.011,
 * target/include/assert.h): a failed assertion prints its location and test and aborts, or,
 * with SCE_LIBC_ASSERT_BREAK, stops at a breakpoint.
 */
#undef assert

#ifdef NDEBUG
#define assert(test)	((void)0)
#else
#define _STRIZE(x)	_VAL(x)
#define _VAL(x)		#x
#ifdef SCE_LIBC_ASSERT_BREAK
#define assert(test)	((test) ? (void)0 \
	: _SCE_Assert(__FILE__ "(" _STRIZE(__LINE__) ") : Assertion Failed (" #test ")", __func__))
#else
#define assert(test)	((test) ? (void)0 \
	: _Assert(__FILE__ "(" _STRIZE(__LINE__) ") : Assertion Failed (" #test ")", __func__))
#endif
#endif

#ifndef _ASSERT_DECL
#define _ASSERT_DECL
#ifdef __cplusplus
extern "C" {
#endif
void _Assert(const char *mesg, const char *fun);
void _SCE_Assert(const char *mesg, const char *fun);
#ifdef __cplusplus
}
#endif
#endif /* _ASSERT_DECL */
