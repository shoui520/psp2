/*
 * SceNpCommonPs4: the clock, the debug print and the allocators.
 */

#include "np_common_ps4_internal.h"

extern "C" {

extern SceUInt32 SceThreadmgr_47F6DE49(void);
extern SceInt64 sceKernelGetSystemTimeWide(void);
extern int sceKernelGetProcessId(void);

int SceNpCommonPs4_F56B0EAB(SceInt64 *pTime)
{
	*pTime = sceKernelGetSystemTimeWide();
	return 0;
}

int SceNpCommonPs4_A25DA29A(SceUInt32 *pTime)
{
	*pTime = SceThreadmgr_47F6DE49();
	return 0;
}

int SceNpCommonPs4_93DA3594(const char *data, int length)
{
	return sceClibPrintf("%.*s", length, data);
}

void SceNpCommonPs4_0A7044DA(const unsigned char *data, SceSize size)
{
	char line[128];
	char *out;
	const unsigned char *hex;
	SceSize hexIndex;
	SceSize textIndex;
	int hexDone;
	int textDone;
	int i;
	int notDone;
	int terminator;

	hexIndex = 0;
	notDone = 0;
	hex = data;
	textIndex = 0;
	if (size != 0) {
		terminator = 0;
		do {
			hexDone = notDone;
			out = line;
			while (1) {
				if (hexDone != 0)
					break;
				i = hexIndex & 0xF;
				if (size > hexIndex) {
					sceClibSnprintf(out, 128, "%.2x", *hex++);
					out += 2;
					if ((hexIndex & 1) == 1)
						*out++ = ' ';
					if (i == 15)
						hexDone = 1;
					hexIndex++;
					continue;
				}
				while (i < 16) {
					out[0] = ' ';
					out[1] = ' ';
					out += 2;
					if ((i & 1) == 1) {
						*out = ' ';
						out++;
					}
					i++;
				}
				break;
			}
			textDone = notDone;
			*out++ = ' ';
			do {
				if (size <= textIndex)
					break;
				if (*data < 0x20 || *data > 0x7E)
					*out = '.';
				else
					*out = *data;
				out++;
				data++;
				if ((textIndex & 0xF) == 0xF)
					textDone = 1;
				textIndex++;
			} while (textDone == 0);
			*out = terminator;
			sceClibPrintf("%s\n", line);
		} while (size > hexIndex);
	}
	sceClibPrintf("\n");
}

int SceNpCommonPs4_F50BD96A(void)
{
	*(volatile int *)0 = 0;
	return 0;
}

void SceNpCommonPs4_31DB2D97(void)
{
}

int SceNpCommonPs4_AA980113(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	return ((int (*)(void *, void *))allocator->allocate)(pointer, allocator->context);
}

int SceNpCommonPs4_67BBE5F9(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size)
{
	return ((int (*)(void *, SceSize, void *))allocator->reallocate)(pointer, size, allocator->context);
}

int SceNpCommonPs4_6B428829(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	return ((int (*)(void *, void *))allocator->deallocate)(pointer, allocator->context);
}

char *SceNpCommonPs4_1CAB4A2E(struct NpCommonPs4Allocator *allocator, const char *string)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, 0xFFFFFFFF) + 1;
	copy = (char *)allocator->allocate(length, allocator->context);
	if (copy != NULL)
		sceClibStrncpy(copy, string, length);
	return copy;
}

char *SceNpCommonPs4_83496021(struct NpCommonPs4Allocator *allocator, const char *string, SceSize maxLength)
{
	SceSize length;
	char *copy;

	length = sceClibStrnlen(string, maxLength) + 1;
	copy = (char *)allocator->allocate(length, allocator->context);
	if (copy != NULL)
		sceClibStrncpy(copy, string, length);
	return copy;
}

int SceNpCommonPs4_299ADFC3(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	return ((int (*)(void *, void *))allocator->allocate)(pointer, allocator->context);
}

int SceNpCommonPs4_38CC9AF5(struct NpCommonPs4Allocator *allocator, void *pointer)
{
	return ((int (*)(void *, void *))allocator->deallocate)(pointer, allocator->context);
}

int NpCommonPs4AllocatorReallocate(struct NpCommonPs4Allocator *allocator, void *pointer, SceSize size)
{
	return ((int (*)(void *, SceSize, void *))allocator->reallocate)(pointer, size, allocator->context);
}
}
