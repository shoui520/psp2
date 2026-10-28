/* These entry points are fixed in the shipped library. */
#define printf_enabled ScePafCommon_7DD63E0E
#define on_error ScePafCommon_6A687A77
#define CommonDebugBreak ScePafCommon_E19DFE47

int printf_enabled(void)
{
	return 1;
}

void on_error(void)
{
	__breakpoint(0);
}

void CommonDebugBreak(void)
{
	__breakpoint(0);
}
