/* Bluetooth HID requests shared by the caller and the transport queues. */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>

typedef struct _SceBtHidRequest {
	struct _SceBtHidRequest *next;
	struct _SceBtHidRequest *prev;
	uint8_t type;
	uint8_t reportId;
	uint8_t internalFlags[2];
	void *buffer;
	uint32_t length;
	uint32_t argument;
} SceBtHidRequest;

_Static_assert(sizeof(SceBtHidRequest) == 24, "SceBtHidRequest");
_Static_assert(_Alignof(SceBtHidRequest) == 4, "SceBtHidRequest alignment");
_Static_assert(offsetof(SceBtHidRequest, next) == 0, "SceBtHidRequest.next");
_Static_assert(offsetof(SceBtHidRequest, prev) == 4, "SceBtHidRequest.prev");
_Static_assert(offsetof(SceBtHidRequest, type) == 8, "SceBtHidRequest.type");
_Static_assert(offsetof(SceBtHidRequest, reportId) == 9, "SceBtHidRequest.reportId");
_Static_assert(offsetof(SceBtHidRequest, internalFlags) == 10,
	"SceBtHidRequest.internalFlags");
_Static_assert(offsetof(SceBtHidRequest, buffer) == 12, "SceBtHidRequest.buffer");
_Static_assert(offsetof(SceBtHidRequest, length) == 16, "SceBtHidRequest.length");
_Static_assert(offsetof(SceBtHidRequest, argument) == 20, "SceBtHidRequest.argument");

int ksceBtRegisterCallback(SceUID callback, uint64_t eventMask);
int ksceBtHidTransfer(uint32_t mac0, uint32_t mac1, SceBtHidRequest *request);
int ksceBtHidGetReportDescriptor(uint64_t deviceAddress, void *buffer,
			       unsigned int size);
