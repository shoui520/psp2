if(FW_DEBUG)
	set(audioin_capture_order source/capture_process.c source/capture_buffers.c)
else()
	set(audioin_capture_order source/capture_buffers.c source/capture_process.c)
endif()

psp2_module(os0_kd_bootimage_audioin KIND kernel SOURCES
	source/capture_config.c
	${audioin_capture_order}
	source/capture.c
	source/port.c
	source/legacy.c
	source/aec.c
	source/resampler.c
	source/aec_core.c
	source/aec_rpc.c
	source/capture_data.c
)
