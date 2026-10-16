/* SCE native MMU cache requests. These wrappers have no debug source record.
 * Bit names/values follow pristine IMG sgx_mkif_km.h; upstream stays untouched.
 */
#define SGXMKIF_CC_INVAL_BIF_PT 0x1U
#define SGXMKIF_CC_INVAL_BIF_PD 0x2U
#define SGXMKIF_CC_INVAL_BIF_SL 0x4U

void SetGpuCacheControl(unsigned flags);

void RequestGpuSystemCacheInvalidate(void)
{
	SetGpuCacheControl(SGXMKIF_CC_INVAL_BIF_SL);
}

void RequestGpuPageTableCacheInvalidate(void)
{
	SetGpuCacheControl(SGXMKIF_CC_INVAL_BIF_PT);
	RequestGpuSystemCacheInvalidate();
}

void RequestGpuPageDirectoryCacheInvalidate(void)
{
	SetGpuCacheControl(SGXMKIF_CC_INVAL_BIF_PD);
	RequestGpuSystemCacheInvalidate();
}
