if(FW_DEBUG)
	set(krm_order ../krm_challenge.c ../krm_state_keys.c)
else()
	set(krm_order ../krm_state_keys.c ../krm_challenge.c)
endif()

psp2_module(os0_kd_bootimage_krm KIND kernel SCE_LAYOUT SOURCES
	../krm.c
	${krm_order}
	../krm_control.c
	../krm_key_ring.c
	../krm_ecc.c
	../import_bindings.S
)
