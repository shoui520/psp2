# os0:kd/buserror.skprx - bus error interrupt: hand the error to the secure world for a crash dump
if(FW_DEBUG)
	psp2_module(os0_kd_buserror KIND kernel
		SOURCES buserror.c buserror_es3.c
		IMPORTS snprintf
	)
else()
	psp2_module(os0_kd_buserror KIND kernel
		SOURCES buserror.c buserror_es3.c
	)
endif()
