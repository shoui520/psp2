# generated skeleton (T2): stub bodies, original module/export/import tables
psp2_module(os0_kd_bootimage_npdrm KIND kernel COMPILER gcc48 SOURCES
	package.c
	npdrm.c
	../../../driver/src/msif/msif_ecdsa.c
)
# npdrm's copy of the shared ECC file verifies the activation data with the 160-bit
# parameters (5 words, 20-byte digest), not msif's 224-bit ones.
set_source_files_properties(../../../driver/src/msif/msif_ecdsa.c PROPERTIES
	COMPILE_DEFINITIONS "MSIF_ECDSA_VERIFY_WORDS=5;MSIF_ECDSA_VERIFY_BYTES=20")
