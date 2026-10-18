/* SceEnumWakeUp: The MTP responder thread, start and initialisation. */
#include "ew_internal.h"

/* answer the host's requests until stopped */
int mtpThread(SceSize args, void *argp)
{
	MtpContext *ctx = *(MtpContext **)argp;
	const void *port = ctx->port;
	int err = 0, tries;
	SceUInt16 op;

	(void)args;

	for (;;) {
		if ((ctx->flags & MTP_CTX_STOPPED) || port == 0)
			return 0;
		if (SceMtpIfForDriver_2D9318E1(port) == 0) {
			sceKernelLockMutex(ctx->mutex, 1, NULL);
			ctx->state = MTP_STATE_DISCONNECTED;
			ctx->stateParam = -1;
			sceKernelUnlockMutex(ctx->mutex, 1);
			if (ctx->sessionId) {
				mtpCloseSession(ctx);
				SceMtpIfForDriver_92CD107E(port);
			}
			SceMtpIfForDriver_05408B5B(port, 1000000);
			continue;
		}
		if (err < 0) {
			if (err == (int)0x80543601 || err == (int)0x80543609)
				SceMtpIfForDriver_92CD107E(port);
			err = 0;
			continue;
		}

		SceMtpIfForDriver_8D16B997(port, 0);
		err = SceMtpIfForDriver_117F7519(port, &ctx->request);
		if (err < 0) {
			if (err == (int)0x8054000d || err == (int)0x8054360a)
				err = 0;
			continue;
		}
		SceMtpIfForDriver_8D16B997(port, 1);
		sceKernelLockMutex(ctx->mutex, 1, NULL);
		ctx->state = MTP_STATE_BUSY;
		ctx->stateParam = -1;
		sceKernelUnlockMutex(ctx->mutex, 1);
		sceKernelWaitSema(ctx->sema, 1, NULL);
		mtpSetResponse(PTP_RC_OK, NULL, 0, &ctx->request, &ctx->response);

		op = ctx->request.code;
		if (op == PTP_OC_OPEN_SESSION) {
			err = mtpOpenSession(ctx);
		} else if (op == PTP_OC_CLOSE_SESSION) {
			err = mtpCloseSession(ctx);
		} else if (op == PTP_OC_GET_DEVICE_INFO) {
			err = mtpGetDeviceInfo(ctx);
		} else if (ctx->sessionId == 0) {
			mtpSetResponse(PTP_RC_SESSION_NOT_OPEN, NULL, 0, &ctx->request, &ctx->response);
			goto respond;
		} else {
			if (op != 0x100d) {
				ctx->sendObject[0] = 0;
				ctx->sendObject[1] = 0;
				if (op == PTP_OC_GET_DEVICE_PROP_DESC) {
					err = mtpGetDevicePropDesc(ctx);
					goto done;
				}
			}
			mtpSetResponse(PTP_RC_OPERATION_NOT_SUPPORTED, NULL, 0, &ctx->request,
				       &ctx->response);
			goto respond;
		}
done:
		if (err < 0) {
			sceKernelSignalSema(ctx->sema, 1);
			continue;
		}
respond:
		sceKernelSignalSema(ctx->sema, 1);
		SceMtpIfForDriver_8D16B997(port, 8);
		for (tries = 4; tries; tries--) {
			err = SceMtpIfForDriver_5766D257(port, &ctx->response);
			if (err != (int)0x80543605)
				break;
		}
	}
}

int mtpStart(int portId, MtpDeviceStringsProvider provider)
{
	MtpContext *ctx = NULL;
	int ret;

	if (provider == NULL)
		return MTP_ERROR_INVALID_ARGUMENT;
	mtpSetupDeviceStrings(provider);
	if (!(ewg.mtp.flags & MTP_CTX_STOPPED))
		return MTP_ERROR_NOT_STOPPED;
	ctx = &ewg.mtp;
	mtpCacheLoad(mtpIconPath(), 0x10000);
	ewg.mtp.port = SceMtpIfForDriver_16D7DBCC(portId);
	if (ewg.mtp.port == 0 || (ret = SceMtpIfForDriver_5245D237(ewg.mtp.port)) < 0) {
		ret = MTP_ERROR_NOT_CONNECTED;
	} else {
		ewg.mtp.flags &= ~MTP_CTX_STOPPED;
		sceKernelLockMutex(ewg.mtp.mutex, 1, NULL);
		ewg.mtp.state = MTP_STATE_STARTED;
		ewg.mtp.stateParam = -1;
		sceKernelUnlockMutex(ewg.mtp.mutex, 1);
		if (ewg.mtp.thread > 0 && sceKernelStartThread(ewg.mtp.thread, sizeof(ctx), &ctx) >= 0)
			return ret;
		ret = MTP_ERROR_FAILED;
	}
	mtpClearDeviceStrings();
	return ret;
}

int mtpInit(void)
{
	SceUID mutex;
	int ret;

	memset(&ewg.mtp, 0, sizeof(ewg.mtp));
	ewg.mtp.flags = MTP_CTX_STOPPED;
	mutex = sceKernelCreateMutex("SceMtpMutex", 2, 0, NULL);
	if (mutex < 0)
		return mutex;
	ewg.mtp.mutex = mutex;
	sceKernelLockMutex(mutex, 1, NULL);
	ewg.mtp.state = MTP_STATE_NONE;
	ewg.mtp.stateParam = -1;
	sceKernelUnlockMutex(ewg.mtp.mutex, 1);
	ewg.mtp.sema = sceKernelCreateSema("SceMtpSema", 0, 1, 1, NULL);
	if (ewg.mtp.sema < 1) {
		ret = MTP_ERROR_FAILED;
	} else {
		ewg.mtp.thread = sceKernelCreateThread("SceMtpThread", mtpThread, 0x10000102, 0x8000,
						    0x20000000, 0x80000, NULL);
		if (ewg.mtp.thread < 0) {
			ret = MTP_ERROR_FAILED;
		} else {
			ret = mtpHeapCreate(0x40000);
			if (ret < 0) {
				ret = MTP_ERROR_FAILED;
			} else {
				ret = mtpXferInit(&ewg.mtp, &ewg.mtp.xfer);
				if (ret >= 0)
					return mutex;
				mtpHeapDelete();
			}
			sceKernelDeleteThread(ewg.mtp.thread);
		}
		sceKernelDeleteSema(ewg.mtp.sema);
	}
	sceKernelDeleteMutex(ewg.mtp.mutex);
	return ret;
}
