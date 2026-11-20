# FreeType 2.4.9

Upstream: git://git.sv.nongnu.org/freetype/freetype2.git, tag VER-2-4-9, commit
50ed33e53712b45508b4f3802d715450577e1dda (2012-03-08, "Version 2.4.9 released").
Pinned from libSceFt2 itself: its FT_New_Library stores version 2.4.9, and its raster's
Line_Up still calls FT_MulDiv where the 2012-06-06 change (495bd3cc) calls
FT_MulDiv_No_Round.

Kept, unchanged (`git archive VER-2-4-9`): the public headers (`include/`), the license
files (`docs/FTL.TXT`, `docs/LICENSE.TXT`, `docs/GPLv2.TXT`) and the sources of the modules
libSceFt2 contains, `src/{autofit,base,cff,otvalid,psaux,pshinter,psnames,raster,sfnt,smooth,truetype,type1}`.
Left out: each module's Jamfile, rules.mk and module.mk, and every other part of the release.

Sony's changes are not applied here: libSceFt2's `freetype-sce.patch` patches a copy in the
build tree, and its `include/freetype/config/` overrides the options Sony changed.

Used under the FreeType License (`docs/FTL.TXT`).
