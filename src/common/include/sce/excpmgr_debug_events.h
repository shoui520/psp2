/* Sony's debug-event descriptions, shared by exception report code. */
#pragma once

#define SCE_EXCP_DEBUG_EVENTS(X) \
	X(0, "Halt Request debug event") \
	X(1, "Breakpoint debug event") \
	X(2, "Asynchronous Watchpoint debug event") \
	X(3, "BKPT Instruction debug event") \
	X(4, "External Debug Request debug event") \
	X(5, "Vector Catch debug event") \
	X(6, "0x6") \
	X(7, "0x7") \
	X(8, "OS Unlock Catch debug event") \
	X(9, "0x9") \
	X(10, "Synchronous Watchpoint debug event") \
	X(11, "0xB") \
	X(12, "0xC") \
	X(13, "0xD") \
	X(14, "0xE") \
	X(15, "0xF")

