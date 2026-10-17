/* ES3 bus error collection in the secure world. */

#include "buserror.h"
#include "../buserror_es3.h"

static BusErrorRecord *busErrorRecord(void)
{
	return (BusErrorRecord *)((char *)SceSysrootForTZS_1C96096F() + BUSERROR_SHM_RECORD);
}

/* name unknown: collect the record (and print it in debug builds) */
int MoncallCollectBusErrorInfo_ES3(int arg0, int arg1, int arg2, int arg3)
{
	BusErrorRecord *rec = busErrorRecord();

	CollectBusErrorInfo_ES3(rec);
#if FW_DEBUG
	FormatBusErrorInfo_ES3(NULL, rec);
#endif
	return 0;
}

/* name unknown: the bus error interrupt; the error is fatal */
static int busErrorIntrHandler(int intrCode, void *userCtx)
{
	MoncallCollectBusErrorInfo_ES3(0, 0, 0, 0);
	for (;;)
		;
	return 0;
}

/* name unknown: collect the record and write it back for the kernel */
int MoncallCollectCleanBusErrorInfo_ES3(int arg0, int arg1, int arg2, int arg3)
{
	BusErrorRecord *rec = busErrorRecord();

	CollectBusErrorInfo_ES3(rec);
	SceCpuForTZS_7A5373EB(rec, BUSERROR_SHM_RECORD_SIZE);
	return 0;
}

/* name unknown */
int MoncallNop_ES3(int arg0, int arg1, int arg2, int arg3)
{
	return 0;
}

int RegisterBusErrorHandler_ES3(void)
{
	int ret;

	ret = sceKernelRegisterIntrHandler(0x21, "SceBusErrorSecure", 0, busErrorIntrHandler, NULL,
					   0x10, 0xf, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(6, 1703, "register default bus error handler failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1704);
	} else {
		sceKernelEnableIntr(0x21);
		ret = 0;
	}
	return ret;
}
