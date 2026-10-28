# generated skeleton (T2): stub bodies, original module/export/import tables
psp2_module(os0_kd_bootimage_motion KIND kernel COMPILER gcc48 SOURCES
	magnetometer.c
	motion_command.c
	motion_protocol.c
	motion_sensor.c
	motion_device.c
	motion_spi.c
	motion_manager.c
	ring.c
	syscall.c
	motion_heap.c
	heap.c
	filter.c
)
# ring.c, heap.c and filter.c are the library SceMotionDevDummy links too; Sony built
# them at -O3 there and here (the ring-reader searches are unrolled compare chains)
set_source_files_properties(ring.c heap.c filter.c motion_protocol.c PROPERTIES COMPILE_OPTIONS -O3)
