// newrep.cpp: _InitNewReplace, the operators' functions: the program's replacements when it
// gives all eight, else the library's own
#include <stdio.h>
#include "newreplace.h"

extern "C" int sceClibPrintf(const char *, ...);

void *(*_User_new)(_CSTD size_t);
void *(*_User_new_nothrow)(_CSTD size_t, const _STD nothrow_t&);
void *(*_User_new_array)(_CSTD size_t);
void *(*_User_new_array_nothrow)(_CSTD size_t, const _STD nothrow_t&);
void (*_User_delete)(void *);
void (*_User_delete_nothrow)(void *, const _STD nothrow_t&);
void (*_User_delete_array)(void *);
void (*_User_delete_array_nothrow)(void *, const _STD nothrow_t&);

extern "C" int _InitNewReplace(const SceLibcNewReplace *replace)
	{	// take the program's replacement, or the library's functions
	int res = 0;

	if (replace != 0 && (replace->user_new != 0
		|| replace->user_new_nothrow != 0
		|| replace->user_new_array != 0
		|| replace->user_new_array_nothrow != 0
		|| replace->user_delete != 0
		|| replace->user_delete_nothrow != 0
		|| replace->user_delete_array != 0
		|| replace->user_delete_array_nothrow != 0))
		{	// the program replaces them: all eight, or none
		if (replace->user_new != 0
			&& replace->user_new_nothrow != 0
			&& replace->user_new_array != 0
			&& replace->user_new_array_nothrow != 0
			&& replace->user_delete != 0
			&& replace->user_delete_nothrow != 0
			&& replace->user_delete_array != 0
			&& replace->user_delete_array_nothrow != 0)
			{	// a complete set
			_User_new = replace->user_new;
			_User_new_nothrow = replace->user_new_nothrow;
			_User_new_array = replace->user_new_array;
			_User_new_array_nothrow = replace->user_new_array_nothrow;
			_User_delete = replace->user_delete;
			_User_delete_nothrow = replace->user_delete_nothrow;
			_User_delete_array = replace->user_delete_array;
			_User_delete_array_nothrow = replace->user_delete_array_nothrow;
			}
		else
			{	// an incomplete set
			sceClibPrintf("SceLibc new replace failed.\n");
			res = -1;
			}
		}
	else
		{	// the library's own
		_User_new = _Default_new;
		_User_new_nothrow = _Default_new_nothrow;
		_User_new_array = _Default_new_array;
		_User_new_array_nothrow = _Default_new_array_nothrow;
		_User_delete = _Default_delete;
		_User_delete_nothrow = _Default_delete_nothrow;
		_User_delete_array = _Default_delete_array;
		_User_delete_array_nothrow = _Default_delete_array_nothrow;
		}
	return (res);
	}
