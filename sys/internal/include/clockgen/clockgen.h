/* SceClockgen driver interface additions to vitasdk. */
#pragma once

/* Name unknown: disable the motion clock when that clock is present. */
int SceClockgenForDriver_66AC5580(void);

/* Enable and disable the camera clock, respectively. */
int SceClockgenForDriver_06663ECA(void);
int SceClockgenForDriver_82F39378(void);

int SceClockgenForDriver_09072366(unsigned int reg);
int SceClockgenForDriver_0BA41732(void);
int SceClockgenForDriver_5130682D(void);
int SceClockgenForDriver_62478916(void);
int sceClockgenWlanBtClkEnable(void);
int SceClockgenForDriver_820D3516(int frequency);
int SceClockgenForDriver_A2CA8B65(int spread);
int sceClockgenWlanBtClkDisable(void);
int SceClockgenForDriver_CF63B11D(void);
int SceClockgenForDriver_EFE8A68C(int singleRead);
int SceClockgenForDriver_F0A0242D(void);
