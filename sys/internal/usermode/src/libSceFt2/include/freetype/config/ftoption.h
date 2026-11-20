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

/*
 * SCE: the latin2 auto-hinter is built in. Sony's af_script_classes has five entries
 * (dummy, latin2, latin, cjk, indic), and af_autofitter_load_glyph takes load flag bit 20
 * as the request for it.
 */
#define FT_OPTION_AUTOFIT2
