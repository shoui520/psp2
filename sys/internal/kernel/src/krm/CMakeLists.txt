if(FW_DEBUG)
	set(krm_order krm_challenge.c krm_state_keys.c)
else()
	set(krm_order krm_state_keys.c krm_challenge.c)
endif()

# one Sony build, shipped as os0:kd/krm and inside bootimage (os0:kd/bootimage/krm):
# the target is named from the rel
psp2_module(${PSP2_TARGET} KIND kernel COMPILER gcc48 SCE_LAYOUT SOURCES
	driver.c
	${krm_order}
	krm.c
	key-ring.c
	krm_ecc.c
)
