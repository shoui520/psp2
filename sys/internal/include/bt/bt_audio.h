/* Bluetooth audio and AVRCP device interfaces. */
#pragma once

#include <psp2kern/types.h>

int SceBtForDriver_E9936446(SceUInt64 deviceAddress);
int SceBtForDriver_EC5F8519(SceUInt64 deviceAddress, int button);
int SceBtForDriver_7689DA3D(SceUInt64 deviceAddress, int volume);
int SceBtForDriver_C9C70056(SceUInt64 deviceAddress);
int SceBtForDriver_8D47CABD(SceUInt64 deviceAddress, int profile);
int SceBtForDriver_CAE5A9F3(SceUInt64 deviceAddress, int profile);
int SceBtForDriver_DA20DCC8(SceUInt64 deviceAddress);
int SceBtForDriver_47F19727(SceUInt64 deviceAddress, void *buffer, SceSize size);
int SceBtForDriver_CD7C0492(SceUInt64 deviceAddress, void *buffer, SceSize size);

int SceBtForDriver_014A1D28(uint64_t deviceAddress, const void *data, int length);
int SceBtForDriver_08632086(int protection);
int SceBtForDriver_0B7D8E0E(uint64_t deviceAddress, uint8_t *buffer, int capacity);
int SceBtForDriver_1AB31C7D(void);
int SceBtForDriver_1C26F878(const void *resultBuffer);
int SceBtForDriver_21C788E2(void);
int SceBtForDriver_2C9DD7BB(uint64_t deviceAddress, void *buffer);
int SceBtForDriver_3333A878(void *buffer, int maxCount);
int SceBtForDriver_3BBC1EE1(uint64_t deviceAddress, uint8_t reportId);
int SceBtForDriver_4F5A852E(int enabled);
int SceBtForDriver_6059113A(unsigned int mac0, unsigned int mac1);
int SceBtForDriver_633F784C(uint64_t deviceAddress, char *buffer);
int SceBtForDriver_6A1D9598(void);
int SceBtForDriver_6E49D11D(void);
int SceBtForDriver_780A5557(uint64_t deviceAddress, void *buffer);
int SceBtForDriver_7BC66D6D(void);
int SceBtForDriver_7FF77792(uint64_t deviceAddress, const void *path, unsigned int length);
int SceBtForDriver_8008AA2C(void);
int SceBtForDriver_91EF55AD(const void *title, unsigned int length);
int SceBtForDriver_92241DFB(uint64_t deviceAddress, const void *data, unsigned int length);
int SceBtForDriver_94C66A3B(void);
int SceBtForDriver_9C195411(int selector, void *buffer, int size);
int SceBtForDriver_9DC7E7E5(void);
int SceBtForDriver_AB2032A3(uint64_t deviceAddress, const void *data, int length);
int SceBtForDriver_AB57F8C1(SceUID callback, int vendorId, int productId);
int SceBtForDriver_AD31A43E(void);
int SceBtForDriver_B5FA291C(unsigned int mac0, unsigned int mac1, int accepted);
int SceBtForDriver_BB69A62A(unsigned int mac0, unsigned int mac1, unsigned char *code, SceSize length);
int SceBtForDriver_BF72E1BE(SceUID callback);
int SceBtForDriver_C09D9E7C(uint64_t deviceAddress, void *data, int length);
int SceBtForDriver_C5E7349C(int status);
int SceBtForDriver_CA9B885C(uint64_t deviceAddress, uint8_t reportId);
int SceBtForDriver_D2449052(uint64_t deviceAddress, const void *data, int length);
int SceBtForDriver_E3A66814(uint64_t deviceAddress, const void *data, unsigned int length);
int SceBtForDriver_E6F659E0(uint64_t deviceAddress);
int SceBtForDriver_E8E95977(const void *path, unsigned int length);
int SceBtForDriver_EBDCC992(int configuration);
