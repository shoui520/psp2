/*
 * FreeType's build options as Sony set them for libSceFt2: upstream's ftoption.h, then
 * Sony's changes.
 */
#include_next <freetype/config/ftoption.h>

/*
 * SCE: no Macintosh font support. Sony's binary has none of ftobjs.c's resource-fork
 * loaders (IsMacResource, load_mac_face) and its FT_Raccess_Guess only reports every
 * guess unimplemented.
 */
#undef FT_CONFIG_OPTION_MAC_FONTS
#undef FT_CONFIG_OPTION_GUESSING_EMBEDDED_RFORK
