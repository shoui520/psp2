/* Shared DECI4P 24-bit packet-size encoding used by the debug managers. */
#pragma once
#include <deci4p_dfmgr/deci4p.h>

static inline SceSize deci4pGetSize(const SceDeci4pHeader *header)
{
	return *(const SceUInt16 *)header->size | header->size[2] << 16;
}

static inline void deci4pSetSize(SceDeci4pHeader *header, SceSize size)
{
	*(SceUInt16 *)header->size = size;
	header->size[2] = size >> 16;
}
