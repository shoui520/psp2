/* SceOled driver interface and dependency declarations missing from vitasdk. */
#pragma once
#include <stdint.h>
#include <sysmem/sysroot.h>

int SceOledForDriver_2F0C4B67(unsigned char command,
			  const unsigned char *data, unsigned length);
int SceOledForDriver_3E6AE8A2(unsigned command, unsigned char *data,
			  unsigned length);
int ksceOledGetDDB(uint16_t *id, uint16_t *revision);
int ksceOledSetBrightness(unsigned value);
unsigned ksceOledGetBrightness(void);
int ksceOledSetDisplayColorSpaceMode(unsigned mode);
unsigned ksceOledGetDisplayColorSpaceMode(void);

int ksceOledWaitReady(void);
int ksceOledDisplayOn(void);
int ksceOledDisplayOff(void);
int SceOledForDriver_9F4ABDDC(void);
int SceOledForDriver_DDB1412B(void);
int SceOledForDriver_E30604CC(void);
int SceOledForDriver_ED2D6F19(unsigned mode);
int SceOledForDriver_BF5C5624(unsigned poll);
