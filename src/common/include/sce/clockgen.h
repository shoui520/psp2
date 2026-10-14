/* SceClockgen driver interface additions to vitasdk. */
#pragma once

/* Name unknown: disable the motion clock when that clock is present. */
int SceClockgenForDriver_66AC5580(void);

/* Enable and disable the camera clock, respectively. */
int SceClockgenForDriver_06663ECA(void);
int SceClockgenForDriver_82F39378(void);
