/*
 * __close, remove: there are no files to close or remove.
 */

#include <stdio.h>
#include <yfuns.h>

int __close(int handle)
{
	return 0;
}

int remove(const char *name)
{
	return 0;
}
