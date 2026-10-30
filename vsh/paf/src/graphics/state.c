/* Native ScePafGraphics globals, field accessors and GXM context forwarding. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/string.h"
#include "../std/new.h"
#include "../thread/mutex.h"
#include <gxm.h>
#include <sce_atomic.h>

extern int sceSharedFbGetShellRenderPort(void);
extern const SceGxmProgramParameter *sceGxmProgramFindParameterBySemantic(
	const SceGxmProgram *program, SceGxmParameterSemantic semantic,
	unsigned int index);
uint32_t ScePafGraphics_29FC820B(uint8_t *self);
uint32_t ScePafGraphics_9FAF51CF(uint8_t *self);

static const SceGxmProgramParameter *GraphicsFindNamedProgramParameter(
		const uint8_t *self, const char *name)
{
	SceGxmShaderPatcherId programId =
		*(SceGxmShaderPatcherId const *)(self + 28);
	const SceGxmProgram *program =
		sceGxmShaderPatcherGetProgramFromId(programId);
	const SceGxmProgramParameter *parameter =
		sceGxmProgramFindParameterByName(program, name);

	if (parameter)
		sceGxmProgramParameterGetCategory(parameter);
	return parameter;
}

const SceGxmProgramParameter *ScePafGraphics_689ED3F2(
		const uint8_t *self, const char *name)
{
	return GraphicsFindNamedProgramParameter(self, name);
}

const SceGxmProgramParameter *ScePafGraphics_AF2AE70A(
		const uint8_t *self, const char *name)
{
	return GraphicsFindNamedProgramParameter(self, name);
}

void *ScePafGraphics_C4ED1731(const uint8_t *self, void *const *source)
{
	void **slot = *(void ***)(self + 112);
	void *previous = *slot;
	void *next = *source;
	void *result = previous;

	if (previous != next) {
		if (previous)
			result = (void *)(uintptr_t)ScePafGraphics_9FAF51CF(previous);
		*slot = next;
		if (next)
			return (void *)(uintptr_t)ScePafGraphics_29FC820B(next);
	}
	return result;
}

void *ScePafGraphics_CA9687DF(const uint8_t *self, void **destination)
{
	void **slot = *(void ***)(self + 112);
	void *previous = *destination;
	void *next = *slot;
	void *result = previous;

	if (previous != next) {
		if (previous)
			result = (void *)(uintptr_t)ScePafGraphics_9FAF51CF(previous);
		*destination = next;
		if (next)
			return (void *)(uintptr_t)ScePafGraphics_29FC820B(next);
	}
	return result;
}

int ScePafGraphics_6EA0C6D7(uint8_t *self, int value)
{
	int result = value % *(int16_t *)(self + 100);

	*(int16_t *)(self + 104) = (int16_t)result;
	return result;
}

static inline __attribute__((always_inline))
uint8_t *DestroyGraphicsOwnedPointer(uint8_t *self)
{
	if (self[4])
		PafDelete(*(void **)self);
	*(void **)self = NULL;
	return self;
}

uint8_t *ScePafGraphics_BEA98E74(uint8_t *self)
{
	return DestroyGraphicsOwnedPointer(self);
}

uint8_t *ScePafGraphics_E5D162EB(uint8_t *self)
{
	return DestroyGraphicsOwnedPointer(self);
}

void *ScePafGraphics_E69349E0(const uint8_t *self)
{
	void **slot = *(void ***)(self + 112);
	void *result = *slot;

	if (result) {
		ScePafGraphics_9FAF51CF(result);
		*slot = NULL;
		return NULL;
	}
	return result;
}

extern uint8_t *sce_paf_graphics_indicator_3070;
uint8_t sce_paf_graphics_reset_flag_3074;

uint8_t *ScePafGraphics_00B9D8CC(void)
{
	uint8_t *result = sce_paf_graphics_indicator_3070;

	if (result) {
		sce_paf_graphics_reset_flag_3074 = 1;
		*result = 0;
	}
	return result;
}

uint8_t sce_paf_graphics_flag_306c;
uint32_t sce_paf_graphics_state_11e0;
uint32_t sce_paf_graphics_state_12ac;
uint32_t sce_paf_graphics_state_10fe4;
uint32_t sce_paf_graphics_state_11fc;
uint32_t sce_paf_graphics_state_12b0;
uint8_t sce_paf_graphics_flag_06e8;
int32_t *sce_paf_graphics_active_surface_2e94;
uint8_t sce_paf_graphics_flag_3518;
uint8_t sce_paf_graphics_flag_136c;
uint8_t sce_paf_graphics_flag_351a;

char *ScePafGraphics_0EC5770E(void)
{
	char *result = (char *)(uintptr_t)sce_paf_graphics_flag_3518;

	if (sce_paf_graphics_flag_3518) {
		sce_paf_graphics_flag_136c = 0;
		sce_paf_graphics_flag_351a = 0;
		return (char *)&sce_paf_graphics_flag_351a;
	}
	return result;
}

int ScePafGraphics_D08F1491(const uint8_t *self, uint64_t *output)
{
	if (*(const uint16_t *)(self + 112) & 0x22)
		return -2136005885;
	*output = *(const uint64_t *)(self + 104);
	return 0;
}

uint32_t *ScePafGraphics_56C831A9(uint8_t *self, const uint32_t *values)
{
	uint32_t count = *(const uint32_t *)(self + 196);
	uint32_t *result = (uint32_t *)(self + 32 * (count - 1));

	result[54] = values[0];
	result[55] = values[1];
	result[56] = values[2];
	result[57] = values[3];
	return result;
}

uint32_t ScePafGraphics_CD5BC4C4(uint8_t *self)
{
	uint32_t result = self[80] & 4;

	if (result) {
		((void (**)(void *))*(void **)self)[18](self);
		result = (uint32_t)((int8_t)self[80] & ~4);
		self[80] = (uint8_t)result;
	}
	return result;
}

__attribute__((noinline))
uint32_t ScePafGraphics_29FC820B(uint8_t *self)
{
	volatile int32_t *count = (volatile int32_t *)(self + 24);

	sceAtomicIncrement32AcqRel(count);
	return (uint32_t)*count;
}

__attribute__((noinline))
uint32_t ScePafGraphics_E57F91FC(uint8_t *self)
{
	volatile int32_t *count = (volatile int32_t *)(self + 24);

	sceAtomicIncrement32AcqRel(count);
	return (uint32_t)*count;
}

__attribute__((noinline))
uint32_t ScePafGraphics_9FAF51CF(uint8_t *self)
{
	volatile int32_t *count = (volatile int32_t *)(self + 24);
	int32_t oldCount = sceAtomicDecrement32AcqRel(count);

	if (oldCount != 1)
		return (uint32_t)*count;
	if (self)
		((void (**)(void *))*(void **)self)[1](self);
	return 0;
}

static inline __attribute__((always_inline))
uint32_t *DestroyGraphicsRefPair(uint32_t *self)
{
	uint32_t second = self[1];

	if (second)
		ScePafGraphics_9FAF51CF((uint8_t *)(uintptr_t)second);
	if (self[0])
		ScePafGraphics_9FAF51CF((uint8_t *)(uintptr_t)self[0]);
	return self;
}

uint32_t *ScePafGraphics_025C548E(uint32_t *self)
{
	return DestroyGraphicsRefPair(self);
}

uint32_t *ScePafGraphics_9E20360C(uint32_t *self)
{
	return DestroyGraphicsRefPair(self);
}

static inline __attribute__((always_inline))
int GraphicsSetSamplerMode(uint8_t *self, uint32_t fieldOffset, int mode)
{
	int mapped = 2;

	if (mode) {
		int selected = 0;
		if (mode != 1) {
			if (mode != 2) {
				mapped = 1;
				if (mode != 4) {
					mapped = 3;
					if (mode != 5) {
						mapped = 4;
						if (mode != 6) {
							mapped = 6;
							if (mode != 7) {
								mapped = 5;
								if (mode != 8) {
									mapped = -1;
									if (mode == 9)
										mapped = 7;
								}
							}
						}
					}
				}
			}
			selected = mapped;
		}
		mapped = selected;
		if (selected == -1)
			return -2136006142;
	}
	if (mapped != *(int32_t *)(self + fieldOffset)) {
		*(int32_t *)(self + fieldOffset) = mapped;
		self[96] = 1;
	}
	return 0;
}

int ScePafGraphics_DFE83DCA(uint8_t *self, int mode)
{
	return GraphicsSetSamplerMode(self, 84, mode);
}

int ScePafGraphics_D193CA56(uint8_t *self, int mode)
{
	return GraphicsSetSamplerMode(self, 88, mode);
}

__attribute__((noinline))
int ScePafGraphics_7BF244E7(uint8_t *self, int mode)
{
	int filter;
	int wrapping;

	switch (mode) {
	case 0:
	case 2:
		filter = 0;
		wrapping = 0;
		break;
	case 1:
	case 3:
		filter = 1;
		wrapping = 0;
		break;
	case 4:
		filter = 0;
		wrapping = 512;
		break;
	case 5:
		filter = 1;
		wrapping = 512;
		break;
	default:
		return -2136006142;
	}
	if (filter != *(int32_t *)(self + 76) ||
	    wrapping != *(int32_t *)(self + 80)) {
		*(int32_t *)(self + 76) = filter;
		*(int32_t *)(self + 80) = wrapping;
		self[96] = 1;
	}
	return 0;
}

__attribute__((noinline))
int ScePafGraphics_5D18583B(uint8_t *self, int mode)
{
	int mapped = 0;

	if (mode)
		mapped = mode == 1 ? 1 : -1;
	if (mapped != *(int32_t *)(self + 72)) {
		if (mapped == -1)
			return -2136006142;
		*(int32_t *)(self + 72) = mapped;
		self[96] = 1;
	}
	return 0;
}

int ScePafGraphics_0F98D1F8(const uint8_t *self, int mode,
			     uint32_t index)
{
	if (index < 4) {
		uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return ScePafGraphics_D193CA56(item, mode);
	}
	return -1;
}

int ScePafGraphics_26D2C953(const uint8_t *self, int mode,
			     uint32_t index)
{
	if (index < 4) {
		uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return ScePafGraphics_DFE83DCA(item, mode);
	}
	return -1;
}

int ScePafGraphics_C5B9145C(const uint8_t *self, int mode,
			     uint32_t index)
{
	if (index < 4) {
		uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return ScePafGraphics_7BF244E7(item, mode);
	}
	return -1;
}

int ScePafGraphics_C85E8878(const uint8_t *self, int mode,
			     uint32_t index)
{
	if (index < 4) {
		uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return ScePafGraphics_5D18583B(item, mode);
	}
	return -1;
}

int ScePafGraphics_96173730(const uint8_t *self)
{
	uint8_t *object = *(uint8_t *const *)(self + 1684);
	int result;

	if (object)
		ScePafGraphics_29FC820B(object);
	result = *(int *)(object + 4);
	if (object)
		ScePafGraphics_9FAF51CF(object);
	return result;
}

int ScePafGraphics_EA29AF79(const uint8_t *self)
{
	int result = 0;

	if (*(const uint32_t *)(self + 368) == 1) {
		result = sceSharedFbGetShellRenderPort();
		if (result < 0)
			return 0;
	}
	return result;
}

const SceGxmProgramParameter *ScePafGraphics_B2C9A58C(
	const uint8_t *self, SceGxmParameterSemantic semantic,
	unsigned int index, uint8_t flag)
{
	SceGxmShaderPatcherId programId =
		*(SceGxmShaderPatcherId const *)(self + 28);
	const SceGxmProgram *program =
		sceGxmShaderPatcherGetProgramFromId(programId);
	const SceGxmProgramParameter *result =
		sceGxmProgramFindParameterBySemantic(program, semantic, index);

	if (!result && !flag)
		return 0;
	return result;
}

int ScePafGraphics_3EA43639(uint8_t *self, float value)
{
	if (value != *(float *)(self + 188)) {
		uint8_t flag = self[80];
		*(float *)(self + 188) = value;
		self[80] = flag | 8;
	}
	return (int)(uintptr_t)self;
}

int ScePafGraphics_6FD7F728(uint8_t *self, float value)
{
	if (value != *(float *)(self + 192)) {
		uint8_t flag = self[80];
		*(float *)(self + 192) = value;
		self[80] = flag | 8;
	}
	return (int)(uintptr_t)self;
}

static inline __attribute__((always_inline))
uint32_t *InitGraphicsRefParameter(uint32_t *result,
				   const uint32_t *source, uint32_t value)
{
	uint32_t object = source[0];

	result[0] = source[0];
	if (object)
		ScePafGraphics_29FC820B((uint8_t *)(uintptr_t)object);
	result[2] = value;
	result[1] = 0;
	return result;
}

uint32_t *ScePafGraphics_3D0309E3(uint32_t *result,
				   const uint32_t *source, uint32_t value)
{
	return InitGraphicsRefParameter(result, source, value);
}

uint32_t *ScePafGraphics_A078C447(uint32_t *result,
				   const uint32_t *source, uint32_t value)
{
	return InitGraphicsRefParameter(result, source, value);
}

int ScePafGraphics_B54C77D3(uint8_t *self, uint8_t enabled)
{
	uint8_t old = self[64];

	self[64] = (uint8_t)((old & 0xFB) | (4 * (enabled & 1)));
	return (old & 4) != 0;
}

int ScePafGraphics_C34E1F2C(uint8_t *self, int32_t mode)
{
	if (mode != 2 && mode != 3)
		return -2136006138;
	*(int32_t *)(self + 368) = mode;
	return 0;
}

int ScePafGraphics_CA0F3549(uint32_t flags)
{
	uint32_t mask = 0x80000000U;
	int index;

	for (index = 31; index != 0; --index) {
		if (flags & mask)
			return index;
		mask >>= 1;
	}
	return 0;
}

uint32_t *ScePafGraphics_FA81A18E(uint32_t *result,
				  uint32_t unused1, uint32_t unused2,
				  uint32_t value)
{
	result[0] = 0x200000;
	result[1] = 0;
	result[5] = 0x100000;
	result[2] = value;
	result[3] = value;
	result[4] = value;
	result[6] = 0;
	result[7] = 0x100000;
	return result;
}

int ScePafGraphics_BFDF8AC1(const uint8_t *self, uint32_t *output)
{
	if (*(const uint16_t *)(self + 112) & 0x11)
		return -2136005885;
	*output = *(const uint32_t *)(self + 128);
	return 0;
}

int ScePafGraphics_D1081027(uint8_t *self, int32_t index,
			     const uint64_t *value)
{
	if (index != 1)
		return -2136005886;
	*(uint64_t *)(self + 8) = *value;
	return 0;
}

__attribute__((noinline)) uint32_t ScePafGraphics_6C448991(void)
{
	return sce_paf_graphics_state_11e0;
}

uint32_t ScePafGraphics_E6EB3D67(void)
{
	uint32_t result = sce_paf_graphics_state_12b0;

	if (!result)
		return *(uint32_t *)(uintptr_t)(ScePafGraphics_6C448991() + 1676);
	return result;
}

static inline __attribute__((always_inline))
void *InitGraphicsHeapSlot(uint8_t *self)
{
	*(void **)self = 0;
	self[4] = 1;
	*(void **)self = PafNew(16);
	return self;
}

void *ScePafGraphics_00AFCFE2(uint8_t *self)
{
	return InitGraphicsHeapSlot(self);
}

void *ScePafGraphics_D46C64C4(uint8_t *self)
{
	return InitGraphicsHeapSlot(self);
}

uint32_t ScePafGraphics_77872076(void)
{
	return sce_paf_graphics_state_12ac;
}

uint32_t ScePafGraphics_CDE30B64(void)
{
	return sce_paf_graphics_state_10fe4;
}

uint32_t ScePafGraphics_52C45E43(uint32_t value)
{
	sce_paf_graphics_flag_306c = value;
	return value;
}

uint32_t ScePafGraphics_D666D983(uint32_t value)
{
	sce_paf_graphics_state_11fc = value;
	return value;
}

uint32_t ScePafGraphics_E4AB21CC(uint32_t value)
{
	sce_paf_graphics_state_12b0 = value;
	return value;
}

static inline __attribute__((always_inline))
uint32_t *InitializeGraphicsState(uint32_t *self)
{
	self[24] = 0;
	self[29] = 0;
	self[31] = 0;
	self[30] = 0;
	return self;
}

uint32_t *ScePafGraphics_13DC10AE(uint32_t *self)
{
	return InitializeGraphicsState(self);
}

uint32_t *ScePafGraphics_A5727A5A(uint32_t *self)
{
	return InitializeGraphicsState(self);
}

int ScePafGraphics_428855D1(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	*first = *(const uint32_t *)(self + 96);
	*second = *(const uint32_t *)(self + 100);
	return 0;
}

void *ScePafGraphics_AEBC68E5(const uint8_t *self, const uint64_t *value)
{
	uint8_t *target = *(uint8_t *const *)(self + 80);

	*(uint64_t *)(target + 48) = *value;
	return target;
}

void ScePafGraphics_6E7B77AC(const uint8_t *self, float value)
{
	sceGxmSetWClampValue(*(SceGxmContext *const *)(self + 1676), value);
}

void ScePafGraphics_E4B3B1D2(const uint8_t *self)
{
	sceGxmFinish(*(SceGxmContext *const *)(self + 1676));
}

int32_t ScePafGraphics_F3CD22A0(void)
{
	return -2127491096;
}

int ScePafGraphics_1AD606BC(uint8_t *self, uint16_t bits)
{
	*(uint16_t *)(self + 112) |= bits;
	return 0;
}

int ScePafGraphics_74E0DEAA(uint8_t *self, uint16_t bits)
{
	*(uint16_t *)(self + 112) &= (uint16_t)~bits;
	return 0;
}

int32_t ScePafGraphics_A9D60F9B(const uint8_t *self, uint32_t index)
{
	if (index < 4) {
		const uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return *(const int32_t *)(item + 84);
	}
	return -1;
}

int32_t ScePafGraphics_D9C76401(const uint8_t *self, uint32_t index)
{
	if (index < 4) {
		const uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return *(const int32_t *)(item + 88);
	}
	return -1;
}

int ScePafGraphics_AB29233A(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	const uint8_t *item = *(uint8_t *const *)(self + 16);
	uint32_t value = 0;
	if (item)
		value = *(const uint32_t *)(item + 4);
	*first = value;
	*second = (uint32_t)(uintptr_t)item;
	return 0;
}

int32_t ScePafGraphics_F5D1E17D(const uint8_t *self, int32_t shiftCount)
{
	int32_t result = *(const int32_t *)(self + 60);
	int32_t iteration;

	for (iteration = shiftCount - 1; ; --iteration) {
		int32_t oldCount = shiftCount;
		shiftCount = iteration;
		if (oldCount <= 0)
			break;
		if (result > 1)
			result >>= 1;
	}
	return result;
}

int32_t ScePafGraphics_EF7B7032(const uint8_t *self,
				 int32_t unused, int32_t shiftCount)
{
	int32_t result = *(const int16_t *)(self + 30);
	int32_t iteration;

	for (iteration = shiftCount - 1; ; --iteration) {
		int32_t oldCount = shiftCount;
		shiftCount = iteration;
		if (oldCount <= 0)
			break;
		if (result > 1)
			result >>= 1;
	}
	return result;
}

int ScePafGraphics_111C68BE(const uint8_t *self, uint64_t *output)
{
	const uint8_t *item = *(uint8_t *const *)(self + 136);
	uint64_t value = 0;

	if (item)
		value = *(const uint64_t *)(item + 16);
	*output = value;
	return (int)(uintptr_t)item;
}

int ScePafGraphics_89425A43(uint8_t *self, uint8_t enabled)
{
	uint8_t old = self[64];

	self[64] = (uint8_t)((old & 0xF7) | (8 * (enabled & 1)));
	return (old & 8) != 0;
}

int ScePafGraphics_A1BB7F6C(const uint8_t *self,
			    int32_t index, int32_t *output)
{
	if (index)
		return -2136005886;
	*output = (int8_t)self[4];
	return 0;
}

int ScePafGraphics_C16D98B4(uint8_t *self)
{
	RMutex *mutex = *(RMutex **)(self + 140);

	if (mutex) {
		ScePafThread_F9FFA0BE(mutex);
		mutex = *(RMutex **)(self + 140);
	}
	*(uint16_t *)(self + 112) |= 7;
	if (mutex)
		ScePafThread_95F9C45F(mutex);
	return 7;
}

int32_t ScePafGraphics_22AB24A0(const uint8_t *self, int32_t value)
{
	return ((value << *(const uint32_t *)(self + 12)) + 7) >> 3;
}

int32_t ScePafGraphics_3B5327DF(const uint8_t *self)
{
	return *(const int32_t *)(self + 56) *
		(3 - *(const int32_t *)(self + 32));
}

uint8_t *ScePafGraphics_4C4593FF(void)
{
	sce_paf_graphics_flag_06e8 = 0;
	return &sce_paf_graphics_flag_06e8;
}

uint8_t *ScePafGraphics_A05227FA(void)
{
	sce_paf_graphics_flag_06e8 = 1;
	return &sce_paf_graphics_flag_06e8;
}

int32_t *ScePafGraphics_8624501D(int32_t *result)
{
	*result = -2127676444;
	return result;
}

int32_t *ScePafGraphics_AAB453CB(int32_t *result)
{
	*result = -2127676444;
	return result;
}

void *ScePafGraphics_AE20B115(void *self, void *destination,
			      const void *source, uint32_t count)
{
	return sce_paf_memcpy(destination, source, count);
}

int32_t ScePafGraphics_BEF37718(void)
{
	return *sce_paf_graphics_active_surface_2e94;
}

int ScePafGraphics_E61D19A9(const uint8_t *self,
			     uint32_t *first, uint32_t *second)
{
	*first = *(const uint32_t *)(self + 4);
	*second = *(const uint32_t *)(self + 8);
	return 0;
}

int32_t *sce_paf_graphics_buffer_11e4;
uint8_t *sce_paf_graphics_display_06f8;

uint8_t ScePafGraphics_505CD74E(uint8_t value)
{
	sce_paf_graphics_display_06f8[1432] = value;
	return value;
}

uint32_t *ScePafGraphics_57664903(const void *self, uint32_t index)
{
	return sce_paf_graphics_buffer_11e4 + index;
}

int ScePafGraphics_59EC6C24(const uint8_t *self, PafWString *result)
{
	PafWStringCopy(result, (const PafWString *)(self + 4));
	return 0;
}

int32_t ScePafGraphics_59FF472E(const uint8_t *self)
{
	return ((*(const int16_t *)(self + 28) <<
		  *(const uint32_t *)(self + 12)) + 7) >> 3;
}

uint8_t *ScePafGraphics_5B6A4EA2(const uint8_t *self, uint32_t index)
{
	return *(uint8_t *const *)(self + 148) + index * 48;
}

uint32_t *ScePafGraphics_7E87FE3F(uint32_t *self, uint32_t value)
{
	self[2] = value;
	self[0] = 0;
	self[1] = 0;
	self[3] = (uint32_t)-2;
	return self;
}

int ScePafGraphics_83ABDFF5(const uint8_t *self, uint8_t argument)
{
	void *object = *(void *const *)(self + 4);

	if (object)
		return ((int (**)(void *, uint8_t))*(void **)object)[7](
			object, argument);
	return 0;
}

uint8_t *ScePafGraphics_87657577(const uint8_t *self, int32_t index)
{
	uint8_t *base = *(uint8_t *const *)(self + 108);

	if (base)
		return base + *(const int32_t *)(self + 56) * index;
	return 0;
}

uint32_t *ScePafGraphics_E79F111A(uint32_t *self, uint32_t value)
{
	self[2] = value;
	self[0] = 0;
	self[1] = 0;
	self[3] = (uint32_t)-2;
	return self;
}

static inline __attribute__((always_inline))
uint32_t *InitializeGraphicsFlags(uint32_t *self)
{
	self[0] = 0;
	self[1] = 0;
	self[5] = 0x100000;
	self[7] = 0;
	self[6] = 0;
	return self;
}

uint32_t *ScePafGraphics_212605A1(uint32_t *self)
{
	return InitializeGraphicsFlags(self);
}

uint32_t *ScePafGraphics_2F46C2B9(uint32_t *self)
{
	return InitializeGraphicsFlags(self);
}

int32_t ScePafGraphics_133EC137(void *self, SceGxmTexture **texture,
			       void *palette)
{
	int32_t result = sceGxmTextureSetPalette(*texture, palette);
	return result >= 0 ? 0 : result;
}

int ScePafGraphics_D53DC736(const uint8_t *self)
{
	void *object = *(void *const *)(self + 4);

	if (object)
		return ((int (**)(void *))*(void **)object)[4](object);
	return 0;
}

int ScePafGraphics_B000FA14(const uint8_t *self)
{
	int result = *(const int16_t *)(self + 102) - 1;
	if (result < 0)
		return *(const int16_t *)(self + 100) - 1;
	return result;
}

static inline __attribute__((always_inline))
uint32_t *InitializeGraphicsZeros(uint32_t *self)
{
	self[0] = 0;
	self[1] = 0;
	self[2] = 0;
	self[3] = 0;
	self[4] = 0;
	self[5] = 0;
	self[7] = 0;
	self[6] = 0;
	return self;
}

uint32_t *ScePafGraphics_19163245(uint32_t *self)
{
	return InitializeGraphicsZeros(self);
}

uint32_t *ScePafGraphics_BF863FEE(uint32_t *self)
{
	return InitializeGraphicsZeros(self);
}

int ScePafGraphics_404AC3D7(const uint8_t *self)
{
	void *object = *(void *const *)(self + 4);

	if (object)
		return ((int (**)(void *))*(void **)object)[8](object);
	return 1;
}

static inline __attribute__((always_inline))
int GraphicsInvokeOffset20(const uint32_t *self)
{
	void *object = (void *)(uintptr_t)self[0];
	return ((int (**)(void *, uint32_t, uint32_t, uint32_t))
		*(void **)object)[5](object, self[1], self[2], self[3]);
}

int ScePafGraphics_4423922B(const uint32_t *self)
{
	return GraphicsInvokeOffset20(self);
}

int ScePafGraphics_7C402472(const uint32_t *self)
{
	return GraphicsInvokeOffset20(self);
}

void ScePafGraphics_722A4987(uint8_t *self, SceGxmCullMode mode)
{
	SceGxmContext *context =
		*(SceGxmContext *const *)(self + 1676);
	*(SceGxmCullMode *)(self + 1896) = mode;
	sceGxmSetCullMode(context, mode);
}

__attribute__((noinline))
int ScePafGraphics_8D25DDE9(const void *self, const char *tag)
{
	return tag == "DrawObj";
}

int ScePafGraphics_D3A10EAF(const void *self, const char *tag)
{
	if (tag == "BubbleObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

uint8_t *sce_paf_graphics_indicator_3070;

int ScePafGraphics_BD1F2F7D(uint8_t *self, uint32_t value)
{
	if (value > 0x3f)
		return -1;
	*(uint32_t *)(self + 92) = value;
	return 0;
}

int ScePafGraphics_DD2002B4(void)
{
	if (sce_paf_graphics_indicator_3070)
		return *sce_paf_graphics_indicator_3070;
	return 0;
}

void ScePafGraphics_EA96F738(int32_t *self)
{
	*self = -2127676444;
	PafDelete(self);
}

int ScePafGraphics_FBC9302A(const uint32_t *self)
{
	return GraphicsInvokeOffset20(self);
}

uint32_t *ScePafGraphics_570E7907(uint32_t *self, uint32_t second,
				    uint32_t third, uint32_t fourth,
				    uint32_t fifth)
{
	self[88] = third;
	self[89] = second;
	self[90] = fourth;
	self[91] = fifth;
	return self;
}

int ScePafGraphics_0FD022D5(uint8_t *self, int mode, uint8_t value)
{
	if (mode != 0)
		return -2136005886;
	self[4] = value;
	return 0;
}

static inline __attribute__((always_inline))
int GraphicsIndexedValue(const uint8_t *self, uint32_t index)
{
	if (index < 4) {
		uint8_t *item = *(uint8_t *const *)(self + 196 + 4 * index);
		if (item)
			return *(const int32_t *)(item + 72);
	}
	return -1;
}

int ScePafGraphics_1D53BDC2(const uint8_t *self, uint32_t index)
{
	return GraphicsIndexedValue(self, index);
}

int ScePafGraphics_9DCF66E4(const uint8_t *self, uint32_t index)
{
	return GraphicsIndexedValue(self, index);
}

int ScePafGraphics_5FDB925A(const void *self, const char *tag)
{
	if (tag == "PlaneObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

int ScePafGraphics_88561C1B(const void *self, const char *tag)
{
	if (tag == "PanelObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

int ScePafGraphics_8D9E23C2(const void *self, const char *tag)
{
	if (tag == "PageTurnObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

int ScePafGraphics_930F72BF(const void *self, const char *tag)
{
	if (tag == "TextObj")
		return 1;
	return ScePafGraphics_8D25DDE9(self, tag);
}

void *ScePafGraphics_0C968320(const uint8_t *self, const uint64_t *data)
{
	uint64_t second = data[1];
	uint8_t *target = *(uint8_t *const *)(self + 80);
	*(uint64_t *)(target + 92) = data[0];
	*(uint64_t *)(target + 100) = second;
	return target;
}
