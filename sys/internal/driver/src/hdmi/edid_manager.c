/*
 * SceHdmi EDID manager (Sony's edid_manager.c): reads the sink's EDID and
 * parses what SceHdmi uses from it into SceHdmiEdidInfo.
 */
#include <scetypes.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "hdmi_internal.h"
#include <sysmem/sblaimgr.h>
#include <sysmem/sysclib.h>

#define EDID_BLOCK_SIZE			128
#define EDID_DESCRIPTOR_COUNT		4
#define EDID_DESCRIPTOR_SIZE		18
#define EDID_TAG_MONITOR_NAME		0xfc
#define EDID_TAG_RANGE_LIMITS		0xfd

#define CEA_TAG_VIDEO			2
#define CEA_TAG_VENDOR			3
#define CEA_TAG_EXTENDED		7

static inline int _getEdidManager(void)
{
	if (_sceHdmiLockMutex() != 0) {
#if FW_SINCE(0x04000000)
		SCE_DBG_ASSERT_PRINTF(0, 49, "err _sceHdmiLockMutex()\n");
#endif
		return 7;
	}
	return 0;
}

static inline void _releaseEdidManager(void)
{
#if FW_SINCE(0x04000000)
	if (_sceHdmiUnlockMutex() != 0)
		SCE_DBG_ASSERT_PRINTF(0, 60, "err _sceHdmiUnlockMutex()\n");
#else
	_sceHdmiUnlockMutex();
#endif
}

/* A range limits descriptor: the CEA formats whose line rates it allows. */
static inline void parseRangeLimits(SceHdmiEdidInfo *info, const SceUInt8 *d)
{
	/* d[5], d[6]: vertical Hz, d[7], d[8]: horizontal kHz, d[9]: pixel clock / 10 MHz */
	if (d[5] <= 50 && d[6] >= 50) {
		if (d[9] * 10 >= 27) {
			if (d[7] < 16 && d[8] >= 16)
				info->videoFormats[1].supported |= 0x10;	/* 576i */
			if (d[7] < 32 && d[8] >= 32)
				info->videoFormats[1].supported |= 0x1;		/* 576p */
		}
		if (d[9] * 10 >= 75) {
			if (d[7] < 29 && d[8] >= 29)
				info->videoFormats[1].supported |= 0x8;		/* 1080i50 */
			if (d[7] < 38 && d[8] >= 38)
				info->videoFormats[1].supported |= 0x4;		/* 720p50 */
			if (d[9] * 10 >= 149 && d[7] < 57 && d[8] >= 57)
				info->videoFormats[1].supported |= 0x4000;	/* 1080p50 */
		}
	}
	if (d[5] <= 60 && d[6] >= 60) {
		if (d[9] * 10 >= 27) {
			if (d[7] < 16 && d[8] >= 16)
				info->videoFormats[0].supported |= 0x10;	/* 480i */
			if (d[7] < 32 && d[8] >= 32)
				info->videoFormats[0].supported |= 0x1;		/* 480p */
		}
		if (d[9] * 10 >= 74) {
			if (d[7] < 34 && d[8] >= 34)
				info->videoFormats[0].supported |= 0x8;		/* 1080i60 */
			if (d[7] < 45 && d[8] >= 45)
				info->videoFormats[0].supported |= 0x4;		/* 720p60 */
			if (d[9] * 10 >= 148 && d[7] < 68 && d[8] >= 68)
				info->videoFormats[0].supported |= 0x20000;	/* 1080p60 */
		}
	}
}

/* A short video descriptor of a CEA video data block. */
static inline void parseShortVideoDescriptor(SceHdmiEdidInfo *info, SceUInt8 svd)
{
	switch (svd & 0x7f) {
	case 1: info->videoFormats[3].supported |= 0x1; break;
	case 2: info->videoFormats[0].supported |= 0x1; break;
	case 3: info->videoFormats[0].supported |= 0x2; break;
	case 4: info->videoFormats[0].supported |= 0x4; break;
	case 5: info->videoFormats[0].supported |= 0x8; break;
	case 6: info->videoFormats[0].supported |= 0x10; break;
	case 7: info->videoFormats[0].supported |= 0x20; break;
	case 8: info->videoFormats[0].supported |= 0x40; break;
	case 9: info->videoFormats[0].supported |= 0x80; break;
	case 10: info->videoFormats[0].supported |= 0x100; break;
	case 11: info->videoFormats[0].supported |= 0x1000; break;
	case 12: info->videoFormats[0].supported |= 0x2000; break;
	case 13: info->videoFormats[0].supported |= 0x4000; break;
	case 14: info->videoFormats[0].supported |= 0x8000; break;
	case 15: info->videoFormats[0].supported |= 0x10000; break;
	case 16: info->videoFormats[0].supported |= 0x20000; break;
	case 17: info->videoFormats[1].supported |= 0x1; break;
	case 18: info->videoFormats[1].supported |= 0x2; break;
	case 19: info->videoFormats[1].supported |= 0x4; break;
	case 20: info->videoFormats[1].supported |= 0x8; break;
	case 21: info->videoFormats[1].supported |= 0x10; break;
	case 22: info->videoFormats[1].supported |= 0x20; break;
	case 23: info->videoFormats[1].supported |= 0x40; break;
	case 24: info->videoFormats[1].supported |= 0x80; break;
	case 25: info->videoFormats[1].supported |= 0x100; break;
	case 26: info->videoFormats[1].supported |= 0x200; break;
	case 27: info->videoFormats[1].supported |= 0x400; break;
	case 28: info->videoFormats[1].supported |= 0x800; break;
	case 29: info->videoFormats[1].supported |= 0x1000; break;
	case 30: info->videoFormats[1].supported |= 0x2000; break;
	case 31: info->videoFormats[1].supported |= 0x4000; break;
	case 32: info->videoFormats[2].supported |= 0x1; break;
	case 34: info->videoFormats[2].supported |= 0x2; break;
	default: break;
	}
}

/* A CEA-861 extension block: video, HDMI vendor and video capability blocks. */
static inline void parseCeaExtension(SceHdmiEdidInfo *info, const SceUInt8 *block)
{
	SceUInt32 end, offset, len, i;
	SceUInt8 header;
	const SceUInt8 *p;

	if (block == NULL || block[1] < 3 || block[2] < 5 || block[2] > 0x80)
		return;
	end = block[2] - 1;
	offset = 4;
	header = block[offset];
	len = header & 0x1f;
	while (offset + len <= end && header != 0) {
		p = &block[offset + 1];
		switch (header >> 5) {
		case CEA_TAG_VENDOR:
			if (p != NULL && len >= 3 && p[0] == 0x03 && p[1] == 0x0c && p[2] == 0x00 &&
			    len >= 8)
				info->contentTypes = p[7] & 0xf;
			break;
		case CEA_TAG_EXTENDED:
			if (p[0] == 0 && len == 2)
				info->quantizationSelectable = (p[1] >> 6) & 1;
			break;
		case CEA_TAG_VIDEO:
			for (i = 0; i < len; i++)
				parseShortVideoDescriptor(info, p[i]);
			break;
		}
		offset += 1 + len;
		header = block[offset];
		len = header & 0x1f;
	}
}

int edidManagerRead(void)
{
	SceHdmiEdidInfo *info;
	const SceUInt8 *edid = edidManager.edid;
	const SceUInt8 *d;
	int ret, blockCount, i, j;

	if (_getEdidManager() != 0) {
		ret = 7;
		goto out;
	}
	sceHdmiDeviceGetEdid(edidManager.edid, &edidManager.edidBlockCount);
	if (edidManager.edidBlockCount == 0 || (info = edidManagerGetInfo()) == NULL) {
		ret = 1;
		edidManager.valid = 0;
		goto out;
	}
	edidManagerClearInfo();
	blockCount = edidManager.edidBlockCount == 1 ? 1 : 2;
	info->blockCount = blockCount;
	edidManager.blockCount = blockCount;
	for (i = 0; i < 10; i++)
		info->productId[i] = edid[8 + i];
	edidManager.isSonyD702 = info->productId[0] == 0x4d && info->productId[1] == 0xd9 &&
				 info->productId[2] == 0x02 && info->productId[3] == 0xd7;

	/* chromaticity: the high 8 bits of each 10-bit coordinate, then 2 low bits */
	info->chromaticity[0] = (SceUInt8)((edid[0x1b] << 2) + (edid[0x19] >> 6));
	info->chromaticity[1] = (SceUInt8)((edid[0x1c] << 2) + ((edid[0x19] >> 4) & 3));
	info->chromaticity[2] = (SceUInt8)((edid[0x1d] << 2) + ((edid[0x19] >> 2) & 3));
	info->chromaticity[3] = (SceUInt8)((edid[0x1e] << 2) + (edid[0x19] & 3));
	info->chromaticity[4] = (SceUInt8)((edid[0x1f] << 2) + (edid[0x1a] >> 6));
	info->chromaticity[5] = (SceUInt8)((edid[0x20] << 2) + ((edid[0x1a] >> 4) & 3));
	info->chromaticity[6] = (SceUInt8)((edid[0x21] << 2) + ((edid[0x1a] >> 2) & 3));
	info->chromaticity[7] = (SceUInt8)((edid[0x22] << 2) + (edid[0x1a] & 3));
	/* established timings: 640x480@72, 640x480@60, 800x600@60 */
	info->videoFormats[3].supported |= (edid[0x23] & 0x8) | ((edid[0x23] >> 5) & 1) |
					   ((edid[0x23] & 0x1) ? 0x2 : 0);
	info->maxImageWidth = edid[0x15];
	info->maxImageHeight = edid[0x16];

	for (i = 0; i < EDID_DESCRIPTOR_COUNT; i++) {
		d = &edid[0x36 + i * EDID_DESCRIPTOR_SIZE];
		if (d[0] != 0 || d[1] != 0 || d[2] != 0 || d[4] != 0)
			continue;
		if (d[3] == EDID_TAG_MONITOR_NAME) {
			for (j = 0; j < 13; j++) {
				if (d[5 + j] == '\n')
					break;
				info->monitorName[j] = d[5 + j];
			}
		} else if (d[3] == EDID_TAG_RANGE_LIMITS && blockCount == 1) {
			parseRangeLimits(info, d);
		}
	}

	for (i = 1; i < edidManager.edidBlockCount; i++)
		parseCeaExtension(info, &edid[i * EDID_BLOCK_SIZE]);

	edidManager.valid = 1;
	if (!SceSblAIMgrForDriver_71608CA3()) {
		ret = hdmiSetContentType(0);
		if (ret == 0)
			ret = hdmiSetItContent(1);
	} else {
		info = edidManagerGetInfo();
		if (info == NULL) {
			ret = 0;
		} else {
			ret = hdmiSetContentType((info->contentTypes & 8) ? 4 : 0);
			if (ret == 0)
				ret = hdmiSetItContent(info->quantizationSelectable);
		}
	}
out:
	_releaseEdidManager();
	return ret;
}

int edidManagerClear(void)
{
	if (_getEdidManager() != 0) {
		_releaseEdidManager();
		return 7;
	}
	edidManager.edidBlockCount = 0;
	edidManager.blockCount = 0;
	edidManager.valid = 0;
	memset(edidManager.edid, 0, sizeof(edidManager.edid));
	_releaseEdidManager();
	return 0;
}
