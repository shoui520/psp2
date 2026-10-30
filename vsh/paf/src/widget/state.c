/* Standalone native widget state constructors and field operations. */
#include <stdint.h>

static inline __attribute__((always_inline))
uint32_t *InitializeWidgetRecord(uint32_t *self, uint32_t payload)
{
	self[4] = payload;
	self[0] = 0;
	self[1] = 0;
	self[2] = 0;
	return self;
}

uint32_t *ScePafWidget_348741A7(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_2C5E7C87(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_E0F8DBA7(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_E2F44822(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_E983D809(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_647784E1(uint32_t *self)
{
	self[0] = UINT32_MAX;
	self[3] = 0;
	return self;
}

uint32_t *ScePafWidget_688E163E(uint32_t *self)
{
	self[0] = UINT32_MAX;
	self[3] = 0;
	return self;
}

uint8_t *ScePafWidget_434B1CB8(uint8_t *self)
{
	++*(uint32_t *)(self + 1056);
	return self;
}

uint8_t *ScePafWidget_4A3F4C93(uint8_t *self)
{
	++*(uint32_t *)(self + 924);
	return self;
}

int ScePafWidget_9A7BE3B0(uint8_t *self, uint32_t value, uint8_t flag)
{
	self[205] = flag;
	*(uint32_t *)(self + 184) = value;
	return 0;
}

uint8_t *ScePafWidget_1F9DB221(uint8_t *self)
{
	if (self[640])
		self[640] = 0;
	return self;
}

int ScePafWidget_E8F862FC(void *self, const void *value)
{
	return value ? 0 : -1;
}

/* Shared PAF-widget registration and state slots. */
uint32_t sce_paf_widget_state_199c;
uint32_t sce_paf_widget_state_19a8;
uint32_t sce_paf_widget_state_19ac;
uint32_t sce_paf_widget_state_19b0;
uint32_t sce_paf_widget_state_19b4;
uint32_t sce_paf_widget_state_19b8;
uint32_t sce_paf_widget_state_19bc;
uint32_t sce_paf_widget_state_19c0;
uint32_t sce_paf_widget_state_1ce8;
uint32_t sce_paf_widget_state_142d4;
uint32_t sce_paf_widget_state_142d8;

uint32_t ScePafWidget_1EE9D37E(void)
{
	return sce_paf_widget_state_19a8;
}

uint32_t ScePafWidget_BFF4C5A2(void)
{
	return sce_paf_widget_state_19ac;
}

uint32_t ScePafWidget_4AAE618E(void)
{
	return sce_paf_widget_state_19b0;
}

uint32_t ScePafWidget_74F3EF0E(void)
{
	return sce_paf_widget_state_19b4;
}

uint32_t ScePafWidget_A55215E5(void)
{
	return sce_paf_widget_state_19b8;
}

uint32_t ScePafWidget_02E8CF46(void)
{
	return sce_paf_widget_state_19bc;
}

uint32_t ScePafWidget_E638AA1F(void)
{
	return sce_paf_widget_state_19c0;
}

uint32_t ScePafWidget_03EC4830(void)
{
	return sce_paf_widget_state_1ce8;
}

uint32_t ScePafWidget_66EEB261(uint32_t value)
{
	sce_paf_widget_state_1ce8 = value;
	return value;
}

uint32_t ScePafWidget_887EF5E5(uint32_t value)
{
	sce_paf_widget_state_199c = value;
	return value;
}

uint32_t *ScePafWidget_88BB79F0(void)
{
	sce_paf_widget_state_142d4 = 0;
	return &sce_paf_widget_state_142d4;
}

uint32_t *ScePafWidget_37FBE9CE(void)
{
	sce_paf_widget_state_142d4 = 1;
	return &sce_paf_widget_state_142d4;
}

int ScePafWidget_59CA8E05(void)
{
	return sce_paf_widget_state_142d4 != 0;
}

int ScePafWidget_295F20D1(void)
{
	return sce_paf_widget_state_142d8 != 0;
}
