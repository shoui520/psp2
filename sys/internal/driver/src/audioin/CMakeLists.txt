if(FW_DEBUG)
	set(audioin_capture_order capture_process.c capture_buffers.c)
else()
	set(audioin_capture_order capture_buffers.c capture_process.c)
endif()

psp2_module(os0_kd_bootimage_audioin KIND kernel COMPILER gcc48 SOURCES
	capture_config.c
	${audioin_capture_order}
	capture.c
	port.c
	legacy.c
	aec.c
	resampler.c
	aec_core.c
	aec_rpc.c
	capture_data.c
)
