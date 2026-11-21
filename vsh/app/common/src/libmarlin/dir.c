/*
 * Directories: the path separator, a tree removed, a directory's files listed. Its own
 * file: the file layer calls the separator out of line. The file's name is ours.
 */
#include <kernel/iofilemgr.h>
#include "mln_internal.h"

/* the platform's path separator */
signed char MlnImpl_81026430(void)
{
	return '/';
}
