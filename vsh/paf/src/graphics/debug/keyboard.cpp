/*
 * paf::gutil::debug::Keyboard: the keyboard of a devkit's host, its 256 keys
 * counted a frame at a time. This build reads no keys.
 */
#include <graphics/gutil.h>

using namespace paf::gutil::debug;

Keyboard::Keyboard(int32_t param)
{
	for (int32_t i = 0; i < 256; i++)
		m_keys[i].Clear();
	m_param = param;
	for (int32_t i = 0; i < 32; i++)
		m_inputs[i].Clear();
}

void Keyboard::Read()
{
}

void Keyboard::Update()
{
	for (int32_t i = 0; i != 256; i++)
		m_keys[i].Update();
}

static Keyboard s_keyboard(0);

Keyboard *Keyboard::GetInstance()
{
	return &s_keyboard;
}
