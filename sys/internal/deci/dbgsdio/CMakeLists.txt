psp2_module(os0_kd_dbgsdio KIND kernel SOURCES
	dbgsdio.c
)

# Preserve the channel array address, including unchecked index arithmetic.
target_link_options(os0_kd_dbgsdio PRIVATE -Wl,-Tdata=0x81003000)
