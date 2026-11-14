// newreplace.h: the program's replacement of operator new and operator delete. The program's
// crt0 hands its user_new ... user_delete_array functions to the library (__sce_libcnewreplace,
// in its SceLibcParam); _InitNewReplace takes them, or the library's own when the program has
// none, and every operator calls through the pointer it set. The names are ours.
#ifndef _NEWREPLACE_H
#define _NEWREPLACE_H
#include <new>

struct SceLibcNewReplace
	{	// __sce_libcnewreplace
	_CSTD size_t size;
	unsigned int version;
	void *(*user_new)(_CSTD size_t);
	void *(*user_new_nothrow)(_CSTD size_t, const _STD nothrow_t&);
	void *(*user_new_array)(_CSTD size_t);
	void *(*user_new_array_nothrow)(_CSTD size_t, const _STD nothrow_t&);
	void (*user_delete)(void *);
	void (*user_delete_nothrow)(void *, const _STD nothrow_t&);
	void (*user_delete_array)(void *);
	void (*user_delete_array_nothrow)(void *, const _STD nothrow_t&);
	};

		// the operators' functions (newrep.cpp)
extern void *(*_User_new)(_CSTD size_t);
extern void *(*_User_new_nothrow)(_CSTD size_t, const _STD nothrow_t&);
extern void *(*_User_new_array)(_CSTD size_t);
extern void *(*_User_new_array_nothrow)(_CSTD size_t, const _STD nothrow_t&);
extern void (*_User_delete)(void *);
extern void (*_User_delete_nothrow)(void *, const _STD nothrow_t&);
extern void (*_User_delete_array)(void *);
extern void (*_User_delete_array_nothrow)(void *, const _STD nothrow_t&);

		// the library's own, in the operators' files
void *_Default_new(_CSTD size_t);
void *_Default_new_nothrow(_CSTD size_t, const _STD nothrow_t&) _THROW0();
void *_Default_new_array(_CSTD size_t);
void *_Default_new_array_nothrow(_CSTD size_t, const _STD nothrow_t&) _THROW0();
void _Default_delete(void *) _THROW0();
void _Default_delete_nothrow(void *, const _STD nothrow_t&) _THROW0();
void _Default_delete_array(void *) _THROW0();
void _Default_delete_array_nothrow(void *, const _STD nothrow_t&) _THROW0();

		// the new handler's cell
_STD new_handler *_New_handler_address();

extern "C" int _InitNewReplace(const SceLibcNewReplace *);
#endif /* _NEWREPLACE_H */
