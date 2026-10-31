/*
 * paf::system: what the console is. Capability holds the model's capabilities
 * (SceVshBridge's model capability bits), Model whether it is a Vita or a
 * Dolce (PS Vita TV), SystemParam the text style and the system parameters'
 * flags. Each is one object made when libpaf is loaded.
 */
#ifndef PAF_SYSTEM_SYSTEM_H
#define PAF_SYSTEM_SYSTEM_H

#include <stdint.h>
#include <scetypes.h>

extern "C" {
int sceAppMgrIsGameProgram(void);
int sceAppMgrIsNonGameProgram(void);
int vshKernelCheckModelCapability(int capability);
int sceKernelGetModelForCDialog(void);
int sceRegMgrSystemParamGetInt(int id, int *value);
}

namespace paf {
namespace system {

class Capability {
public:
	enum Flag {
		Flag_WirelessController = 0x01,
		Flag_Camera = 0x02,
		Flag_B7CFFF5C = 0x04,
		Flag_OLED = 0x08,
		Flag_LCD = 0x10,
		Flag_HID = 0x20,
		Flag_7ED8F03C = 0x40,
		/* what a game is told it has, without asking the kernel */
		Flag_Game = Flag_Camera | Flag_OLED | Flag_7ED8F03C
	};

	Capability()
	{
		flags = Flag_Game;
		if (sceAppMgrIsGameProgram() == 0 && sceAppMgrIsNonGameProgram() == 0) {
			flags = 0;
			if (vshKernelCheckModelCapability(1))
				flags |= Flag_WirelessController;
			if (vshKernelCheckModelCapability(2))
				flags |= Flag_Camera;
			if (vshKernelCheckModelCapability(5) || vshKernelCheckModelCapability(6))
				flags |= Flag_B7CFFF5C;
			if (vshKernelCheckModelCapability(7))
				flags |= Flag_OLED;
			if (vshKernelCheckModelCapability(8))
				flags |= Flag_LCD;
			if (vshKernelCheckModelCapability(9))
				flags |= Flag_HID;
			if (vshKernelCheckModelCapability(10))
				flags |= Flag_7ED8F03C;
		}
	}

	virtual ~Capability()
	{
	}

	static bool HasWirelessController();
	static bool HasCamera();
	static bool HasOLED();
	static bool HasLCD();
	static bool HasHID();
	static bool HasFrontTouch();
	static bool HasBackTouch();
	static bool HasHDMI();

	char flags;
};

class Model {
public:
	enum Type {
		Type_Vita = 0,
		Type_Dolce = 1
	};

	Model()
	{
		type = Type_Vita;
		if ((sceKernelGetModelForCDialog() & MODEL_DOLCE) != 0)
			type = Type_Dolce;
	}

	virtual ~Model()
	{
	}

	static int32_t GetType();

	int32_t type;

private:
	/* sceKernelGetModelForCDialog's bit for the Dolce */
	enum { MODEL_DOLCE = 0x20000 };
};

class SystemParam {
public:
	enum {
		/* the system parameters read, each a flag when it is 1 */
		ID_23FC2 = 147394,
		ID_611DC9 = 6364617,
		ID_8A2AD7 = 9054935,
		/* the text style before the Framework sets one */
		TEXT_STYLE_DEFAULT = 6
	};

	SystemParam()
	{
		text_style = TEXT_STYLE_DEFAULT;
		param_flags = ReadParamFlags();
	}

	virtual ~SystemParam()
	{
	}

	static void SetTextStyle(char style);
	static char GetTextStyle();
	static void SetParamFlags(char flags);
	static int32_t ReadParamFlags();

	char text_style;
	char param_flags;
};

}
}

/* Capability and Model exports no Sony name found for (their NIDs' hashes searched). */
extern "C" {
bool ScePafMisc_B7CFFF5C(void);
bool ScePafMisc_7ED8F03C(void);
bool ScePafMisc_183E674B(void);
int32_t ScePafMisc_C51EF81D(void);
}

#endif /* PAF_SYSTEM_SYSTEM_H */
