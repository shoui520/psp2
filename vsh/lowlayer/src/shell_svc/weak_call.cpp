/*
 * Two wrappers nothing refers to, each passing its arguments and a mode (0 or 1) to a weak
 * function the library is linked without, so each returns 9. Sony linked this object last into
 * the system software's modules (libshellsvc, the common dialogs' plugins, safemode, the
 * applications), after their own code, and psp2ld kept it there unreferenced. Its folder, its
 * file and its names are ours.
 */
#include <stddef.h>

extern "C" int shellSvcWeakHandler(int arg0, int arg1, int arg2, int mode) __attribute__((weak));

int shellSvcWeakCall0(int arg0, int arg1, int arg2)
{
	if (shellSvcWeakHandler != NULL)
		return shellSvcWeakHandler(arg0, arg1, arg2, 0);
	return 9;
}

int shellSvcWeakCall1(int arg0, int arg1, int arg2)
{
	if (shellSvcWeakHandler != NULL)
		return shellSvcWeakHandler(arg0, arg1, arg2, 1);
	return 9;
}
