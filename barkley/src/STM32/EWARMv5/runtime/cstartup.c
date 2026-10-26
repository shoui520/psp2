/*
 * The start-up (IAR's __cmain): the data initialised (unless __low_level_init says
 * not to) from the linker's table of initialisers, the static constructors run, then
 * main, and exit with its result.
 */

#include <stddef.h>

int __low_level_init(void);
int main(int argc);
void exit(int status);

/* the linker's: the initialisers (each a function and its arguments), and the static
   constructors (each an offset from its own address) */
extern const unsigned long regionTableBase[] __asm__("Region$$Table$$Base");
extern const unsigned long regionTableLimit[] __asm__("Region$$Table$$Limit");
extern const long initArrayBase[] __asm__("SHT$$INIT_ARRAY$$Base");
extern const long initArrayLimit[] __asm__("SHT$$INIT_ARRAY$$Limit");

typedef const unsigned long *(*Initializer)(const unsigned long *args);

/* size bytes at dst zeroed, for each (size, dst) to a size of 0 */
const unsigned long *__iar_zero_init2(const unsigned long *p)
{
	unsigned long size;
	unsigned long *dst;

	while ((size = *p++) != 0) {
		dst = (unsigned long *)*p++;
		do {
			*dst++ = 0;
			size -= 4;
		} while (size != 0);
	}
	return p;
}

/* the destination ranges filled from the source ranges, PackBits-compressed: a count n,
   then n + 1 bytes copied (n >= 0) or one byte repeated -n times (n < 0) */
const unsigned long *__iar_packbits_init2(const unsigned long *p)
{
	const unsigned long *src = p + 2;
	const unsigned long *dst = src + 2 * p[0];
	const unsigned long *end = dst + 2 * p[1];
	const unsigned char *in = 0, *inEnd = 0;
	unsigned char *d, *dEnd;
	unsigned char c;
	short count = 0;
	int needCount = 1;

	while (dst != end) {
		d = (unsigned char *)*dst++;
		dEnd = d + *dst++;
		while (d != dEnd) {
			while (in == inEnd) {
				in = (const unsigned char *)*src++;
				inEnd = in + *src++;
			}
			if (needCount) {
				count = *(const signed char *)in++;
				needCount = 0;
				while (in == inEnd) {
					in = (const unsigned char *)*src++;
					inEnd = in + *src++;
				}
			}
			if (count < 0) {
				c = *in++;
				while (d != dEnd) {
					*d++ = c;
					if (++count >= 0) {
						needCount = 1;
						break;
					}
				}
			} else {
				while (d != dEnd && in != inEnd) {
					*d++ = *in++;
					if (--count < 0) {
						needCount = 1;
						break;
					}
				}
			}
		}
	}
	return end;
}

__attribute__((noinline)) void __iar_data_init2(void)
{
	const unsigned long *p = regionTableBase;

	while (p != regionTableLimit) {
		Initializer init = (Initializer)*p++;

		p = init(p);
	}
}

__attribute__((noinline)) void __call_ctors(const long *begin, const long *end)
{
	const long *p;

	for (p = begin; p != end; p++)
		((void (*)(void))((const char *)p + *p))();
}

void __iar_program_start(void)
{
	if (__low_level_init() != 0)
		__iar_data_init2();
	__call_ctors(initArrayBase, initArrayLimit);
	exit(main(0));
}

/* each element of an array constructed, in order */
void *__aeabi_vec_ctor_nocookie_nodtor(void *array, void *(*ctor)(void *), size_t size,
				      size_t count)
{
	char *p = array;

	if (ctor != 0)
		for (; count != 0; count--, p += size)
			ctor(p);
	return array;
}
