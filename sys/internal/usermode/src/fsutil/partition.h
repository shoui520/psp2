/* fsutil's partition tables and formatting (partition.c), which appmgr builds in. */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sdstor/mbr.h>
#include <sdstor/sdstor.h>

int _fs_util_get_device_info(const char *blockDeviceName, void *buffer,
			     unsigned int bufferSize);
int _fs_util_device_format(const char *blockDeviceName);
uint64_t appmgrMbrGetFdiskOffset(const SceSdstorMbr *mbr, unsigned int index);
uint64_t appmgrMbrGetFdiskSize(const SceSdstorMbr *mbr, unsigned int index);
uint64_t appmgrMbrGetPartitionOffset(const SceMbr *mbr, int index);
uint64_t appmgrMbrGetPartitionSize(const SceMbr *mbr, int index);
unsigned int appmgrMbrGetFormatType(const SceMbr *mbr, int index);
unsigned int appmgrMbrGetClusterSize(const SceMbr *mbr, int index);
int _fs_util_prepare_rm_mbr(const char *blockDeviceName, SceUID file,
			    uint64_t volumeSize, unsigned int sectorSize,
			    SceMbr *buffer, unsigned int bufferSize);
int _fs_util_prepare_mbr(const char *blockDeviceName, unsigned char systemId,
			 SceMbr *buffer, unsigned int bufferSize,
			 uint64_t volumeSize);
