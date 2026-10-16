/* SceLowio module start: each stage's result is ignored. */
#include "lowio_internal.h"

int module_start(SceSize arglen, const void *argp)
{
	LowioPervasiveInit(arglen, argp);
	LowioGpioInit(arglen, argp);
	LowioPwmInit(arglen, argp);
	LowioI2cInit(arglen, argp);
	LowioGrabInit(arglen, argp);
	LowioCdramInit(arglen, argp);
	LowioDsiInit(arglen, argp);
	LowioIftuInit(arglen, argp);
	LowioCsiInit(arglen, argp);
	return 0;
}
