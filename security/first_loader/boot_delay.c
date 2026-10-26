/* The Cmep timer is accessed through its control bus. */
#define CONTROL_WRITE(address, value) \
	__asm__ volatile ("stcb %0, %1" : : "r" (value), "i" (address) : "memory")
#define CONTROL_READ(address, value) \
	__asm__ volatile ("ldcb %0, %1" : "=r" (value) : "i" (address) : "memory")

void delayMicroseconds(unsigned int microseconds)
{
	unsigned int status;
	unsigned int zero = 0;
	unsigned int one = 1;

	if (microseconds == 0)
		return;
	CONTROL_WRITE(0x402, zero);
	CONTROL_WRITE(0x403, zero);
	CONTROL_WRITE(0x400, zero);
	CONTROL_WRITE(0x405, zero);
	CONTROL_WRITE(0x401, microseconds * 56U);
	CONTROL_WRITE(0x404, zero);
	CONTROL_WRITE(0x402, one);
	do {
		CONTROL_READ(0x404, status);
	} while (!(status & 1));
	CONTROL_WRITE(0x402, zero);
	CONTROL_WRITE(0x403, zero);
	CONTROL_WRITE(0x405, zero);
	CONTROL_WRITE(0x401, zero);
	CONTROL_WRITE(0x400, one);
	CONTROL_WRITE(0x404, zero);
}
