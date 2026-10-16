/* Common translation inlined into the original MagicGate consumer routines. */
#pragma once

#define TRANSLATE_MAGICGATE_RESULT(result) do { \
	if ((result) == (int)0x80505001) { \
		(result) = (int)0x8050c021; \
	} else if ((result) <= (int)0x80505001) { \
		if ((result) == (int)0x80503001) \
			(result) = (int)0x8050c032; \
		else if ((result) > (int)0x80503001) { \
			if ((result) == (int)0x80503002) \
				(result) = (int)0x8050c012; \
			else if ((unsigned int)(result) - 0x80504000 <= 2) \
				(result) = (int)0x8050c005; \
		} else if ((unsigned int)(result) - 0x80502000 <= 2) { \
			(result) = (int)0x8050c005; \
		} \
	} else if ((result) == (int)0x80505005) { \
		(result) = (int)0x8050c012; \
	} else if ((result) > (int)0x80505005) { \
		if ((result) == (int)0x80505007) \
			(result) = (int)0x8050c026; \
		else if ((result) < (int)0x80505007) \
			(result) = (int)0x8050c025; \
		else if ((result) == (int)0x80505009) \
			(result) = (int)0x8050c027; \
	} else if ((result) == (int)0x80505003) { \
		(result) = (int)0x8050c024; \
	} else if ((result) <= (int)0x80505003) { \
		(result) = (int)0x8050c022; \
	} else { \
		(result) = (int)0x8050c023; \
	} \
} while (0)
