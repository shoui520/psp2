/*
 * SKBL: the build numbers and the build's revision and date strings, which
 * boot() and sceKblMain print. (File name ours: no debug record names it.)
 */

#include <fw.h>

#if FW_SINCE(0x04000000)
const char bootRevision[] = "r96726";
const char bootBuildDate[] = "2017-02-23 19:47:57+0900";
#elif FW_SINCE(0x03650000)
const char bootRevision[] = "r96745";
const char bootBuildDate[] = "2017-03-17 06:22:36+0900";
#else
const char bootRevision[] = "r96393";
const char bootBuildDate[] = "2016-10-24 06:28:25+0900";
#endif

/* name ours: the build number boot() prints */
int KblGetBuildNumber(void)
{
	return 300;
}

#if FW_DEBUG
/* name ours: the build number sceKblMain prints */
int KblMainGetBuildNumber(void)
{
	return 284;
}
#endif
