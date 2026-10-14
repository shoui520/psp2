if(FW_DEBUG)
	set(krm_order source/krm_challenge.c source/krm_state_keys.c)
else()
	set(krm_order source/krm_state_keys.c source/krm_challenge.c)
endif()

psp2_module(os0_kd_krm KIND kernel SCE_LAYOUT SOURCES
	source/krm.c
	${krm_order}
	source/krm_control.c
	source/krm_key_ring.c
	source/krm_ecc.c
	source/import_bindings.S
)
