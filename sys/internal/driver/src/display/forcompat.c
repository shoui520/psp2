/*
 * SceDisplay: frame buffers of compatibility applications (PSP emulation,
 * titles that render below the native resolution). Included by display.c.
 */

/* Sony's name (debug record forcompat.c:64). */
int setIftuFrameBufAddr(SceIftuPlaneState *state, SceUInt32 pitch, SceUInt32 width,
			SceUInt32 height, int x, int y, int srcWidth, SceUInt32 srcHeight,
			SceUIntPtr paddr, SceUInt32 flags)
{
	int head = primaryHead;
	SceUInt32 alignedWidth, alignedHeight;
	int shift, format;

	if (flags & 2) {
		shift = 1;
		format = 4;
	} else {
		shift = 2;
		format = 0x10;
	}
	state->sampleX = 0;
	state->sampleY = 0;

	if ((flags & 4) && (head == 1 || hdmiOnly != 0)) {
		DisplayHead *h = &heads[head];
		float scaledWidth = (float)srcWidth * 0.93333333f;
		SceUInt32 limit = (srcHeight * (544 - y)) >> 16;
		float ratioX, ratioY, shrink, fracY, posY, posX, offsetX;
		int vic;

		if (height >= limit)
			height = limit;
		vic = h->vic;
		switch (vic) {
		case 0x8100:
		case 0x8300:
			ratioY = 0.9075f;
			ratioX = 0.75f;
			break;
		case 0x8600:
		case 0x8900:
			ratioY = 1.3333334f;
			ratioX = ratioY;
			break;
		default:
#if FW_SINCE(0x04000000)
			SCE_DBG_PRINTF(6, 64, "unsupported display mode: %x\n", vic);
#endif
			ratioY = 1.0f;
			ratioX = ratioY;
			break;
		}

		if (h->plane[0].scaleFlags & 1) {
			float scale = h->plane[0].scale;
			float rest = 0.99264705f - scale;

			if (rest < 0.0f) {
				ratioY *= scale;
				shrink = 1.0f - scale * 1.0714285f;
				offsetX = ratioX * scale;
				goto plain;
			}
			ratioY *= scale;
			shrink = 1.0f - scale * 1.0714285f;
			offsetX = ratioX * scale;
			posY = ((float)y + (float)(272 - y) * rest) * ratioX - 2.0f;
		} else {
			shrink = -0.071428537f;
			offsetX = ratioX;
plain:
			posY = (float)y * ratioX;
		}

		posX = ((float)x + (float)(480 - x) * shrink) * ratioX;
		y = (int)posY + 1;
		if (y < 0) {
			y = 0;
			fracY = 0.0f;
		} else {
			fracY = ((float)y - posY) * 0.00390625f;
		}
		x = (int)posX + 1;
		srcWidth = (int)(scaledWidth / offsetX);
		srcHeight = (int)((float)srcHeight / ratioY);
		state->sampleX = (int)(((float)x - posX) * 0.00390625f * (float)srcWidth);
		state->sampleY = (int)((float)(int)srcHeight * fracY);
	}

	alignedWidth = (width + 15) & ~15;
	alignedHeight = (height + 7) & ~7;
	state->fb.pixelformat = paddr == 0 ? 0 : format;
	state->fb.paddr0 = paddr;
	state->fb.leftover_stride = (pitch - alignedWidth) << shift;
	state->fb.width = alignedWidth;
	state->hright_padding = alignedWidth - width;
	state->fb.height = alignedHeight;
	state->vtop_padding = 0;
	state->vbot_padding = alignedHeight - height;
	state->hleft_padding = 0;
	state->sourceConfig[0] = 0;
	state->fb.leftover_align = 0;
	state->fb.paddr1 = 0;
	state->fb.paddr2 = 0;
	state->sourceConfig[1] = 0;
	state->sourceConfig[2] = 0;
	state->dst_x = 0;
	state->dst_y = 0;
	state->scaleX = 0x10000;
	state->scaleY = 0x10000;
	if (height != 0 && width != 0) {
		if ((SceUInt32)(x + 15) < 15) {
			state->hleft_padding = -x;
			x = 0;
		}
		if ((SceUInt32)(y + 15) < 15) {
			state->vtop_padding = -y;
			y = 0;
		}
		state->scaleX = srcWidth;
		state->scaleY = srcHeight;
		state->dst_x = x;
		state->dst_y = y;
	}
	return 0;
}

static int procStart(SceUID pid, int event, ProcEventParam *param, void *common) /* name unknown */
{
	DisplayProcessLocal *pls;

	if (event == 0x10000) {
		pls = NULL;
		sceKernelGetRemoteKPLS(pid, plsKey, (void **)&pls, 0);
		if (pls != NULL) {
			pls->vcountBase[0] = heads[0].vcount + pls->vcountBase[0] - pls->vcountStopped[0];
			pls->vcountBase[1] = heads[1].vcount + pls->vcountBase[1] - pls->vcountStopped[1];
			pls->vcountBase[2] = heads[2].vcount + pls->vcountBase[2] - pls->vcountStopped[2];
		}
	}
	return 0;
}

static int procStop(SceUID pid, int event, ProcEventParam *param, void *common) /* name unknown */
{
	DisplayProcessLocal *pls;

	if (event == 0x800) {
		pls = NULL;
		sceKernelGetRemoteKPLS(pid, plsKey, (void **)&pls, 0);
		if (pls != NULL) {
			pls->vcountStopped[0] = heads[0].vcount;
			pls->vcountStopped[1] = heads[1].vcount;
			pls->vcountStopped[2] = heads[2].vcount;
		}
	}
	return 0;
}

/* Titles allowed a 1280x725 frame buffer on the main panel. */
static int isHighResolutionTitle(void) /* name unknown */
{
	char titleId[32];

	if (SceSysrootForKernel_EC3124A3(0, titleId, sizeof(titleId)) < 0)
		return 0;
	if (strncmp(titleId, "PCSG80001", sizeof(titleId)) == 0
	    || strncmp(titleId, "PCSG80007", sizeof(titleId)) == 0
	    || strncmp(titleId, "PCSG00318", sizeof(titleId)) == 0
	    || strncmp(titleId, "PCSG00319", sizeof(titleId)) == 0
	    || strncmp(titleId, "PCSG00320", sizeof(titleId)) == 0
	    || strncmp(titleId, "PCSG00321", sizeof(titleId)) == 0)
		return 1;
	return strncmp(titleId, "PCSH00059", sizeof(titleId)) == 0;
}
