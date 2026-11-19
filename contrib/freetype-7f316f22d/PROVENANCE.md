# FreeType 2, git 7f316f22d

Upstream: git://git.sv.nongnu.org/freetype/freetype2.git, commit
7f316f22d1222484af9f7f62face6cb94be777b2 (2012-08-31, "[autofit] Implement properties
service framework"), between 2.4.10 and 2.4.11. Pinned by compiling releases and snapshots
against libSceFt2 and safemode: af_property_get matches only at this commit.

Kept, unchanged (`git archive 7f316f22d include docs/FTL.TXT docs/LICENSE.TXT docs/GPLv2.TXT`):
the public headers (`include/`) and the license files. The library sources are added from the
same commit when libSceFt2 is built from here.

Used under the FreeType License (`docs/FTL.TXT`).

Sources (2026-11-19): `src/{autofit,base,cid,psaux,pshinter,psnames,raster,sfnt,smooth,truetype,type1,type42}` from the same commit (Jamfile, rules.mk and module.mk left out). Not yet built by any module; libSceFt2's build will pick the files Sony's module contains.
