/* SceOled driver interface and dependency declarations missing from vitasdk. */
#pragma once
#include <stdint.h>
#include <sysmem/sysroot.h>

int SceOledForDriver_2F0C4B67(unsigned char command,
			  const unsigned char *data, unsigned length);
int SceOledForDriver_3E6AE8A2(unsigned command, unsigned char *data,
			  unsigned length);
/* vitasdk: ksceOledGetDDB */
int SceOledForDriver_C9D5987C(uint16_t *id, uint16_t *revision);
int SceOledForDriver_F9624C47(unsigned value);	/* vitasdk: ksceOledSetBrightness */
unsigned SceOledForDriver_43EF811A(void);	/* vitasdk: ksceOledGetBrightness */
int SceOledForDriver_DABBD9D3(unsigned mode);	/* vitasdk: ksceOledSetDisplayColorSpaceMode */
unsigned SceOledForDriver_4F8A1D4A(void);	/* vitasdk: ksceOledGetDisplayColorSpaceMode */

int SceOledForDriver_0CC6BCB4(void);	/* vitasdk: ksceOledWaitReady */
int SceOledForDriver_4C7836C7(void);	/* vitasdk: ksceOledDisplayOn */
int SceOledForDriver_BC84602E(void);	/* vitasdk: ksceOledDisplayOff */
int SceOledForDriver_9F4ABDDC(void);
int SceOledForDriver_DDB1412B(void);
int SceOledForDriver_E30604CC(void);
int SceOledForDriver_ED2D6F19(unsigned mode);
int SceOledForDriver_BF5C5624(unsigned poll);

int SceOledForDriver_26F9EEA8(void);
int SceOledForDriver_3C990F53(void);
int SceOledForDriver_4891257F(void);
int SceOledForDriver_5AF31932(const unsigned char *p);
int SceOledForDriver_6B1E0B52(void);
int SceOledForDriver_AD6C7FB8(const unsigned char *p);
