/*
 * __dwrite: the library's writes go to the application's __write.
 */

#include <yfuns.h>

size_t __dwrite(int handle, const unsigned char *buffer, size_t size)
{
	return __write(handle, buffer, size);
}
