# SceUdcd: Sony's udcd_udc2.c (the UDC2 controller), the driver/API code and the user syscalls.
psp2_module(os0_kd_bootimage_udcd KIND kernel SOURCES
	source/udcd.c
	source/udcd_udc2.c
	source/udcd_user.c
)

# vita-elf-create appends the module/export/import records to the RX segment.
target_link_options(os0_kd_bootimage_udcd PRIVATE
	"LINKER:-T,${CMAKE_CURRENT_SOURCE_DIR}/source/layout.ld")
set_property(TARGET os0_kd_bootimage_udcd APPEND PROPERTY LINK_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/source/layout.ld")
