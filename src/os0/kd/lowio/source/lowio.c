/* SceLowio module initialization; stage return values are ignored by Sony. */
#include <stddef.h>

int LowioPervasiveInit(void);
int LowioGpioInit(void);
int LowioPwmInit(void);
int LowioI2cInit(void);
int LowioCdramInit(void);
int LowioDsiInit(void);
int LowioIftuInit(void);
int LowioCsiInit(void);

int module_start(unsigned arglen, void *argp)
{
	(void)arglen;
	(void)argp;
	LowioPervasiveInit();
	LowioGpioInit();
	LowioPwmInit();
	LowioI2cInit();
	LowioCdramInit();
	LowioDsiInit();
	LowioIftuInit();
	LowioCsiInit();
	return 0;
}
