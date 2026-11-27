# FreeType 2.4.3

Upstream: git://git.sv.nongnu.org/freetype/freetype2.git, tag VER-2-4-3, commit
f7e920af84b3ddcf3bf1f88e0fc8e6713c20945f (2010-10-03, "Version 2.4.3 released").
Pinned from pup:psp2swu.self itself: its FT_New_Library stores version 2.4.3, its
FT_ModuleRec retains FT_Generic (20 bytes total, placing AF_LoaderRec at offset +20 in
af_autofitter_init), and its sfnt/sfobjs.c includes tt_name_entry_ascii_from_utf16.

Kept, unchanged (`git archive VER-2-4-3`): the public headers (`include/`), the license
files (`docs/FTL.TXT`, `docs/LICENSE.TXT`, `docs/GPL.TXT`) and the sources of the modules
psp2swu's statically linked libSceFt2 contains,
`src/{autofit,base,cff,psaux,pshinter,psnames,raster,sfnt,smooth,truetype,type1}`.
Left out: each module's Jamfile, rules.mk and module.mk, and every other part of the release.

Sony's changes are not applied here: psp2swu's `freetype-2.4.3-sce.patch` patches a copy
in the build tree, and libSceFt2's `include/freetype/config/` overrides the options Sony
changed.

Used under the FreeType License (`docs/FTL.TXT`).
