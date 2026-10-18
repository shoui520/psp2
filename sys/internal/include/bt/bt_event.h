/* Bluetooth event records shared by the transport and its clients. */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct SceBtEvent {
	union {
		uint8_t data[16];
		struct {
			uint8_t id;
			uint8_t status;
			uint16_t reserved;
			uint32_t argument;
			uint32_t mac0;
			uint32_t mac1;
		};
	};
} SceBtEvent;

_Static_assert(sizeof(SceBtEvent) == 16, "SceBtEvent");
_Static_assert(_Alignof(SceBtEvent) == 4, "SceBtEvent alignment");
_Static_assert(offsetof(SceBtEvent, data) == 0, "SceBtEvent.data");
_Static_assert(offsetof(SceBtEvent, id) == 0, "SceBtEvent.id");
_Static_assert(offsetof(SceBtEvent, status) == 1, "SceBtEvent.status");
_Static_assert(offsetof(SceBtEvent, reserved) == 2, "SceBtEvent.reserved");
_Static_assert(offsetof(SceBtEvent, argument) == 4, "SceBtEvent.argument");
_Static_assert(offsetof(SceBtEvent, mac0) == 8, "SceBtEvent.mac0");
_Static_assert(offsetof(SceBtEvent, mac1) == 12, "SceBtEvent.mac1");

int SceBtForDriver_5ABB9A9D(SceBtEvent *events, int count);
