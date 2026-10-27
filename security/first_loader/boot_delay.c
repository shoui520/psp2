/* The Cmep timer is accessed through its control bus. */
#define CONTROL_WRITE(address, value) \
	__asm__ volatile ("stcb %0, %1" : : "r" (value), "i" (address) : "memory")
#define CONTROL_READ(address, value) \
	__asm__ volatile ("ldcb %0, %1" : "=r" (value) : "i" (address) : "memory")

void delayMicroseconds(unsigned int microseconds)
{
	register unsigned int zero __asm__("$0");
	register unsigned int one __asm__("$2");
	register unsigned int scale __asm__("$3");
	register unsigned int status __asm__("$0");

	if (microseconds == 0)
		return;
	zero = 0;
	one = 1;
	CONTROL_WRITE(0x402, zero);
	CONTROL_WRITE(0x403, zero);
	CONTROL_WRITE(0x400, zero);
	CONTROL_WRITE(0x405, zero);
	scale = 56;
	__asm__ volatile ("mulru %0, %1"
			  : "+r" (microseconds) : "r" (scale));
	CONTROL_WRITE(0x401, microseconds);
	CONTROL_WRITE(0x404, zero);
	CONTROL_WRITE(0x402, one);
	do {
		CONTROL_READ(0x404, status);
		__asm__ volatile ("and %0, %1" : "+r" (status) : "r" (one));
	} while (status == 0);
	zero = 0;
	CONTROL_WRITE(0x402, zero);
	CONTROL_WRITE(0x403, zero);
	CONTROL_WRITE(0x405, zero);
	CONTROL_WRITE(0x401, zero);
	CONTROL_WRITE(0x400, one);
	CONTROL_WRITE(0x404, zero);
}
