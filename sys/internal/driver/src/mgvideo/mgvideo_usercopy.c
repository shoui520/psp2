/* Descriptive file name: no source-file debug records exist in the original. */
#include <processmgr/processmgr_imports.h>

int copyVideoDataFromUser(void *destination, const void *source, SceSize size)
{
	return SceSysmemForDriver_BC996A7A(destination, source, size);
}

int copyVideoDataToUser(void *destination, const void *source, SceSize size)
{
	return SceSysmemForDriver_6D88EF8A(destination, source, size);
}
