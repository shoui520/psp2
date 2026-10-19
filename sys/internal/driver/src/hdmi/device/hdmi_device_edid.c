/*
 * SceHdmi device layer: the sink's EDID. It is read over the DDC into
 * pDevice.edid (the base block and up to three CEA extensions), and its
 * HDMI vendor specific data block decides between HDMI and DVI and gives
 * the CEC physical address. Internal names are ours unless a comment says
 * otherwise.
 */
#include <stddef.h>
#include <scetypes.h>
#include <threadmgr/threadmgr.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "../hdmi_internal.h"

#define HDMI_EDID_MAX_BLOCKS	4

/* the base block's extension count, and an extension's tag */
#define EDID_EXTENSION_COUNT	0x7e
#define EDID_TAG_CEA		0x02

/* CEA-861 data block collection */
#define CEA_REVISION		1
#define CEA_DTD_OFFSET		2
#define CEA_DATA_BLOCKS		4

/* the HDMI Licensing IEEE OUI, 00-0c-03, as the VSDB stores it */
#define HDMI_OUI_0		0x03
#define HDMI_OUI_1		0x0c
#define HDMI_OUI_2		0x00

/* HdmiDevice.sinkType */
#define HDMI_SINK_DVI		1
#define HDMI_SINK_HDMI		2

#define SCE_ERROR_INVALID_POINTER	0x80000013
#define SCE_ERROR_NOT_SUPPORTED		0x80000031

static SceUID edidMutex;

int hdmiDeviceEdidInitialize(HdmiDevice *device)
{
	edidMutex = 0;
	edidMutex = sceKernelCreateMutex("SceHdmiDeviceEdid", 0x2002, 0, NULL);
	if (edidMutex < 0)
		return SCE_HDMI_ERROR_DEVICE;
	return 0;
}

void hdmiDeviceEdidClear(HdmiDevice *device)
{
	SceUInt32 timeout = 0xffffffff;

	sceKernelLockMutex(edidMutex, 1, &timeout);
	device->edidBlockCount = 0;
	sceKernelUnlockMutex(edidMutex, 1);
}

int hdmiDeviceEdidSetDviMode(HdmiDevice *device)
{
	device->sinkType = HDMI_SINK_DVI;
	return HdmiDeviceHwSetHDMIMode(device, 0);
}

/* HDMI when a CEA extension has the HDMI VSDB, DVI otherwise. */
int hdmiDeviceEdidCheckSinkType(HdmiDevice *device)
{
	SceUInt32 block;
	int base, offset, dtdOffset;
	int sinkType, hdmi;

	if (hdmiDeviceIsEdidOn()) {
		if (hdmiDeviceIsEdidForced()) {
			device->sinkType = HDMI_SINK_DVI;
			HdmiDeviceHwSetHDMIMode(device, 0);
			return 0;
		}
		device->sinkType = HDMI_SINK_HDMI;
		HdmiDeviceHwSetHDMIMode(device, 1);
		return 0;
	}
	if (device->edidBlockCount >= 2)
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "EDID Block Size is %d.\n",
			    device->edidBlockCount);
	for (block = 1; block < device->edidBlockCount; block++) {
		base = block * EDID_BLOCK_SIZE;
		if (device->edid[base] != EDID_TAG_CEA)
			continue;
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "ATD Block Revision %d.\n",
			    device->edid[base + CEA_REVISION]);
		if (device->edid[base + CEA_REVISION] < 3)
			continue;
		dtdOffset = device->edid[base + CEA_DTD_OFFSET];
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "ATD DTD Offset [%02x].\n", dtdOffset);
		for (offset = CEA_DATA_BLOCKS; offset < dtdOffset;
		     offset += (device->edid[base + offset] & CEA_LENGTH_MASK) + 1) {
			HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "ATD CEA Tag Code [%02x].\n",
				    device->edid[base + offset]);
			if ((device->edid[base + offset] & CEA_TAG_MASK) !=
			    CEA_TAG_VENDOR << CEA_TAG_SHIFT)
				continue;
			HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "VSDB Code [%02x][%02x][%02x].\n",
				    device->edid[base + offset + 1], device->edid[base + offset + 2],
				    device->edid[base + offset + 3]);
			if (device->edid[base + offset + 1] == HDMI_OUI_0 &&
			    device->edid[base + offset + 2] == HDMI_OUI_1 &&
			    device->edid[base + offset + 3] == HDMI_OUI_2) {
				HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "HDMI Mode Set.\n");
				device->physicalAddress[0] = device->edid[base + offset + 4];
				device->physicalAddress[1] = device->edid[base + offset + 5];
				sinkType = HDMI_SINK_HDMI;
				hdmi = 1;
				goto set;
			}
		}
	}
	HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "DVI Mode Set.\n");
	device->physicalAddress[0] = 0xff;
	device->physicalAddress[1] = 0xff;
	sinkType = HDMI_SINK_DVI;
	hdmi = 0;
set:
	device->sinkType = sinkType;
	HdmiDeviceHwSetHDMIMode(device, hdmi);
	return 0;
}

/* Read the EDID: its header, the base block and the CEA extensions. */
int hdmiDeviceEdidRead(HdmiDevice *device)
{
	SceUInt32 header[2];
	HdmiDdcRequest request = { 0, 0, 8, 0, header };
	SceUInt32 timeout;
	int extensions, block;
	int ret, ret2;

	if (hdmiDeviceIsEdidOn()) {
		device->edidBlockCount = 0;
		return 0;
	}
	ret = sceHdmiDeviceDdcRead(device, &request, 200000, 0);
	if (ret != 0) {
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "Error Read EDID Header 1st.\n");
		return 1;
	}
	if (header[0] != 0xffffff00 || header[1] != 0x00ffffff) {
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "Error  EDID Header 1st.\n");
		return 1;
	}
	timeout = 0xffffffff;
	sceKernelLockMutex(edidMutex, 1, &timeout);
	device->edidBlockCount = 0;
	ret = hdmiDeviceEdidReadBlock(device, 0, device->edid);
	if (ret != 0) {
		HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "DDCEDID Read Error 1\n");
		goto out;
	}
	extensions = (SceUInt8)(device->edid[device->edidBlockCount * EDID_BLOCK_SIZE +
					     EDID_EXTENSION_COUNT] + 1);
	device->edidBlockCount++;
	if (extensions < 2 || device->edidBlockCount >= HDMI_EDID_MAX_BLOCKS)
		goto out;
	for (block = 1; block < extensions && device->edidBlockCount < HDMI_EDID_MAX_BLOCKS;
	     block++) {
		ret2 = hdmiDeviceEdidReadBlock(device, block, device->edid +
					       device->edidBlockCount * EDID_BLOCK_SIZE);
		if (ret2 != 0) {
			HDMI_PRINTF(HDMI_DBG_EDID, "[hdmiEDID] ", "DDCEDID Read Error 2\n");
			ret = ret2;
			break;
		}
		/* only CEA extensions are kept */
		if (device->edid[device->edidBlockCount * EDID_BLOCK_SIZE] == EDID_TAG_CEA)
			device->edidBlockCount++;
	}
out:
	sceKernelUnlockMutex(edidMutex, 1);
	return ret;
}

int hdmiDeviceEdidGetPhysicalAddress(HdmiDevice *device, SceUInt8 *address)
{
	SceUInt32 timeout = 0xffffffff;
	int ret;

	sceKernelLockMutex(edidMutex, 1, &timeout);
	if (device->sinkType == HDMI_SINK_HDMI &&
	    (!hdmiDeviceIsEdidOn() || !hdmiDeviceIsEdidNotForced())) {
		if (address == NULL) {
			ret = SCE_ERROR_INVALID_POINTER;
		} else {
			address[0] = device->physicalAddress[0];
			address[1] = device->physicalAddress[1];
			ret = 0;
		}
	} else {
		ret = SCE_ERROR_NOT_SUPPORTED;
	}
	sceKernelUnlockMutex(edidMutex, 1);
	return ret;
}

