/* SceEnumWakeUp: The MTP data transfer thread. */
#include "ew_internal.h"

/* data phase helper: sends (event 1) or receives (event 2) for the responder */
int mtpXferThread(SceSize args, void *argp)
{
	MtpXfer *x = *(MtpXfer **)argp;
	MtpContext *ctx;
	SceUInt32 bits;

	(void)args;

	if (x == NULL)
		return -1;
	ctx = x->ctx;
	for (;;) {
		if (ksceKernelWaitEventFlag(x->evf, 0x8003,
					    SCE_KERNEL_EVF_WAITMODE_OR |
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					    &bits, NULL) < 0)
			return -1;
		if (bits & 1) {
			x->result = SceMtpIfForDriver_EDC3AB26(ctx->port, x->buf, x->size, x->total);
			ksceKernelSetEventFlag(x->evf, 0x10);
		} else if (bits & 2) {
			x->result = SceMtpIfForDriver_F0FA2D86(ctx->port, x->buf, x->size);
			ksceKernelSetEventFlag(x->evf, 0x20);
		}
	}
}

int mtpXferInit(MtpContext *ctx, MtpXfer **out)
{
	MtpXfer *x;
	int ret;

	x = mtpAlloc(sizeof(*x));
	if (x == NULL) {
		ret = MTP_ERROR_NO_MEMORY;
	} else {
		x->evf = ksceKernelCreateEventFlag("SceMtpXferEventFlag", SCE_KERNEL_EVF_ATTR_MULTI,
						   0, NULL);
		if (x->evf > 0) {
			x->thread = ksceKernelCreateThread("SceMtpXferThread", mtpXferThread,
							   0x10000103, 0x400, 0x20000000, 0x80000,
							   NULL);
			if (x->thread >= 0) {
				x->ctx = ctx;
				*out = x;
				if (ksceKernelStartThread(x->thread, sizeof(x), &x) >= 0)
					return 0;
				ksceKernelDeleteThread(x->thread);
			}
			ksceKernelDeleteEventFlag(x->evf);
		}
		ret = MTP_ERROR_FAILED;
		mtpFree(x);
	}
	*out = NULL;
	return ret;
}
