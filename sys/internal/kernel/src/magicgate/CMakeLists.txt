# Wrapped data is shared by the debug pair and by the retail/release pair.
if(FW STREQUAL "4.00-DEBUGGER" OR FW STREQUAL "3.630.011-DEBUGGER")
	set(MAGICGATE_WRAPPED_DATA data/wrapped-debug.c)
else()
	set(MAGICGATE_WRAPPED_DATA data/wrapped-retail.c)
endif()
psp2_module(os0_kd_magicgate KIND kernel PARTIAL SOURCES
	source/magicgate_lifecycle.c
	source/magicgate_mutex.c
	source/magicgate_tree.c
	source/magicgate_slots.c
	source/magicgate_record.c
	source/magicgate_cipher.c
	source/magicgate_api.c
	source/magicgate_file.c
	source/magicgate_crypto.c
	source/magicgate_keys.c
	${MAGICGATE_WRAPPED_DATA}
)
