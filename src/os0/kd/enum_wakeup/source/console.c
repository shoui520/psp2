/* SceEnumWakeUp: The text console (set up, never used). */
#include "ew_internal.h"

int ewConsoleInit(void)
{
	int i;

	ewg.console.columns = ewg.console.width >> 3;
	ewg.console.rows = ewg.console.height >> 4;
	ewg.console.font = ewFont8x16;
	ewg.console.glyphWidth = 8;
	ewg.console.reserved0 = 0;
	ewg.console.glyphHeight = 16;
	ewg.console.glyphBytes = 16;
	memset(ewg.console.colors, 0, sizeof(ewg.console.colors));
	ewg.console.reserved2 = 0;
	ewg.console.x = 0;
	ewg.console.y = 0;
	ewg.console.colors[1] = -1;
	ewg.console.scale = 2;
	ewg.console.visible = 1;
	for (i = 0; i < 0x1540; i += 0xa0) {
		memset(&ewg.text[0][i], 0, 0xa0);
		memset(&ewg.text[1][i], 1, 0xa0);
	}
	return 0;
}
