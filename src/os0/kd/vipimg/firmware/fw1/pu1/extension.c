/*
 * PU1: the frame stores' extensions. The SEI messages and VUI of the picture
 * being decoded are kept in g_frameExtension, the staging copy that
 * g_frameInfo.extension points at, until the picture goes to its frame store; a
 * frame's two fields may each keep a picture timing and a pan-scan rectangle.
 */
#include "codec.h"

/* Forget the SEI messages and VUI kept. */
void clearExtension(VipFrameExtension *extension)
{
	uint32_t i;

	extension->bufferingPeriodCount = 0;
	extension->pictureTimingCount = 0;
	extension->recoveryPointCount = 0;
	extension->panScanCount = 0;
	extension->vuiPresent = 0;
	for (i = 0; i < VIP_BUFFERING_PERIOD_WORDS; i++)
		extension->bufferingPeriod[i] = 0;
	for (i = 0; i < VIP_EXTENSION_COPIES * VIP_PICTURE_TIMING_WORDS; i++)
		extension->pictureTiming[0][i] = 0;
	for (i = 0; i < VIP_RECOVERY_POINT_WORDS; i++)
		extension->recoveryPoint[i] = 0;
	for (i = 0; i < VIP_EXTENSION_COPIES * VIP_PAN_SCAN_WORDS; i++)
		extension->panScan[0][i] = 0;
	for (i = 0; i < VIP_VUI_WORDS; i++)
		extension->vui[i] = 0;
	for (i = 0; i < VIP_HRD_WORDS; i++) {
		extension->nalHrd[i] = 0;
		extension->vclHrd[i] = 0;
	}
}

/* Keep the picture timing SEI in g_parsed, the picture's first. */
void keepPictureTiming(void)
{
	VipFrameExtension *extension = g_frameInfo.extension;
	uint32_t count = extension->pictureTimingCount, i;

	if (count >= VIP_EXTENSION_COPIES)
		return;
	for (i = 0; i < VIP_PICTURE_TIMING_WORDS; i++)
		extension->pictureTiming[count][i] = g_parsed[i];
	extension->pictureTimingCount++;
	extension->pictureTimingCount |= VIP_EXTENSION_TAKEN;
}

/* the next picture (a frame's second field) may keep its own */
void nextPictureTiming(void)
{
	VipFrameExtension *extension = g_frameInfo.extension;

	if (extension->pictureTimingCount & VIP_EXTENSION_TAKEN)
		extension->pictureTimingCount &= ~VIP_EXTENSION_TAKEN;
}

/* Keep the pan-scan rectangle SEI in g_parsed, the picture's first. */
void keepPanScan(void)
{
	VipFrameExtension *extension = g_frameInfo.extension;
	uint32_t count = extension->panScanCount, i;

	if (count >= VIP_EXTENSION_COPIES)
		return;
	for (i = 0; i < VIP_PAN_SCAN_WORDS; i++)
		extension->panScan[count][i] = g_parsed[i];
	extension->panScanCount++;
	extension->panScanCount |= VIP_EXTENSION_TAKEN;
}

void nextPanScan(void)
{
	VipFrameExtension *extension = g_frameInfo.extension;

	if (extension->panScanCount & VIP_EXTENSION_TAKEN)
		extension->panScanCount &= ~VIP_EXTENSION_TAKEN;
}

/* Keep the buffering period SEI in g_parsed, the frame's first. */
void keepBufferingPeriod(void)
{
	VipFrameExtension *extension = g_frameInfo.extension;
	uint32_t i;

	if (extension->bufferingPeriodCount)
		return;
	extension->bufferingPeriodCount = 1;
	for (i = 0; i < VIP_BUFFERING_PERIOD_WORDS; i++)
		extension->bufferingPeriod[i] = g_parsed[i];
}

/* Keep the recovery point SEI in g_parsed, the frame's first. */
void keepRecoveryPoint(void)
{
	VipFrameExtension *extension = g_frameInfo.extension;
	uint32_t i;

	if (extension->recoveryPointCount)
		return;
	extension->recoveryPointCount = 1;
	for (i = 0; i < VIP_RECOVERY_POINT_WORDS; i++)
		extension->recoveryPoint[i] = g_parsed[i];
}

/* Keep the SPS's VUI with the picture. */
void copyVui(VipFrameExtension *extension, const VipSps *sps)
{
	uint32_t i;

	extension->vuiPresent = sps->vuiPresent;
	for (i = 0; i < VIP_VUI_WORDS; i++)
		extension->vui[i] = sps->vui[i];
	for (i = 0; i < VIP_HRD_WORDS; i++) {
		extension->nalHrd[i] = sps->nalHrd[i];
		extension->vclHrd[i] = sps->vclHrd[i];
	}
}

/* the frame store's extension, as frame.c's accessors: -1 without one */
int setFrameBufferingPeriodCount(VipFrameHandle *handle, uint8_t count)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->bufferingPeriodCount = count;
	return 0;
}

int setFramePictureTimingCount(VipFrameHandle *handle, uint8_t count)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->pictureTimingCount = count;
	return 0;
}

int setFrameRecoveryPointCount(VipFrameHandle *handle, uint8_t count)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->recoveryPointCount = count;
	return 0;
}

int setFramePanScanCount(VipFrameHandle *handle, uint8_t count)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->panScanCount = count;
	return 0;
}

int setFrameVuiPresent(VipFrameHandle *handle, uint8_t present)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	extension->vuiPresent = present;
	return 0;
}

int setFrameBufferingPeriod(VipFrameHandle *handle, const uint32_t *words)
{
	VipFrameExtension *extension = frameExtension(handle);
	uint32_t i;

	if (!extension || !words)
		return -1;
	for (i = 0; i < VIP_BUFFERING_PERIOD_WORDS; i++)
		extension->bufferingPeriod[i] = words[i];
	return 0;
}

int setFramePictureTiming(VipFrameHandle *handle, const uint32_t *words)
{
	VipFrameExtension *extension = frameExtension(handle);
	uint32_t i;

	if (!extension || !words)
		return -1;
	for (i = 0; i < VIP_EXTENSION_COPIES * VIP_PICTURE_TIMING_WORDS; i++)
		extension->pictureTiming[0][i] = words[i];
	return 0;
}

int setFrameRecoveryPoint(VipFrameHandle *handle, const uint32_t *words)
{
	VipFrameExtension *extension = frameExtension(handle);
	uint32_t i;

	if (!extension || !words)
		return -1;
	for (i = 0; i < VIP_RECOVERY_POINT_WORDS; i++)
		extension->recoveryPoint[i] = words[i];
	return 0;
}

int setFramePanScan(VipFrameHandle *handle, const uint32_t *words)
{
	VipFrameExtension *extension = frameExtension(handle);
	uint32_t i;

	if (!extension || !words)
		return -1;
	for (i = 0; i < VIP_EXTENSION_COPIES * VIP_PAN_SCAN_WORDS; i++)
		extension->panScan[0][i] = words[i];
	return 0;
}

/* words: the VUI's, then the NAL HRD's and the VCL HRD's, as VipFrameExtension has them */
int setFrameVui(VipFrameHandle *handle, const uint32_t *words)
{
	VipFrameExtension *extension = frameExtension(handle);
	uint32_t i;

	if (!extension || !words)
		return -1;
	for (i = 0; i < VIP_VUI_WORDS; i++)
		extension->vui[i] = words[i];
	for (i = 0; i < VIP_HRD_WORDS; i++) {
		extension->nalHrd[i] = words[VIP_VUI_WORDS + i];
		extension->vclHrd[i] = words[VIP_VUI_WORDS + VIP_HRD_WORDS + i];
	}
	return 0;
}

/* Whether the picture is a frame, a frame's first field or its second. */
int setFrameFieldState(VipFrameHandle *handle)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension)
		return -1;
	if (!g_sliceHeader.fieldPic)
		extension->fieldState = VIP_EXTENSION_FRAME;
	else if (g_secondFieldPending)
		extension->fieldState = VIP_EXTENSION_FIELD_PAIR;
	else
		extension->fieldState = VIP_EXTENSION_FIRST_FIELD;
	return 0;
}

/*
 * The picture's extension goes to its frame store: the SEI messages kept, the
 * VUI, whether it is a field, and what publishPictureInfo gave the area of the
 * picture (profile and level, the rate kind, the chroma sample locations and the
 * colour matrix). After a frame or a second field the staging copy starts over.
 */
void publishExtension(VipFrameHandle *frame, VipDecoderArea *area,
	VipFrameExtension *extension)
{
	copyVui(extension, g_currentSps);
	nextPictureTiming();
	nextPanScan();
	setFrameFieldState(frame);
	setFrameProfile(frame, area->picture.key >> 16);
	setFrameLevel(frame, area->picture.key & 0xffff);
	setFrameRateKind(frame, area->picture.aspectRate >> 8 & 0xff);
	setFrameChromaLocTop(frame,
		area->picture.progressive >> VIP_PICTURE_CHROMA_TOP_SHIFT & 0xff);
	setFrameChromaLocBottom(frame,
		area->picture.progressive >> VIP_PICTURE_CHROMA_BOTTOM_SHIFT & 0xff);
	setFrameMatrix(frame, area->picture.matrix & 0xff);
	setFrameBufferingPeriodCount(frame, extension->bufferingPeriodCount);
	setFramePictureTimingCount(frame, extension->pictureTimingCount);
	setFrameRecoveryPointCount(frame, extension->recoveryPointCount);
	setFramePanScanCount(frame, extension->panScanCount);
	setFrameVuiPresent(frame, extension->vuiPresent);
	setFrameBufferingPeriod(frame, extension->bufferingPeriod);
	setFramePictureTiming(frame, extension->pictureTiming[0]);
	setFrameRecoveryPoint(frame, extension->recoveryPoint);
	setFramePanScan(frame, extension->panScan[0]);
	setFrameVui(frame, extension->vui);
	if (!g_sliceHeader.fieldPic || g_secondFieldPending)
		clearExtension(&g_frameExtension);
}

/* A field shown without its pair has no field state. */
void clearUnpairedField(VipFrameHandle *handle)
{
	VipFrameExtension *extension = frameExtension(handle);

	if (!extension || !g_extensionState)
		return;
	if (!(extension->fieldState & VIP_EXTENSION_COMPLETE))
		extension->fieldState = 0;
}
