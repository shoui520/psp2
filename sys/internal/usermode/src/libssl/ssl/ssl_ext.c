/* Native extension ownership. Filename and helper name are ours. */
#include <skeleton.h>
#include "ssl_ext.h"
#include "../memory.h"

void sslExtensionFree(SslExtension *extension)
{
	if (extension == NULL)
		return;
	if (extension->length != 0) {
		if (extension->data != NULL) {
			sslFree(extension->data);
			extension->data = NULL;
		}
		extension->length = 0;
	}
	sslFree(extension);
}

STUB(SceSslInternal_00ABEBC3)
STUB(SceSslInternal_05F6A292)
STUB(SceSslInternal_0C693D36)
STUB(SceSslInternal_0DB6C206)
STUB(SceSslInternal_133ED36D)
STUB(SceSslInternal_19B2BB58)
STUB(SceSslInternal_243DE290)
STUB(SceSslInternal_24C6257F)
STUB(SceSslInternal_260F5A85)
STUB(SceSslInternal_26CBEEB6)
STUB(SceSslInternal_2B90012E)
STUB(SceSslInternal_4A1557A7)
STUB(SceSslInternal_6046C1F3)
STUB(SceSslInternal_67FB1CD3)
STUB(SceSslInternal_6D0D3B11)
STUB(SceSslInternal_7D30F0A6)
STUB(SceSslInternal_9E38815D)
STUB(SceSslInternal_A374CD7B)
STUB(SceSslInternal_A4EA9DE0)
STUB(SceSslInternal_AC1DBE40)
STUB(SceSslInternal_AF50B945)
STUB(SceSslInternal_B0E4CB2F)
STUB(SceSslInternal_CBEC7D13)
STUB(SceSslInternal_CC7F4211)
STUB(SceSslInternal_D28834E6)
STUB(SceSslInternal_D419451B)
STUB(SceSslInternal_DEC6DD27)
STUB(SceSslInternal_E746465E)
STUB(SceSslInternal_EF9EBAC7)
STUB(SceSslInternal_F2C9B744)
STUB(SceSslInternal_F4007DBE)
STUB(SceSslInternal_F71DB06F)
STUB(SceSslInternal_FB51AE89)
STUB(SceSslInternal_FBEB1FD7)
STUB(SceSslInternal_FEFE761C)
