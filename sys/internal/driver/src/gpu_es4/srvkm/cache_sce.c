#include "mapped_memory_sce.h"
/* SCE native MMU cache requests. These wrappers have no debug source record.
 * Bit names/values follow pristine IMG sgx_mkif_km.h; upstream stays untouched.
 */
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
