if(FW_DEBUG)
	set(krm_order ../../krm/source/krm_challenge.c ../../krm/source/krm_state_keys.c)
else()
	set(krm_order ../../krm/source/krm_state_keys.c ../../krm/source/krm_challenge.c)
endif()

psp2_module(os0_kd_bootimage_krm KIND kernel SCE_LAYOUT SOURCES
	../../krm/source/krm.c
	${krm_order}
	../../krm/source/krm_control.c
	../../krm/source/krm_key_ring.c
	../../krm/source/krm_ecc.c
	../../krm/source/import_bindings.S
)
