psp2_module(os0_kd_bootimage_usbaudio KIND kernel COMPILER gcc48 SOURCES
	usbaudio.c
	usbaudio_device.c
)

# Built at -O3: the original unrolls the device table scans and vectorizes the
# mono-to-stereo copy (vld2.8) in SceUsbAudioForDriver_EFB4E3E4.
set_source_files_properties(usbaudio.c usbaudio_device.c PROPERTIES COMPILE_OPTIONS "-O3")
