# Wrapped data is shared by the debug pair and by the retail/release pair.
if(FW STREQUAL "4.00-DEBUGGER" OR FW STREQUAL "3.630.011-DEBUGGER")
	set(MAGICGATE_WRAPPED_DATA data/wrapped-debug.c)
else()
	set(MAGICGATE_WRAPPED_DATA data/wrapped-retail.c)
endif()
psp2_module(os0_kd_magicgate KIND kernel COMPILER gcc48 PARTIAL SOURCES
	magicgate_lifecycle.c
	magicgate_mutex.c
	magicgate_tree.c
	magicgate_slots.c
	magicgate_record.c
	magicgate_cipher.c
	magicgate_api.c
	magicgate_file.c
	magicgate_crypto.c
	magicgate_keys.c
	${MAGICGATE_WRAPPED_DATA}
)
