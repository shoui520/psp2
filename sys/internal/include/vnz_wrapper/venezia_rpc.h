/* Venezia RPC state shared with codec pack and unpack callbacks. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

/* name unknown: one argument buffer transferred by an RPC thunk. */
typedef struct VeneziaRpcMemoryTransfer {
	int copyRequired;
	int nonContiguous;
	int communicationMemory;
	void *virtualBase;
	unsigned int physicalBase;
	SceSize size;
	SceSize allocationSize;
	unsigned int flags;
} VeneziaRpcMemoryTransfer;

typedef struct VeneziaMemoryOption {
	unsigned int flags;
	SceSize allocationSize;
	unsigned int reserved;
} VeneziaMemoryOption;

/* name unknown: per-call transaction state owned by the RPC provider. */
typedef struct VeneziaRpcThunk {
	unsigned int channel;
	unsigned int allocationCount;
	int packing;
	VeneziaRpcMemoryTransfer memories[16];
} VeneziaRpcThunk;

typedef int (*VeneziaRpcPack)(VeneziaRpcThunk *, const unsigned int *, unsigned int *);
typedef void (*VeneziaRpcUnpack)(VeneziaRpcThunk *);

_Static_assert(sizeof(VeneziaRpcMemoryTransfer) == 32, "Venezia RPC transfer size");
_Static_assert(sizeof(VeneziaMemoryOption) == 12, "Venezia memory option size");
_Static_assert(sizeof(VeneziaRpcThunk) == 524, "Venezia RPC thunk size");
_Static_assert(offsetof(VeneziaRpcThunk, memories) == 12, "Venezia RPC transfers");
_Static_assert(offsetof(VeneziaRpcMemoryTransfer, virtualBase) == 12,
	"Venezia RPC virtual address");
_Static_assert(offsetof(VeneziaRpcMemoryTransfer, size) == 20,
	"Venezia RPC transfer length");
